
#define pr_fmt(fmt)    "BMC-DMA: " fmt

#include <skiboot.h>
#include <pci.h>
#include <phb4.h>
#include <timebase.h>
#include <pci-slot.h>
#include <pci-cfg.h>
//#include <hiomap.h>
#include <bmc-dma.h>

#define MBOX_DMA_BUS_ADDR 0x10000000

#define USE_BYPASS
#undef USE_RAW_MODE

struct phb *bmc_phb;
struct pci_device *bmc_pd;

bool bmc_dma_ready;

static int match_bmc(struct phb __unused *phb, struct pci_device *pd, void __unused *v)
{
	if (pd->vdid == ((0x2000 << 16) | 0x1a03)) /* Aspeed VGA device */
		return 1;
	if (pd->vdid == ((0x2402 << 16) | 0x1a03)) /* Aspeed BMC device */
		return 1;
	return 0;
}

struct OpalIoPhb4ErrorData *diag_data;

bool bmc_dma_available(void)
{
	return !!bmc_phb;
}

void bmc_dma_exit(void)
{
	if (bmc_dma_ok())
		bmc_dma_teardown();

	bmc_phb= NULL;
	bmc_pd = NULL;
	bmc_dma_ready = false;
}

void bmc_dma_probe(void)
{
	struct pci_device *bmc;
	struct phb *phb;

	/*
	 * We're calling PHB4 specific functions so limit it to PHB4 for now
	 * most of these are using the generic PHB ops so we can probably
	 * support other chips too once the callbacks are implemented.
	 */
	if (proc_gen != proc_gen_p9) {
		prlog(PR_DEBUG, "FIXME: only supported on p9\n");
		return;
	}

	for_each_phb(phb) {
		bmc = pci_walk_dev(phb, NULL, match_bmc, NULL);
		if (bmc)
			break;
	}

	if (!bmc) {
		prerror("No BMC device found?\n");
		return;
	}

	/* FIXME: need a way to discover that BMC can DMA to us */
	bmc_phb = phb;
	bmc_pd = bmc;

	bmc_dma_reinit();
	prerror("bmc-dma probed!\n");
}

static bool __bmc_dma_ok(void)
{
	uint16_t sev, err_type;
	uint8_t fstate;

	/* not probed */
	if (!bmc_phb || !bmc_pd)
		return false;

	/* not initialised */
	if (!bmc_dma_ready)
		return false;

	/*
	 * fenced?
	 *
	 * NB: This really just needs to look at the loadstore status register
	 * it's complicated because the OPAL API makes it so.
	 */
	bmc_phb->ops->eeh_freeze_status(bmc_phb, 0, &fstate, &err_type, &sev);
	if (fstate) {
		prerror("PHB error detected! state: %x type: %x, sev: %x\n",
			fstate, err_type, sev);
		phb4_eeh_dump_regs(phb_to_phb4(bmc_phb));
		return false;
	}

	/* probably ok! */
	return true;
}

bool bmc_dma_ok(void)
{
	bool a = __bmc_dma_ok();

	if (!a) {
		prerror("bmc is !ok? p: %p pd: %p, r: %d\n",
			bmc_phb, bmc_pd, bmc_dma_ready);
	}

	return a;
}

/* in order to map 0x1000_0000 with 64K pages we need a two level table */
static uint64_t __align(4096) base_table[512]; /* 4K */
static uint64_t __align(4096) tce_table[512]; /* 4K */

static void tce_table_map_one(uint64_t host_addr, uint64_t bus_addr)
{
	int l1_idx, l0_idx;

	bus_addr >>= 16;

	/* NB: TCE tables are 4k => 512 TCEs => 9 index bits per level */
	l0_idx = (bus_addr >> 9) & 0x1ff;
	l1_idx = bus_addr & 0x1ff;

	base_table[l0_idx] = ((uint64_t ) tce_table & ~0xfff)  | 0x3;
	tce_table[l1_idx]  = ((uint64_t ) host_addr & ~0xffff) | 0x3;
}

static void tce_table_map(uint64_t host_addr, uint64_t bus_addr, uint32_t size)
{
	int pfns = size / (64 * 1024);
	int i;

	for (i = 0; i < pfns; i++) {
		tce_table_map_one(host_addr, bus_addr);

		host_addr += 64 * 1024;
		bus_addr += 64 * 1024;
	}
}

void bmc_dma_reinit(void)
{
	struct phb *phb = bmc_phb;
	uint16_t sev, err_type;
	struct pci_device *pd;
	uint8_t fstate;

	assert(bmc_phb);
	bmc_dma_ready = false;

	/* first up, do we need to clear a freeze / fence? */
	phb->ops->eeh_freeze_status(phb, 0, &fstate, &err_type, &sev);
	if (fstate) { // ok, got some kind of freeze
		prerror("freeze detected! state: %x type: %x, sev: %x\n",
			fstate, err_type, sev);

		/* HACK: This is just to cause a diag spew */
		phb->ops->get_diag_data2(phb, &diag_data, sizeof(diag_data));

		if (sev && sev <= OPAL_EEH_SEV_PHB_FENCED) {
			/* clearing a fence requires a complete reset */
			phb->slot->ops.creset(phb->slot);
			pci_reset_phb(phb);

			/* gotta setup the mappings again */
			time_wait_ms(1000); /* wait out the link-up timer */
		} else {
			/* PE freeze, so try thaw it */
			phb->ops->eeh_freeze_clear(phb, 0, OPAL_EEH_ACTION_CLEAR_FREEZE_ALL);
		}
		opal_pci_eeh_clear_evt(phb->opal_id); /* needed? */
	}

	/*
	 * Make sure the bmc device is mapped to PE#0
	 *
	 * FIXME: who should be responsible for configuring this?
	 */
	phb4_raw_pe_map(phb, bmc_pd->bdfn, 0);

#ifdef USE_RAW_MODE
	/*
	 * Clear various in-memory table BARs and disable some errors that
	 * can escalate to a checkstop. We just want it to fence.
	 */
	phb4_raw_prep(phb);

	// There needs to be at least a one table level (and a table addr)
	// configured to make the TVE valid. So configure for one level
	// at a bogus address so we fence if there's an attempt to fetch
	// a TCE.
	phb->ops->map_pe_dma_window(phb,
					0, 0, // pe#0, window#0
					1, 0xffffffff, // lvls, tablebase,
					0x1000,        // table size
					0x10000);      // IO page size
#else
	phb->ops->map_pe_dma_window(phb,
					0,                      // pe#0
					0,                      // window#0
					2,                      // lvl
					(uint64_t) &base_table, // tablebase,
					0x1000,                 // table size
					0x10000);               // IO page size
#endif
	/*
	 * Setup TVE#1 for bypass so we've got the TVE value on hand when
	 * screwing around with cronus.
	 */
	phb->ops->map_pe_dma_window_real(phb,
					0, 1, // pe#0, window#1
					0x0, 0x80000000); // pci base, pci end

	/* enable memory accesses for memspace enable bits for this dev */
	for (pd = bmc_pd; pd; pd = pd->parent) {
		pci_cfg_write8(phb, pd->bdfn, PCI_CFG_CMD,
				PCI_CFG_CMD_BUS_MASTER_EN | PCI_CFG_CMD_MEM_EN);
	}

	/* good to go! */
	bmc_dma_ready = true;
}

void bmc_dma_teardown(void)
{
	bmc_dma_ready = false;

	/*
	 * these are largely redundant due to the reset below, but i'm keeping
	 * them here for the sake of illustration.
	 */
	phb4_raw_tce_unmap(bmc_phb);
	phb4_raw_pe_unmap(bmc_phb);

	prerror("Flushing IODA state...\n");

#ifdef USE_RAW_MODE
	/* force a complete phb reset before we hand off the PHB */
	bmc_phb->slot->ops.creset(bmc_phb->slot);
	pci_reset_phb(bmc_phb);
	prerror("PHB reset done\n");
	time_wait_ms(1000);
#else
	/* Purge the IODA state before we hand off to the OS */
	bmc_phb->ops->ioda_reset(bmc_phb, true);
#endif
}

void bmc_dma_poll(void)
{
	uint32_t old;
	uint32_t *a = (uint32_t *) 0x80;

	old = *a;
	sync();

	prerror("Polling for \"exit\" at 0x80...\n");

	while (memcmp(a, "exit", 4)) {
		time_wait_ms(100);
		sync();
	}

	*a = old;
	sync();
}

/* dumb test that sits around waiting for the BMC to DMA into its buffer */
void bmc_dma_wait(void)
{
	static char *dma_buf;

	if (!bmc_phb) {
		prerror("%s skipping no DMA capable BMC detected\n", __func__);
		return;
	}

	/* FIXME: leaks */
	if (!dma_buf)
		dma_buf = local_alloc(0, 64 * 1024, 64 * 1024);
	if (!dma_buf) {
		prerror("unable to allocate DMA buffer\n");
		return;
	}

	prerror("entering bmc-dma wait loop\n");

configure:
	bmc_dma_reinit();
	phb4_raw_tce_map(bmc_phb, (uint64_t) dma_buf, MBOX_DMA_BUS_ADDR, 64 * 1024);

	/* stay a while and listen */
	while (memcmp("exit", dma_buf, 4)) {
		prerror("BUF: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			dma_buf[0],
			dma_buf[1],
			dma_buf[2],
			dma_buf[3],
			dma_buf[4],
			dma_buf[5],
			dma_buf[6],
			dma_buf[7]);

		time_wait_ms(1000);

		/*
		 * Check if PE#0 is still active. This might be due to a freeze
		 * or due to a PHB fence so we have to look at the error detail.
		 *
		 * NB: Do this after the wait to stop it from flooding the
		 * console.
		 */
		if (!bmc_dma_ok()) {
			goto configure;
		}
	}

	// so we don't exit immediately on the next wait
	memset(dma_buf, 0, 65536);

	bmc_dma_teardown();
	prerror("exiting bmc-dma wait loop\n");
}

/*
 * In "direct" mode we only support one mapping at a time since
 * we use the same DMA buffer (at 0x10000000) in all cases.
 *
 * FIXME: sort that out at some point.
 */
static bool have_mapped;
void *mapped_addr;
uint32_t mapped_bytes;

int64_t bmc_dma_tce_unmap(void __unused *buf, size_t __unused size)
{
#ifndef USE_RAW_MODE
	/* zap our tables */
	memset(base_table, 0, sizeof(base_table));
	memset(tce_table, 0, sizeof(tce_table));
#endif
	/* FIXME: use the IODA op */
	phb4_raw_tce_unmap(bmc_phb);

	have_mapped = false;
	mapped_addr = 0;
	mapped_bytes = 0;

	return OPAL_SUCCESS;
}

int64_t bmc_dma_tce_map(void *buf, size_t size)
{
	uint64_t buf_addr = (uint64_t) buf;

	if (!bmc_phb)
		return OPAL_HARDWARE;
	/*
	 * We only allow for aligned DMA buffers. Otherwise we may allow the
	 * BMC to read/write something it shouldn't.
	 */
	if (buf_addr & 0xffff)
		return OPAL_UNSUPPORTED;
	if (size & 0xffff)
		return OPAL_UNSUPPORTED;

	if (have_mapped) {
		prerror("Can't map [%p,%p) due to exiting mapping of [%p,%p)\n",
			buf, buf + size - 1,
			mapped_addr, mapped_addr + mapped_bytes - 1);
		return OPAL_BUSY;
	}

#ifdef USE_RAW_MODE
	/* Limited to 256 sets until we make the tce mapper smarter */
	if (size > (256 * 64 * 1024)) {
		prerror("DMA mapping too long! %zx bytes\n", size);
		return OPAL_CONSTRAINED;
	}

	phb4_raw_tce_map(bmc_phb, (uint64_t) buf, MBOX_DMA_BUS_ADDR, size);
#else
	/* setup our tce table */
	tce_table_map((uint64_t) buf, MBOX_DMA_BUS_ADDR, size);
#endif

	mapped_addr = buf;
	mapped_bytes = size;

	return OPAL_SUCCESS;
}
