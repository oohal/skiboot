#include <stdbool.h>

#if 0
/* Add any stub functions required for linking here. */
static void stub_function(void)
{
	abort();
}
#endif
#define STUB(fnname) \
	void fnname(void) __attribute__((weak, alias ("stub_function")))

//STUB(fsp_preload_lid);
//STUB(fsp_wait_lid_loaded);
//STUB(fsp_adjust_lid_side);

/* Add HW specific stubs here */
static int true_stub(void) { return 1; }
static int false_stub(void) { return 0; }

#define TRUE_STUB(fnname) \
	int fnname(void) __attribute__((weak, alias ("true_stub")))
#define FALSE_STUB(fnname) \
	int fnname(void) __attribute__((weak, alias ("false_stub")))
#define NOOP_STUB FALSE_STUB

//TRUE_STUB(lock_held_by_me);
NOOP_STUB(start_kernel_secondary);
NOOP_STUB(do_slw_rvwinkle);
NOOP_STUB(init_shared_sprs);
NOOP_STUB(init_replicated_sprs);


long opal_branch_table;
long reset_patch_start;
long reset_patch_end;
//long _etext;
long hir_trigger;
long attn_trigger;
long opal_entry;

void do_opal_inits(void *fdt);
void do_it(void *);
void do_pci_inits(void);

void do_it(void *fdt)
{
	struct dt_node *n;
	unsigned long saved_r13;

	asm volatile("mr %0,13" : "=r" (saved_r13));
	asm volatile("mr 13,%0" : : "r" (0x31c00000));
	do_opal_inits(fdt);
	do_pci_inits();
	asm volatile("mr 13,%0" : : "r" (saved_r13));
}

