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

static const struct slot_table_entry witherspoon_gpu0[] = {
	{
		.etype = st_pluggable_slot,
		.location = ST_LOC_DEVFN(0x80,0),
		.name = "GPU0",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_gpu1[] = {
	{
		.etype = st_pluggable_slot,
		.location = ST_LOC_DEVFN(0xa0,0),
		.name = "GPU1",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_gpu2[] = {
	{
		.etype = st_pluggable_slot,
		.location = ST_LOC_DEVFN(0xc0,0),
		.name = "GPU2",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_gpu3[] = {
	{
		.etype = st_pluggable_slot,
		.location = ST_LOC_DEVFN(0x60,0),
		.name = "GPU3",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_gpu4[] = {
	{
		.etype = st_pluggable_slot,
		.location = ST_LOC_DEVFN(0x80,0),
		.name = "GPU4",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_gpu5[] = {
	{
		.etype = st_pluggable_slot,
		.location = ST_LOC_DEVFN(0xa0,0),
		.name = "GPU5",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_plx0_down[] = {
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0x4a,0),
		.children = witherspoon_gpu0,
		.name = "GPU0 down",
	},
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0x4b,0),
		.children = witherspoon_gpu1,
		.name = "GPU1 down",
	},
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0x4c,0),
		.children = witherspoon_gpu2,
		.name = "GPU2 down",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_plx1_down[] = {
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0x44,0),
		.children = witherspoon_gpu3,
		.name = "GPU3 down",
	},
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0x45,0),
		.children = witherspoon_gpu4,
		.name = "GPU4 down",
	},
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0x4d,0),
		.children = witherspoon_gpu5,
		.name = "GPU5 down",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_plx0_up[] = {
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0x20,0),
		.children = witherspoon_plx0_down,
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_plx1_up[] = {
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0x20,0),
		.children = witherspoon_plx1_down,
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_phb0_4_slot[] = {
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0,0),
		.children = witherspoon_plx0_up,
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_phb8_5_slot[] = {
	{
		.etype = st_builtin_dev,
		.location = ST_LOC_DEVFN(0,0),
		.children = witherspoon_plx1_up,
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_npu0_slots[] = {
	{
		.etype = st_npu_slot,
		.location = ST_LOC_NPU_GROUP(0),
		.name = "GPU0",
	},
	{
		.etype = st_npu_slot,
		.location = ST_LOC_NPU_GROUP(1),
		.name = "GPU1",
	},
	{
		.etype = st_npu_slot,
		.location = ST_LOC_NPU_GROUP(2),
		.name = "GPU2",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_npu8_slots[] = {
	{
		.etype = st_npu_slot,
		.location = ST_LOC_NPU_GROUP(0),
		.name = "GPU3",
	},
	{
		.etype = st_npu_slot,
		.location = ST_LOC_NPU_GROUP(1),
		.name = "GPU4",
	},
	{
		.etype = st_npu_slot,
		.location = ST_LOC_NPU_GROUP(2),
		.name = "GPU5",
	},
	{ .etype = st_end },
};

static const struct slot_table_entry witherspoon_phb_table[] = {
	{
		.etype = st_phb,
		.location = ST_LOC_PHB(0,4),
		.children = witherspoon_phb0_4_slot,
	},
	{
		.etype = st_phb,
		.location = ST_LOC_PHB(0,7),
		.children = witherspoon_npu0_slots,
	},
	{
		.etype = st_phb,
		.location = ST_LOC_PHB(8,5),
		.children = witherspoon_phb8_5_slot,
	},
	{
		.etype = st_phb,
		.location = ST_LOC_PHB(8,8),
		.children = witherspoon_npu8_slots,
	},
	{ .etype = st_end },
};

#define NPU_BASE 0x5011000
#define NPU_SIZE 0x2c
#define NPU_INDIRECT0	0x8000000009010c3f
#define NPU_INDIRECT1	0x800000000c010c3f

static void create_link(struct dt_node *npu, int group, int index)
{
	struct dt_node *link;
	uint32_t lane_mask;
	uint64_t phy;
	char namebuf[32];

	snprintf(namebuf, sizeof(namebuf), "link@%x", index);
	link = dt_new(npu, namebuf);

	dt_add_property_string(link, "compatible", "ibm,npu-link");
	dt_add_property_cells(link, "ibm,npu-link-index", index);

	if (!(index / 3))
		phy = NPU_INDIRECT0;
	else
		phy = NPU_INDIRECT1;

	switch (index % 3) {
	case 0:
		lane_mask = 0xf1e000;
		break;

	case 1:
		lane_mask = 0x0e1870;
		break;

	case 2:
		lane_mask = 0x00078f;
		break;

	default:
		assert(0);
	}

	dt_add_property_u64s(link, "ibm,npu-phy", phy);
	dt_add_property_cells(link, "ibm,npu-lane-mask", lane_mask);
	dt_add_property_cells(link, "ibm,npu-group-id", group);
}

static void dt_create_npu2(void)
{
        struct dt_node *xscom, *npu;
        char namebuf[32];
	int phb_index = 7;
	int npu_index = 0;

	dt_for_each_compatible(dt_root, xscom, "ibm,xscom") {
		snprintf(namebuf, sizeof(namebuf), "npu@%x", NPU_BASE);
		npu = dt_new(xscom, namebuf);
		dt_add_property_cells(npu, "reg", NPU_BASE, NPU_SIZE);
		dt_add_property_strings(npu, "compatible", "ibm,power9-npu");

		dt_add_property_cells(npu, "ibm,phb-index", phb_index++);
		dt_add_property_cells(npu, "ibm,npu-index", npu_index++);
		dt_add_property_cells(npu, "ibm,npu-links", 6);

		create_link(npu, 0, 0);
		create_link(npu, 0, 1);
		create_link(npu, 1, 2);
		create_link(npu, 1, 3);
		create_link(npu, 2, 4);
		create_link(npu, 2, 5);
	}
}

static bool witherspoon_probe(void)
{
	if (!dt_node_is_compatible(dt_root, "ibm,witherspoon"))
		return false;

	/* Lot of common early inits here */
	astbmc_early_init();

	/* Setup UART for use by OPAL (Linux hvc) */
	uart_set_console_policy(UART_CONSOLE_OPAL);

	/* Add NPU2 bindings */
	dt_create_npu2();

	slot_table_init(witherspoon_phb_table);

	return true;
}

DECLARE_PLATFORM(witherspoon_platform) = {
	.name			= "Witherspoon",
	.probe			= witherspoon_probe,
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
};
