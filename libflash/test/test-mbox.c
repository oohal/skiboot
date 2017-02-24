#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include <sys/mman.h> /* for mprotect() */
#include <sys/unistd.h> /* for usleep */

#include <libflash/libflash.h>
#include <libflash/libflash-priv.h>

#define zalloc(n) calloc(1, n)
#define __unused          __attribute__((unused))

#define __TEST__
void check_timers(bool __unused unused);
void check_timers(bool __unused unused)
{
	return;
}

void time_wait_ms(unsigned long ms);
void time_wait_ms(unsigned long ms)
{
	usleep(ms * 1000);
}

/* skiboot stubs */
unsigned long mftb(void);
unsigned long mftb(void)
{
	return 42;
}
unsigned long tb_hz = 512000000ul;

#include "../libflash.c"
#include "../mbox-flash.c"
#include "../ecc.c"

#undef pr_fmt
#define pr_fmt(fmt) "MBOX-PROXY: " fmt

void _prlog(int __unused log_level, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

/* client interface */

#include "../../include/lpc-mbox.h"

typedef void (*mbox_cb)(struct bmc_mbox_msg *msg, void *priv);

struct {
	mbox_cb fn;
	void *cb_data;
} mbox_data;

int bmc_mbox_register_callback(mbox_cb handler, void *drv_data)
{
	mbox_data.fn = handler;
	mbox_data.cb_data = drv_data;

	return 0;
}

/* server side implementation */

enum win_type {
	WIN_CLOSED,
	WIN_READ,
	WIN_WRITE
};

#define BLOCK_SIZE 4096

#define LPC_BLOCKS 256
#define LPC_SIZE (LPC_BLOCKS * BLOCK_SIZE)

#define BLOCK_OFF(x) (BLOCK_SIZE * (x))
#define BLOCK_PTR(x) ((void *) server_state.lpc_base + BLOCK_OFF(x))

static struct {
	int api;

	void *lpc_base;
	size_t lpc_size;

	uint32_t block_size; /* size of block in bytes */

	uint16_t def_read_win;  /* default window size in blocks */
	uint16_t def_write_win;

	uint16_t max_read_win; /* max window size in blocks */
	uint16_t max_write_win;

	enum win_type win_type;
} server_state;


/* skiboot test stubs */
int64_t lpc_read(enum OpalLPCAddressType __unused addr_type, uint32_t addr,
		 uint32_t *data, uint32_t sz)
{
	memcpy(data, server_state.lpc_base + addr, sz);
	return 0;
}

int64_t lpc_write(enum OpalLPCAddressType __unused addr_type, uint32_t addr,
		  uint32_t data, uint32_t sz)
{
	memcpy(server_state.lpc_base + addr, &data, sz);
	return 0;
}
/* accessor junk */

static void bmc_put_u16(struct bmc_mbox_msg *msg, int offset, uint16_t data)
{
	msg->args[offset + 0] = data & 0xff;
	msg->args[offset + 1] = data >> 8;
}

static void bmc_put_u32(struct bmc_mbox_msg *msg, int offset, uint32_t data)
{
	msg->args[offset + 0] = (data)       & 0xff;
	msg->args[offset + 1] = (data >>  8) & 0xff;
	msg->args[offset + 2] = (data >> 16) & 0xff;
	msg->args[offset + 3] = (data >> 24) & 0xff;
}

#if 0
static u16 bmc_get_u32(struct bmc_mbox_msg *msg, int offset)
{
	u32 data = 0;

	data |= msg->args[offset + 0];
	data |= msg->args[offset + 1] << 8;
	data |= msg->args[offset + 2] << 16;
	data |= msg->args[offset + 3] << 24;

	return data;
}
#endif

static u16 bmc_get_u16(struct bmc_mbox_msg *msg, int offset)
{
	u16 data = 0;

	data |= msg->args[offset + 0];
	data |= msg->args[offset + 1] << 8;

	return data;
}

#define min(x,y) ((x) > (y) ? y : x)

static void close_window(void)
{
	server_state.win_type = WIN_CLOSED;
	mprotect(BLOCK_PTR(0), LPC_SIZE, PROT_NONE);
	prlog(PR_INFO, "window closed\n");
}

static int open_window(struct bmc_mbox_msg *msg, bool write, u32 offset, u32 size)
{
	int max_size = server_state.max_read_win;
	enum win_type type = WIN_READ;
	int prot = PROT_READ;

	assert(server_state.win_type == WIN_CLOSED);

	if (write) {
		max_size = server_state.max_write_win;
		prot |= PROT_WRITE;
		type = WIN_WRITE;
	}

	/* Use the default size if zero size is set */
	if (!size)
		size = server_state.def_write_win;

	prlog(PR_INFO, "Opening range %#.8x, %#.8x for %s\n",
		BLOCK_SIZE * (offset), BLOCK_SIZE * (offset + size) - 1,
		write ? "writing" : "reading");

	/* XXX: Document this behaviour */
	if ((size + offset) > LPC_BLOCKS) {
		prlog(PR_INFO, "tried to open beyond end of flash\n");
		return MBOX_R_PARAM_ERROR;
	}

	/* XXX: should we do this before or after checking for errors?
	 * 	Doing it afterwards ensures consistency between
	 * 	implementations
	 */
	size = min(size, max_size);

	mprotect(BLOCK_PTR(offset), size * BLOCK_SIZE, prot);
	server_state.win_type = type;

	memset(msg->args, 0, sizeof(msg->args));
	bmc_put_u16(msg, 0, offset);
	bmc_put_u16(msg, 2, size);

	return MBOX_R_SUCCESS;
}

int bmc_mbox_enqueue(struct bmc_mbox_msg *msg)
{
	/*
	 * FIXME: should we be using the same storage for message
	 *        and response?
	 */
	int rc = MBOX_R_SUCCESS;
	uint32_t start, size;

	switch (msg->command) {
		case MBOX_C_RESET_STATE:
			prlog(PR_INFO, "reset requested\n");
			rc = open_window(msg, false, 0, LPC_BLOCKS);
			memset(msg->args, 0, sizeof(msg->args));
			break;

		case MBOX_C_GET_MBOX_INFO:
			prlog(PR_INFO, "get info: api=%d, def_read=%d, def_write=%d, block=%d\n",
				server_state.api, server_state.def_read_win,
				server_state.def_write_win, server_state.block_size);
			msg->args[0] = server_state.api;
			bmc_put_u16(msg, 1, server_state.def_read_win);
			bmc_put_u16(msg, 3, server_state.def_write_win);
			msg->args[5] = ilog2(server_state.block_size);
			break;

		case MBOX_C_GET_FLASH_INFO:
			prlog(PR_INFO, "get flash info: %u, %u\n", LPC_SIZE, BLOCK_SIZE);
			bmc_put_u32(msg, 0, LPC_SIZE);
			bmc_put_u32(msg, 4, BLOCK_SIZE); /* FIXME: Make the erase block larger */
			break;

		case MBOX_C_CREATE_READ_WINDOW:
			start = bmc_get_u16(msg, 0);
			size = bmc_get_u16(msg, 2);
			close_window();
			rc = open_window(msg, false, start, size);
			break;

		case MBOX_C_CLOSE_WINDOW:
			close_window();
			break;

		case MBOX_C_CREATE_WRITE_WINDOW:
			/*
			 * FIXME: we want to only open write windows
			 * writes should only be done to the write
			 * area. Fix this up at some point and make
			 * the mark/flush copy it to the actual backing
			 * memory.
			 */
			start = bmc_get_u16(msg, 0);
			size = bmc_get_u16(msg, 2);
			close_window();
			rc = open_window(msg, true, start, size);
			break;

		/* TODO: make these do something */
		case MBOX_C_WRITE_FLUSH:
		case MBOX_C_MARK_WRITE_DIRTY:
			break;

		case MBOX_C_BMC_EVENT_ACK:
			/*
			 * Clear any BMC notifier flags.
			 *
			 * XXX: Should add some kind of reset-injection
			 *      to see how that's handled.
			 */
			msg->bmc &= ~msg->args[0];
	}

	prerror("command response = %d\n", rc);
	msg->response = rc;

	/* now that we've handled the message, holla-back */
	mbox_data.fn(msg, mbox_data.cb_data);

	return 0;
}

#define ERR(...) FL_DBG(__VA_ARGS__)

static int run_flash_test(struct blocklevel_device *bl, void *sim_image,
		uint64_t sim_image_sz)
{
	uint64_t total_size;
	uint32_t erase_granule;
	const char *name;
	uint16_t *test;
	struct ecc64 *ecc_test;
//	uint64_t *test64;
	int i, rc;

	memset(sim_image, 0xff, sim_image_sz);
	test = malloc(0x10000 * 2);

	rc = bl->get_info(bl, &name, &total_size, &erase_granule);
	if (rc) {
		ERR("get_info failed with err %d\n", rc);
		return 1;
	}

	/* Make up a test pattern */
	for (i=0; i<0x10000;i++)
		test[i] = cpu_to_be16(i);

	/* Write 64k of stuff at 0 and at 128k */
	printf("Writing test patterns...\n");
	bl->write(bl, 0, test, 0x10000);
	bl->write(bl, 0x20000, test, 0x10000);

	/* Write "Hello world" straddling the 64k boundary */
#define HW "Hello World"
	printf("Writing test string...\n");
	bl->write(bl, 0xfffc, HW, sizeof(HW));

	/* Check result */
	if (memcmp(sim_image + 0xfffc, HW, sizeof(HW))) {
		abort();
		ERR("Test string mismatch !\n");
		return 1;
	}

	printf("Test string pass\n");
	if (memcmp(sim_image, test, 0xfffc)) {
		ERR("Test pattern mismatch !\n");
		return 1;
	}
	printf("Test pattern pass\n");

	ecc_test = (struct ecc64 *)sim_image;

#if 0
	/*
	 * C+P from the flash test, but the generic blocklayer interface doesn't expose
	 * this ECC write interface. Nothing inside skiboot uses it either...
	 */

	test64 = (uint64_t *)test;

	printf("Test ECC interfaces\n");
	flash_smart_write_corrected(bl, 0, test, 0x10000, 1);
	for (i = 0; i < 0x10000 / sizeof(*ecc_test); i++) {
		if (test64[i] != ecc_test[i].data) {
			ERR("flash_smart_write_corrected() pattern missmatch at %d: 0x%016lx vs 0x%016lx\n",
					i, test64[i], ecc_test[i].data);
			exit(1);
		}
		if (ecc_test[i].ecc != eccgenerate(be64toh(test64[i]))) {
			ERR("ECCs don't match 0x%02x vs 0x%02x\n", ecc_test[i].ecc, eccgenerate(test64[i]));
			exit(1);
		}
	}
	printf("Test ECC interface pass\n");
#endif

	printf("Test erase\n");
	if (bl->erase(bl, 0, 0x10000) != 0) {
		ERR("flash_erase didn't return 0\n");
		exit(1);
	}

	/* HACK: until we implement erase properly */
	memset(sim_image, 0xff, sim_image_sz);

	for (i = 0; i < 0x10000 / sizeof(*ecc_test); i++) {
		uint8_t zero = 0;
		if (ecc_test[i].data != 0xFFFFFFFFFFFFFFFF) {
			ERR("Data not properly cleared at %d\n", i);
			exit(1);
		}
		rc = bl->write(bl, i * sizeof(*ecc_test) + 8, &zero, 1);
		if (rc || ecc_test[i].ecc != 0) {
			ERR("Cleared data not correctly ECCed: 0x%02x (0x%016lx) expecting 0 at %d\n", ecc_test[i].ecc, ecc_test[i].data, i);
			exit(1);
		}
	}
	printf("Test ECC erase pass\n");

	return 0;
}

int main(void)
{
	char *buffer = malloc(LPC_SIZE);

	struct blocklevel_device *bl;

	libflash_debug = true;

	/* setup server */
	server_state.block_size = BLOCK_SIZE;
	server_state.lpc_size = LPC_SIZE;
	server_state.lpc_base = buffer;

	server_state.def_read_win = 16;
	server_state.def_write_win = 16;

	server_state.max_read_win = LPC_BLOCKS;
	server_state.max_write_win = LPC_BLOCKS; /* XXX: fix this */
	server_state.win_type = WIN_CLOSED; /* FIXME: should be the reset state */

	/* run test */
	mbox_flash_init(&bl);
	run_flash_test(bl, buffer, LPC_SIZE);
	mbox_flash_exit(bl);

	return 0;
}


