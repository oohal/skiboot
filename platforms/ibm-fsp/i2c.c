/* Copyright 2017-2018 IBM Corp.
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

#include <device.h>
#include <cpu.h>
#include <vpd.h>
#include <interrupts.h>
#include <ccan/str/str.h>
#include <chip.h>

#define P9_I2CM_XSCOM_SIZE 0x1000
#define P9_I2CM_XSCOM_BASE 0xa0000

static struct dt_node *get_i2cm_node(struct dt_node *xscom, int engine)
{
	uint64_t xscom_base = P9_I2CM_XSCOM_BASE + P9_I2CM_XSCOM_SIZE * (uint64_t)engine;
	struct dt_node *i2cm;
	uint64_t freq, clock;

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

		freq = dt_prop_get_u64_def(xscom, "bus-frequency", 0);
		clock = (u32)(freq / 4);
		if (clock)
			dt_add_property_cells(i2cm, "clock-frequency", clock);
		else
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
		dt_add_property_cells(bus, "#size-cells", 0);
		dt_add_property_cells(bus, "#address-cells", 1);

		/* The P9 I2C master is fully compatible with the P8 one */
		dt_add_property_strings(bus, "compatible", "ibm,opal-i2c",
			"ibm,power8-i2c-port", "ibm,power9-i2c-port");

		/*
		 * use the clock frequency as the bus frequency until we
		 * have actual devices on the bus. Adding a device will
		 * reduce the frequency to something that all devices
		 * can tolerate.
		 */
		dt_add_property_cells(bus, "bus-frequency", freq * 1000);
	}

	return bus;
}

static void add_dimm_buses(void)
{
	struct dt_node *xscom, *i2cm;

	dt_for_each_compatible(dt_root, xscom, "ibm,power9-xscom") {
		i2cm = get_i2cm_node(xscom, 3);

		get_bus_node(i2cm, 0, 400);
		get_bus_node(i2cm, 1, 400);
	}
}

