/* Copyright 2013-2016 IBM Corp.
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

#include <skiboot.h>
#include <fsp.h>
#include <fsp-sysparam.h>
#include <psi.h>
#include <chiptod.h>
#include <nx.h>
#include <cpu.h>
#include <processor.h>
#include <xscom.h>
#include <opal.h>
#include <opal-msg.h>
#include <elf.h>
#include <io.h>
#include <cec.h>
#include <device.h>
#include <pci.h>
#include <lpc.h>
#include <i2c.h>
#include <chip.h>
#include <interrupts.h>
#include <mem_region.h>
#include <trace.h>
#include <console.h>
#include <fsi-master.h>
#include <centaur.h>
#include <libfdt/libfdt.h>
#include <timer.h>
#include <ipmi.h>
#include <sensor.h>
#include <xive.h>
#include <nvram.h>
#include <vas.h>
#include <libstb/secureboot.h>
#include <libstb/trustedboot.h>
#include <phys-map.h>
#include <imc.h>
#include <dts.h>
#include <sbe-p9.h>
#include <debug_descriptor.h>
#include <occ.h>

void main_cpu_entry(void);
void __noreturn main_cpu_entry(void) {
	for (;;) {}
}

void secondary_cpu_entry(void);
void __noreturn secondary_cpu_entry(void) {
	for (;;) {}
}

void __secondary_cpu_entry(void);
void __noreturn __secondary_cpu_entry(void) {
	for (;;) {}
}

void fast_reboot_entry(void);
void __noreturn fast_reboot_entry(void) {
	for (;;) {}
}

void __attribute__((const)) disable_fast_reboot(const char *reason __unused)
{
	return;
}

void __attribute__((const)) fast_reboot(void) {
	return;
}

enum proc_gen proc_gen;
unsigned int pcie_max_link_speed;

#ifdef SKIBOOT_GCOV
void skiboot_gcov_done(void);
#endif

struct debug_descriptor debug_descriptor = {
	.eye_catcher	= "OPALdbug",
	.version	= DEBUG_DESC_VERSION,
	.state_flags	= 0,
	.memcons_phys	= (uint64_t)&memcons,
	.trace_mask	= 0, /* All traces disabled by default */
	/* console log level:
	 *   high 4 bits in memory, low 4 bits driver (e.g. uart). */
	.console_log_levels = (PR_TRACE << 4) | PR_TRACE,
};

void print_debug_desc(const char *file, int line);
void print_debug_desc(const char *file, int line)
{
	prerror("%s:%d: debug desc = %x\n", file, line, debug_descriptor.console_log_levels);
}

extern uint64_t boot_offset;

void *fdt;

static u8 console_get_level(const char *s)
{
	if (strcmp(s, "emerg") == 0)
		return PR_EMERG;
	if (strcmp(s, "alert") == 0)
		return PR_ALERT;
	if (strcmp(s, "crit") == 0)
		return PR_CRIT;
	if (strcmp(s, "err") == 0)
		return PR_ERR;
	if (strcmp(s, "warning") == 0)
		return PR_WARNING;
	if (strcmp(s, "notice") == 0)
		return PR_NOTICE;
	if (strcmp(s, "printf") == 0)
		return PR_PRINTF;
	if (strcmp(s, "info") == 0)
		return PR_INFO;
	if (strcmp(s, "debug") == 0)
		return PR_DEBUG;
	if (strcmp(s, "trace") == 0)
		return PR_TRACE;
	if (strcmp(s, "insane") == 0)
		return PR_INSANE;
	/* Assume it's a number instead */
	return atoi(s);
}

static void console_log_level(void)
{
	const char *s;
	u8 level;

	/* console log level:
	 *   high 4 bits in memory, low 4 bits driver (e.g. uart). */
	s = nvram_query("log-level-driver");
	if (s) {
		level = console_get_level(s);
		debug_descriptor.console_log_levels =
			(debug_descriptor.console_log_levels & 0xf0 ) |
			(level & 0x0f);
		prlog(PR_NOTICE, "console: Setting driver log level to %i\n",
		      level & 0x0f);
	}
	s = nvram_query("log-level-memory");
	if (s) {
		level = console_get_level(s);
		debug_descriptor.console_log_levels =
			(debug_descriptor.console_log_levels & 0x0f ) |
			((level & 0x0f) << 4);
		prlog(PR_NOTICE, "console: Setting memory log level to %i\n",
		      level & 0x0f);
	}
}

typedef void (*ctorcall_t)(void);

static void __nomcount do_ctors(void)
{
	extern ctorcall_t __ctors_start[], __ctors_end[];
	ctorcall_t *call;

	for (call = __ctors_start; call < __ctors_end; call++)
		(*call)();
}

static void pci_nvram_init(void)
{
	const char *nvram_speed;

	pcie_max_link_speed = 0;

	nvram_speed = nvram_query("pcie-max-link-speed");
	if (nvram_speed) {
		pcie_max_link_speed = atoi(nvram_speed);
		prlog(PR_NOTICE, "PHB: NVRAM set max link speed to GEN%i\n",
		      pcie_max_link_speed);
	}
}



// defined in the shim
void map_one(uint64_t addr, uint64_t size);

static void map_cell(struct dt_node *n, const char *prop)
{
	uint64_t addr, size = 0;

	if (streq(prop, "reg"))
		addr = dt_get_address(n, 0, &size);
	else
		addr = dt_read_address(n, prop, 2, 1, 0, &size);

	map_one(addr, size);
}


/* what do we need to do here exactly?
 *
 * Remember the end goal is to have this running in userspace entirely seperate to the rest of
 * OPAL. So...
 *
 * a) How much runtime environment setup do we need?
 *   1. expand the DT
 *   2. parse chip and xscom information?
 *   3. nvram?
 *
 *   anything else? the phb init is slightly different since we've already allocated the tables
 *   but it should be possible
 */

void do_opal_inits(void *fdt_buf);
void do_opal_inits(void *fdt_buf)
{
	struct dt_node *dn;

	/* Call library constructors  -- do we need to do this here?*/
	do_ctors();

	prlog(PR_NOTICE, "OPAL %s%s starting...\n", version,
#ifdef DEBUG
	"-debug"
#else
	""
#endif
	);
	prlog(PR_DEBUG, "initial console log level: memory %d, driver %d\n",
	       (debug_descriptor.console_log_levels >> 4),
	       (debug_descriptor.console_log_levels & 0x0f));
	prlog(PR_TRACE, "OPAL is Powered By Hacks From the Bong (TM).\n");

#ifdef SKIBOOT_GCOV
	skiboot_gcov_done();
#endif

	/* Initialize boot cpu's cpu_thread struct */

	// XXX: we need to pre-allocate these
	init_boot_cpu();

	/* Now locks can be used */
	init_locks();

	/* Create the OPAL call table early on, entries can be overridden
	 * later on (FSP console code for example)
	 */
//	opal_table_init();

	/* Init the physical map table so we can start mapping things */
	phys_map_init();


	// FIXME: source the DT from somewhere, maybe just inhale a file
	dt_root = dt_new_root("");
	dt_expand(fdt_buf);

	/*
	 * From there, we follow a fairly strict initialization order.
	 *
	 * First we need to build up our chip data structures and initialize
	 * XSCOM which will be needed for a number of susbequent things.
	 *
	 * We want XSCOM available as early as the platform probe in case the
	 * probe requires some HW accesses.
	 *
	 * We also initialize the FSI master at that point in case we need
	 * to access chips via that path early on.
	 */
	init_chips();

	dt_for_each_compatible(dt_root, dn, "ibm,xscom")
		map_cell(dn, "reg");

	xscom_init();

	/*
	 * This should be done before mem_region_init, so the stack
	 * region length can be set according to the maximum PIR.
	 */
	init_cpu_max_pir();

	mem_region_init();

	/* Initialize the rest of the cpu thread structs */
	init_all_cpus();
#if 0
	seems unneeded
	if (proc_gen == proc_gen_p9)
		cpu_set_ipi_enable(true);
#endif

	/*
	 * We probe the platform now. This means the platform probe gets
	 * the opportunity to reserve additional areas of memory if needed.
	 *
	 * Note: Timebases still not synchronized.
	 */
	// we'll need to be careful here. the platform can do things that'll break in
	// standalone mode
	prerror("%s:%d: debug desc = %x\n", __FILE__, __LINE__,
		debug_descriptor.console_log_levels);
	probe_platform();

	/* Allocate our split trace buffers now. Depends add_opal_node() */
	//init_trace_buffers(); re-enable this later

	// fetch the ICP address for each cpu thread. I don't think we need this
	//init_interrupts();
/*
	if (proc_gen == proc_gen_p7 || proc_gen == proc_gen_p8)
		cpu_set_ipi_enable(true);
*/

	/* Initialize PSI (depends on probe_platform being called) */
//	psi_init();

	/* Initialize/enable LPC interrupts. This must be done after the
	 * PSI interface has been initialized since it serves as an interrupt
	 * source for LPC interrupts.
	 */
//	lpc_init_interrupts();

	/* Initialize i2c */
//	p8_i2c_init(); // needed?

#if 0
        /*
	 * Initialize the opal messaging before platform.init as we are
	 * getting request to queue occ load opal message when host services
	 * got load occ request from FSP
	 */
        opal_init_msg(); // needed?

	/*
	 * We have initialized the basic HW, we can now call into the
	 * platform to perform subsequent inits, such as establishing
	 * communication with the FSP or starting IPMI.
	 */
	if (platform.init)
		platform.init();
#endif

	/* Read in NVRAM and set it up */
	nvram_init();

	prerror("%s:%d: debug desc = %x\n", __FILE__, __LINE__,
		debug_descriptor.console_log_levels);
	/* Set the console level */
	console_log_level();
}

void do_pci_inits(void);
void do_pci_inits(void)
{
	struct dt_node *dn;

	pci_nvram_init();

	// this should already be in the DT
	//preload_io_vpd();

	// we need to copy the capp ucode lid somewhere accessable since the PHB3/4 drivers
	// need to reload it after a CAPP reset, but deal with that later.
	//preload_capp_ucode(); // needed?

	// map the xscom ranges
	/* Probe IO hubs */
	probe_p7ioc();

	dt_for_each_compatible(dt_root, dn, "ibm,power8-pciex") {
		map_cell(dn, "reg");

		map_cell(dn, "ibm,opal-ivt-table");
		map_cell(dn, "ibm,opal-peltv-table");
		map_cell(dn, "ibm,opal-pest-table");
		map_cell(dn, "ibm,opal-rba-table");
		map_cell(dn, "ibm,opal-rtt-table");
	}

	prerror("%s:%d: debug desc = %x\n", __FILE__, __LINE__,
		debug_descriptor.console_log_levels);
	/* Probe PHB3 on P8 */
	probe_phb3();
	prerror("%s:%d: debug desc = %x\n", __FILE__, __LINE__, debug_descriptor.console_log_levels);

	/* Probe PHB4 on P9 */
	probe_phb4();
#if 0
	/* Probe NPUs */
	probe_npu();
	probe_npu2();
#endif
	/* Initialize do the actual PCI scanning */
	pci_init_slots();
}
