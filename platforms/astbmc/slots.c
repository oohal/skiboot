/* Copyright 2015-2017 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdint.h>

#include <skiboot.h>
#include <device.h>
#include <console.h>
#include <chip.h>
#include <pci-cfg.h>
#include <pci.h>
#include <pci-slot.h>

#include "astbmc.h"

#undef pr_fmt
#define pr_fmt(fmt) "SLOTMAP: " fmt

#define PHB_NR(loc) ((loc) & 0xffff)
#define PHB_CHIP(loc) (((loc) >> 16) & 0xffff)

static bool is_npu_phb(const struct slot_table_entry *phb)
{
	return phb->etype == st_phb && phb->children[0].etype == st_npu_slot;
}

static void slot_trace(struct dt_node *n)
{
	char *c = dt_get_path(n);
	prlog(PR_NOTICE, "SLOTS: Added %s\n", c);
	free(c);
}

static bool parse_one_slot(struct dt_node *parent_node,
		const struct slot_table_entry *entry);

static void parse_switch(struct dt_node *parent_node,
		const struct slot_table_entry *sw_entry)
{
	const struct slot_table_entry *child;
	struct dt_node *node, *sw_node;

	sw_node = dt_new(parent_node, "switch");
	slot_trace(sw_node);

	if (sw_entry->name)
		dt_add_property_string(sw_node, "label", sw_entry->name);

	dt_add_property_cells(sw_node, "upstream-port", 0);
	dt_add_property_cells(sw_node, "#address-cells", 1);
	dt_add_property_cells(sw_node, "#size-cells", 0);

	for (child = sw_entry->children; child->etype != st_end; child++) {
		/* port address is the device number i.e devfn minus the fn */
		node = dt_new_addr(sw_node, "down-port",
			child->location >> 3);
		slot_trace(node);

		dt_add_property_cells(node, "reg", child->location >> 3);
		dt_add_property_cells(node, "#address-cells", 2);
		dt_add_property_cells(node, "#size-cells", 0);

		if (child->name)
			dt_add_property_string(node, "label", child->name);

		parse_one_slot(node, child);
	}
}

static bool parse_one_slot(struct dt_node *parent_node,
		const struct slot_table_entry *entry)
{
	bool children = entry->children != NULL;
	const struct slot_table_entry *child;
	struct dt_node *node = NULL;

	switch(entry->etype) {
	case st_phb:
		if (is_npu_phb(entry))
			return false;

		node = dt_new_2addr(parent_node, "root-complex",
			PHB_CHIP(entry->location), PHB_NR(entry->location));
		slot_trace(node);

		dt_add_property_cells(node, "#address-cells", 2);
		dt_add_property_cells(node, "#size-cells", 0);
		dt_add_property_cells(node, "reg",
			PHB_CHIP(entry->location), PHB_NR(entry->location));
		dt_add_property_string(node, "compatible",
				"ibm,pcie-root-port");
		break;

	case st_npu_slot: /* ignore npu slots since they're not really slots */
		return false;

	case st_builtin_dev:
	case st_pluggable_slot:
		node = dt_new(parent_node, entry->etype == st_builtin_dev ?
						"builtin" : "pluggable");
		slot_trace(node);

		if (entry->name)
			dt_add_property_string(node, "label", entry->name);
		break;

	case st_sw_upstream:
		parse_switch(parent_node, entry);
		return true;

	case st_end:
		assert(0);
		return false;
	}

	if (children)
		for (child = entry->children; child->etype != st_end; child++)
			parse_one_slot(node, child);

	return true;
}

void slot_table_init(const struct slot_table_entry *table_root)
{
	assert(!dt_slots);

	/* dt_slots is global */
	dt_slots = dt_new(dt_root, "ibm,pcie-slots");
	dt_add_property_cells(dt_slots, "#address-cells", 2);
	dt_add_property_cells(dt_slots, "#size-cells", 0);

	for (; table_root->etype != st_end; table_root++)
		parse_one_slot(dt_slots, table_root);
}

static void add_slot_properties(struct pci_slot *slot,
				struct dt_node *np)
{
	struct phb *phb = slot->phb;
	struct pci_device *pd = slot->pd;
	struct dt_node *slot_node = slot->data;
	char label[8], loc_code[LOC_CODE_SIZE];
	size_t base_loc_code_len;
	const char *slot_label = NULL;

	if (!np)
		return;

	/* if we have a label on the device or buse use it for the slot label */
	if (slot_node) {
		/* add a cross reference for the pcie slot */
		dt_add_property_cells(np, "ibm,pcie-slot", slot_node->phandle);

		if (dt_has_node_property(slot_node, "label", NULL))
			slot_label = dt_prop_get(slot_node, "label");
		else if (dt_has_node_property(slot_node->parent, "label", NULL))
			slot_label = dt_prop_get(slot_node->parent, "label");
	}

	if (!slot_label) {
		snprintf(label, 8, "S%04x%02x", phb->opal_id,
				pd->secondary_bus);
		slot_label = label;
	}

	dt_add_property_string(np, "ibm,slot-label", slot_label);

	base_loc_code_len = phb->base_loc_code ? strlen(phb->base_loc_code) : 0;
	if ((base_loc_code_len + strlen(slot_label) + 1) >= LOC_CODE_SIZE)
		return;

	/* Location code */
	if (phb->base_loc_code) {
		strcpy(loc_code, phb->base_loc_code);
		strcat(loc_code, "-");
	} else {
		loc_code[0] = '\0';
	}

	strcat(loc_code, slot_label);
	dt_add_property(np, "ibm,slot-location-code",
			loc_code, strlen(loc_code) + 1);
}

static void init_slot_info(struct pci_slot *slot, bool pluggable, void *data)
{
	slot->data = data;
	slot->ops.add_properties = add_slot_properties;

	slot->pluggable      = pluggable;
	slot->power_ctl      = false;
	slot->wired_lanes    = PCI_SLOT_WIRED_LANES_UNKNOWN;
	slot->connector_type = PCI_SLOT_CONNECTOR_PCIE_NS;
	slot->card_desc      = PCI_SLOT_DESC_NON_STANDARD;
	slot->card_mech      = PCI_SLOT_MECH_NONE;
	slot->power_led_ctl  = PCI_SLOT_PWR_LED_CTL_NONE;
	slot->attn_led_ctl   = PCI_SLOT_ATTN_LED_CTL_NONE;
}

static void create_dynamic_slot(struct phb *phb, struct pci_device *pd)
{
	uint32_t ecap, val;
	struct pci_slot *slot;

	if (!phb || !pd || pd->slot)
		return;

	/* Try to create slot whose details aren't provided by platform.
	 * We only care the downstream ports of PCIe switch that connects
	 * to root port.
	 */
	if (pd->dev_type != PCIE_TYPE_SWITCH_DNPORT ||
	    !pd->parent || !pd->parent->parent ||
	    pd->parent->parent->parent)
		return;

	ecap = pci_cap(pd, PCI_CFG_CAP_ID_EXP, false);
	pci_cfg_read32(phb, pd->bdfn, ecap + PCICAP_EXP_SLOTCAP, &val);
	if (!(val & PCICAP_EXP_SLOTCAP_HPLUG_CAP))
		return;

	slot = pcie_slot_create(phb, pd);
	assert(slot);
	init_slot_info(slot, true, NULL);

	/* On superMicro's "p8dnu" platform, we create dynamic PCI slots
	 * for all downstream ports of PEX9733 that is connected to PHB
	 * direct slot. The power supply to the PCI slot is lost after
	 * PCI adapter is removed from it. The power supply can't be
	 * turned on when the slot is in empty state. The power supply
	 * isn't turned on automatically when inserting PCI adapter to
	 * the slot at later point. We set a flag to the slot here, to
	 * turn on the power supply in (suprise or managed) hot-add path.
	 *
	 * We have same issue with PEX8718 as above on "p8dnu" platform.
	 */
	if (dt_node_is_compatible(dt_root, "supermicro,p8dnu") && slot->pd &&
	    (slot->pd->vdid == 0x973310b5 || slot->pd->vdid == 0x871810b5))
		pci_slot_add_flags(slot, PCI_SLOT_FLAG_FORCE_POWERON);
}

void slot_table_get_slot_info(struct phb *phb, struct pci_device *pd)
{
	struct dt_node *slot_node;
	struct pci_slot *slot;
	bool pluggable;

	if (!pd || pd->slot)
		return;

	slot_node = map_pci_dev_to_slot(phb, pd);
	if (!slot_node) { /* XXX: might want to check the conditions exactly */
		create_dynamic_slot(phb, pd);
		return;
	}

	slot = pcie_slot_create(phb, pd);
	assert(slot);

	pluggable = !strcmp(slot_node->name, "pluggable");
	init_slot_info(slot, pluggable, (void *) slot_node);
}

extern int __print_slot(struct phb *phb, struct pci_device *pd, void *userdata);

/* FIXME: this doesn't check shit */
void check_all_slot_table(void)
{
	struct phb *phb;

	prlog(PR_DEBUG, "PCI: Checking slot table against detected devices\n");

	for_each_phb(phb)
		pci_walk_dev(phb, NULL, __print_slot, NULL);
}
