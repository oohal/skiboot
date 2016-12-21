/* Copyright 2016 IBM Corp.
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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/types.h>

#define __TEST__
#define _printf printf

#if 0
#define smt_very_low()
#define smt_low()
#define smt_medium()
#define smt_high()
#endif
#define lwsync()

/* fake out the timebase sett */
unsigned long tb_hz = 512000000;

static inline unsigned long mftb(void)
{
	return 0;
}

int _printf(const char *fmt, ...);

static struct debug_descriptor debug_descriptor;

#include "../console.c"
#include "../console-log.c"

int flush_buffer_offset = 0;
static bool flushed_to_drivers = false;
static char flush_buffer[65536];
static char console_buffer[65536];

/*
 * Inside skiboot we have our own write() function that breaks
 * the normal stdlib write(). We can use writev() instead though
 */
static size_t unix_write(int fd, const char *buf, size_t len)
{
	struct iovec iov = {(void *)buf, len};

	return writev(fd, &iov, 1);
}

/* For debugging I usually hack in some mambo_printfs
 * this implements the backend for the test */
size_t mambo_console_write(const char *buf, size_t count)
{
	return unix_write(2, buf, count);
}

static size_t flush_write(const char *buf, size_t len)
{
	memcpy(flush_buffer + flush_buffer_offset, buf, len);

	flush_buffer_offset += len;
	flush_buffer_offset %= sizeof(flush_buffer);

	return len;
}

static struct con_ops unix_con = {
	.write = flush_write,
};

static void populate_console(int mem_lvl, int flush_lvl)
{
	int i;

	memset(console_buffer, 0, sizeof(console_buffer));
	con_in = 0;
	con_out = 0;

	debug_descriptor.console_log_levels = (mem_lvl << 4) | flush_lvl;

	for (i = 0; i < 20; i++)
		prlog(i % (PR_INSANE + 1), "Hello World! %d\r\n", i);
}

int main(void)
{
	int ret;

	this_cpu()->con_suspend = false;
	set_console(&unix_con);

	/* fix up the console globals */
	flushed_to_drivers = false;
	con_buf = console_buffer;
	memcons.obuf_phys = (uint64_t) console_buffer;
	memcons.obuf_size = sizeof(console_buffer);

	populate_console(PR_INSANE, PR_NOTICE);
	__flush_console(true);

	/*
	 * Refill the console buffer dropping everything above PR_NOTICE
	 * The contents of the flushed and the refilled buffer should
	 * be identical.
	 */
	set_console(NULL); /* prevent writes into flush_buffer */
	populate_console(PR_NOTICE, 0);

	ret = memcmp(flush_buffer, console_buffer, con_in);
	if (ret) {
		unix_write(2, "=========\n", sizeof("=========\n") - 1);
		unix_write(2, console_buffer, strlen(console_buffer));
		unix_write(2, "=========\n", sizeof("=========\n") - 1);
		unix_write(2, flush_buffer, strlen(flush_buffer));
	}

	return ret;
}
