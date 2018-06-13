/* Copyright 2017 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <skiboot.h>
#include <device.h>
#include <console.h>
#include <chip.h>
#include <ipmi.h>
#include <psi.h>
#include <npu-regs.h>

#include "astbmc.h"

/* backplane slots */
static const struct slot_table_entry hdd_bay_slots[] = {
	SW_PLUGGABLE("hdd0", 0xe),
	SW_PLUGGABLE("hdd1", 0x4),
	SW_PLUGGABLE("hdd2", 0x5),
	SW_PLUGGABLE("hdd3", 0x6),
	SW_PLUGGABLE("hdd4", 0x7),
	SW_PLUGGABLE("hdd5", 0xf),
	SW_PLUGGABLE("hdd6", 0xc),
	SW_PLUGGABLE("hdd7", 0xd),
	SW_PLUGGABLE("hdd8", 0x14),
	SW_PLUGGABLE("hdd9", 0x17),
	SW_PLUGGABLE("hdd10", 0x8),
	SW_PLUGGABLE("hdd11", 0xb),
	SW_PLUGGABLE("hdd12", 0x10),
	SW_PLUGGABLE("hdd13", 0x13),
	SW_PLUGGABLE("hdd14", 0x16),
	SW_PLUGGABLE("hdd15", 0x09),
	SW_PLUGGABLE("hdd16", 0xa),
	SW_PLUGGABLE("hdd17", 0x11),
	SW_PLUGGABLE("hdd18", 0x12),
	SW_PLUGGABLE("hdd19", 0x15),

	{ .etype = st_end },
};

/* 9797 upstream, for some reason both virtual switches are on "port 0" */
const struct slot_table_entry hdd_upstream[] = {
	SW_PLUGGABLE("9797 Upstream", 0x0, .children = hdd_bay_slots),
	{ .etype = st_end },
};

/* Switch card */
const struct slot_table_entry sw_downstream[] = {
	SW_PLUGGABLE("Port 0", 0x6, .children = hdd_upstream),
	SW_PLUGGABLE("Port 1", 0x7, .children = hdd_upstream),
	SW_PLUGGABLE("Port 2", 0x4),
	SW_PLUGGABLE("Port 3", 0x5),
	{ .etype = st_end },
};

const struct slot_table_entry sw_upstream[] = {
	SW_PLUGGABLE("upstream", 0x0, .children = sw_downstream),
	{ .etype = st_end },
};

/* root complex */
const struct slot_table_entry pe4_rc[] = { /* root complex slot */
	SW_PLUGGABLE("PE4", 0x0, .children = sw_upstream),
	{ .etype = st_end },
};

const struct platform_ocapi zaius_ocapi = {
	.i2c_engine        = 1,
	.i2c_port          = 4,
	.i2c_reset_addr    = 0x20,
	.i2c_reset_odl0    = (1 << 1),
	.i2c_reset_odl1    = (1 << 6),
	.i2c_presence_addr = 0x20,
	.i2c_presence_odl0 = (1 << 2), /* bottom connector */
	.i2c_presence_odl1 = (1 << 7), /* top connector */
	.odl_phy_swap      = true,
};

static const struct slot_table_entry zaius_phb_table[] = {
/*
	ST_PHB_ENTRY(0, 0, romulus_cpu1_slot2),
	ST_PHB_ENTRY(0, 1, romulus_cpu1_slot1),

	ST_PHB_ENTRY(0, 2, romulus_builtin_raid),
	ST_PHB_ENTRY(0, 3, romulus_builtin_usb),
	ST_PHB_ENTRY(0, 4, romulus_builtin_ethernet),
	ST_PHB_ENTRY(0, 5, romulus_builtin_bmc),
*/
	ST_PHB_ENTRY(8, 0, pe4_rc), // might be swapped with 3
/*
	ST_PHB_ENTRY(8, 1, romulus_cpu2_slot3), // might be PHB1 or 2
	ST_PHB_ENTRY(8, 3, romulus_cpu2_slot1),
*/
	{ .etype = st_end },
};

#define NPU_BASE 0x5011000
#define NPU_SIZE 0x2c
#define NPU_INDIRECT0	0x8000000009010c3f /* OB0 - no OB3 on Zaius */

/* OpenCAPI only */
static void create_link(struct dt_node *npu, int group, int index)
{
	struct dt_node *link;
	uint32_t lane_mask;
	char namebuf[32];

	snprintf(namebuf, sizeof(namebuf), "link@%x", index);
	link = dt_new(npu, namebuf);

	dt_add_property_string(link, "compatible", "ibm,npu-link-opencapi");
	dt_add_property_cells(link, "ibm,npu-link-index", index);

	switch (index) {
	case 2:
		lane_mask = 0xf1e000; /* 0-3, 7-10 */
		break;
	case 3:
		lane_mask = 0x00078f; /* 13-16, 20-23 */
		break;
	default:
		assert(0);
	}

	dt_add_property_u64s(link, "ibm,npu-phy", NPU_INDIRECT0);
	dt_add_property_cells(link, "ibm,npu-lane-mask", lane_mask);
	dt_add_property_cells(link, "ibm,npu-group-id", group);
	dt_add_property_u64s(link, "ibm,link-speed", 25000000000ul);
}

/* FIXME: Get rid of this after we get NPU information properly via HDAT/MRW */
static void zaius_create_npu(void)
{
	struct dt_node *xscom, *npu;
	int npu_index = 0;
	int phb_index = 7;
	char namebuf[32];

	/* Abort if there's already an NPU in the device tree */
	if (dt_find_compatible_node(dt_root, NULL, "ibm,power9-npu"))
		return;

	prlog(PR_DEBUG, "OCAPI: Adding NPU device nodes\n");
	dt_for_each_compatible(dt_root, xscom, "ibm,xscom") {
		snprintf(namebuf, sizeof(namebuf), "npu@%x", NPU_BASE);
		npu = dt_new(xscom, namebuf);
		dt_add_property_cells(npu, "reg", NPU_BASE, NPU_SIZE);
		dt_add_property_strings(npu, "compatible", "ibm,power9-npu");
		dt_add_property_cells(npu, "ibm,npu-index", npu_index++);
		dt_add_property_cells(npu, "ibm,phb-index", phb_index++);
		dt_add_property_cells(npu, "ibm,npu-links", 2);
		create_link(npu, 1, 2);
		create_link(npu, 2, 3);
	}
}

/* FIXME: Get rid of this after we get NPU information properly via HDAT/MRW */
static void zaius_create_ocapi_i2c_bus(void)
{
	struct dt_node *xscom, *i2cm, *i2c_bus;
	prlog(PR_DEBUG, "OCAPI: Adding I2C bus device node for OCAPI reset\n");
	dt_for_each_compatible(dt_root, xscom, "ibm,xscom") {
		i2cm = dt_find_by_name(xscom, "i2cm@a1000");
		if (!i2cm) {
			prlog(PR_ERR, "OCAPI: Failed to add I2C bus device node\n");
			continue;
		}

		if (dt_find_by_name(i2cm, "i2c-bus@4"))
			continue;

		i2c_bus = dt_new_addr(i2cm, "i2c-bus", 4);
		dt_add_property_cells(i2c_bus, "reg", 4);
		dt_add_property_cells(i2c_bus, "bus-frequency", 0x61a80);
		dt_add_property_strings(i2c_bus, "compatible",
					"ibm,opal-i2c", "ibm,power8-i2c-port",
					"ibm,power9-i2c-port");
	}
}

static bool zaius_probe(void)
{
	if (!dt_node_is_compatible(dt_root, "ingrasys,zaius"))
		return false;

	/* Lot of common early inits here */
	astbmc_early_init();

	/* Setup UART for direct use by Linux */
	uart_set_console_policy(UART_CONSOLE_OS);

	zaius_create_npu();
	zaius_create_ocapi_i2c_bus();

	slot_table_init(zaius_phb_table);

	return true;
}

DECLARE_PLATFORM(zaius) = {
	.name			= "Zaius",
	.probe			= zaius_probe,
	.init			= astbmc_init,
	.start_preload_resource	= flash_start_preload_resource,
	.resource_loaded	= flash_resource_loaded,
	.bmc			= NULL, /* FIXME: Add openBMC */
	.pci_get_slot_info	= slot_table_get_slot_info,
	.pci_probe_complete	= check_all_slot_table,
	.cec_power_down         = astbmc_ipmi_power_down,
	.cec_reboot             = astbmc_ipmi_reboot,
	.elog_commit		= ipmi_elog_commit,
	.exit			= ipmi_wdt_final_reset,
	.terminate		= ipmi_terminate,
	.ocapi			= &zaius_ocapi,
};
