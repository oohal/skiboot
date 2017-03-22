#include <device.h>
#include <cpu.h>
#include <vpd.h>
#include <interrupts.h>
#include <ccan/str/str.h>
#include <chip.h>

#include "spira.h"
#include "hdata.h"

struct i2c_dev {
	uint8_t i2cm_engine;
	uint8_t i2cm_port;
	__be16 i2c_bus_freq;

	/* i2c slave info */
	uint8_t type;
	uint8_t i2c_addr;
	uint8_t i2c_port;
	uint8_t __reserved;

	__be32 purpose;
	__be32 i2c_link;
	__be16 slca_index;
};

#define P9_I2CM_XSCOM_SIZE 0x1000
#define P9_I2CM_XSCOM_BASE 0xa0000

static struct dt_node *get_i2cm_node(struct dt_node *xscom, int engine)
{
	uint64_t xscom_base = P9_I2CM_XSCOM_BASE + P9_I2CM_XSCOM_SIZE * engine;
	struct dt_node *i2cm;

	i2cm = dt_find_by_name_addr(xscom, "i2cm", xscom_base);
	if (!i2cm) {
		i2cm = dt_new_addr(xscom, "i2cm", xscom_base);
		dt_add_property_cells(i2cm, "reg", xscom_base,
			P9_I2CM_XSCOM_SIZE);

		dt_add_property_strings(i2cm, "compatible",
			"ibm,power8-i2cm", "ibm,power9-i2cm");

		dt_add_property_cells(i2cm, "#size-cells", 0);
		dt_add_property_cells(i2cm, "#address-cells", 1);
		dt_add_property_cells(i2cm, "chip-engine#", engine);

		/*
		 * On P8 the clock heirachy goes:
		 * Nest clock (2GHzish)
		 * 	div 4 -> PCB clock (600MHz-ish)
		 * 		div 4 -> fed into i2c (150MHz-ish)
		 * 			I2C master has it's own internal divider
		 * 			which produces the actual bus frequency
		 *
		 * TODO: Verify this is still true on p9 a
		 *
		 * The 'clock-frequency' DT property of the P8 I2CM is the clock
		 * that's fed into the I2CM from the PCB. We might need to
		 * adjust the clock in skiboot to make it safe for all the
		 * devices on the bus. The clock frequency that appears on each
		 * bus (port) is under the bus node as 'bus-frequency'.
		 */
		dt_add_property_cells(i2cm, "clock-frequency", 150000000);
	}

	return i2cm;
}

static struct dt_node *get_bus_node(struct dt_node *i2cm, int port, int freq)
{
	struct dt_node *bus;

	bus = dt_find_by_name_addr(i2cm, "i2c-bus", port);
	if (!bus) {
		bus = dt_new_addr(i2cm, "i2c-bus", port);
		dt_add_property_cells(bus, "reg", port);

		/* The p9 I2C master is identical to the p8 one */
		dt_add_property_strings(bus, "compatible", "ibm,opal-i2c",
			"ibm,power8-i2c-port", "ibm,power9-i2c-port");

		dt_add_property_cells(bus, "#size-cells", 0);
		dt_add_property_cells(bus, "#address-cells", 1);

		/*
		 * use the clock frequency as the bus frequency until we
		 * have actual devices on the bus. Adding a device will
		 * reduce the frequency to something that all devices
		 * can tolerate
		 */
		dt_add_property_cells(bus, "bus-frequency", freq);
	}

	return bus;
}

struct hdat_i2c_type {
	uint32_t id;
	const char *name;
	const char *compat;
};

struct hdat_i2c_type hdat_i2c_devs[] = {
	{ 0x2, "eeprom", "atmel,24c128" } /* XXX: Taken from a firestone, make sure this is what's actually on a p9 */
};

struct hdat_i2c_label {
	uint32_t id;
	const char *label;
};

struct hdat_i2c_label hdat_i2c_labels[] = {
	{ 0x1, "9551-led-controller" },
	{ 0x2, "seeprom" },
	{ 0x5, "module-vpd" },
	{ 0x6, "dimm SPD" },
	{ 0x7, "proc-vpd" },
	{ 0x8, "sbe-eeprom" },
	{ 0x9, "planar-vpd" }
};

/*
 * this is pretty half-assed, to generate the labels properly we need to look
 * up associated SLCA index and determine what kind of module the device is on
 * and why
 */
static struct hdat_i2c_type *map_type(uint32_t type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdat_i2c_devs); i++)
		if (hdat_i2c_devs[i].id == type)
			return &hdat_i2c_devs[i];

	return NULL;
}

static const char *map_label(uint32_t type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdat_i2c_labels); i++)
		if (hdat_i2c_labels[i].id == type)
			return hdat_i2c_labels[i].label;

	return NULL;
}

int parse_i2c_devs(const struct HDIF_common_hdr *hdr, int idata_index,
	struct dt_node *xscom)
{
	struct dt_node *i2cm, *bus, *node;
	const struct hdat_i2c_type *type;
	const struct i2c_dev *dev;
	const char *label;
	int i, count;

	/*
	 * This code makes a few assumptions about XSCOM addrs, etc
	 * and will need updating for new processors
	 */
	assert(proc_gen == proc_gen_p9);

	count = HDIF_get_iarray_size(hdr, idata_index);
	for (i = 0; i < count; i++) {
		dev = HDIF_get_iarray_item(hdr, idata_index, i, NULL);

		i2cm = get_i2cm_node(xscom, dev->i2cm_engine);
		bus = get_bus_node(i2cm, dev->i2cm_port,
			be16_to_cpu(dev->i2c_bus_freq));

		prlog(PR_ERR, "iterating i2c devs: %d/%d "
		"eng = %d, bus = %d\n", i, count, dev->i2cm_engine, dev->i2cm_port);

		type = map_type(dev->type);
		label = map_label(be32_to_cpu(dev->purpose));

		if (type) {
			node = dt_new_addr(bus, type->name, dev->i2c_addr);
			dt_add_property_string(node, "compatible", type->compat);
		} else {
			node = dt_new_addr(bus, "unknown", dev->i2c_addr);
		}

		/*
		 * Looks like hostboot gives the address as an 8 bit, left
		 * justified quantity (i.e it includes the R/W bit). So we need
		 * to strip it off to get an address linux can use.
		 */
		dt_add_property_cells(node, "reg", dev->i2c_addr >> 1);

		/* Make sure the OS doesn't touch it... */
//		dt_add_property_string(node, "status", "reserved");
		if (label)
			dt_add_property_string(node, "label", label);

		dt_add_property_cells(node, "link-id", be32_to_cpu(dev->i2c_link));

		/*
		 * XXX: What should I do with the SLCA index? given we need
		 * to crossref it with the PCIe slots...
		 */
	}

	return 0;
}
