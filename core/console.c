/* Copyright 2013-2014 IBM Corp.
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

/*
 * Console IO routine for use by libc
 *
 * fd is the classic posix 0,1,2 (stdin, stdout, stderr)
 */
#include <skiboot.h>
#include <unistd.h>
#include <console.h>
#include <opal.h>
#include <device.h>
#include <processor.h>
#include <cpu.h>

/*
 * ring buffer pointers
 *
 * con_in = offset the next character will be written to
 * con_out = offset of the next character to be flushed
 *
 * when con_in == con_out there is no data to be flushed
 */

static char *con_buf = (char *)INMEM_CON_START;
static size_t con_in;
static size_t con_out;
static bool con_wrapped;

/* Internal console driver ops */
static struct con_ops *con_driver;

/* External (OPAL) console driver ops */
static struct opal_con_ops *opal_con_driver = &dummy_opal_con;

static struct lock con_lock = LOCK_UNLOCKED;

/* This is mapped via TCEs so we keep it alone in a page */
struct memcons memcons __section(".data.memcons") = {
	.magic		= MEMCONS_MAGIC,
	.obuf_phys	= INMEM_CON_START,
	.ibuf_phys	= INMEM_CON_START + INMEM_CON_OUT_LEN,
	.obuf_size	= INMEM_CON_OUT_LEN,
	.ibuf_size	= INMEM_CON_IN_LEN,
};

static bool dummy_console_enabled(void)
{
#ifdef FORCE_DUMMY_CONSOLE
	return true;
#else
	return dt_has_node_property(dt_chosen,
				    "sapphire,enable-dummy-console", NULL);
#endif
}

/*
 * Helper function for adding /ibm,opal/consoles/serial@<xyz> nodes
 */
struct dt_node *add_opal_console_node(int index, const char *type,
	uint32_t write_buffer_size)
{
	struct dt_node *con, *consoles;
	char buffer[32];

	consoles = dt_find_by_name(opal_node, "consoles");
	if (!consoles) {
		consoles = dt_new(opal_node, "consoles");
		assert(consoles);
		dt_add_property_cells(consoles, "#address-cells", 1);
		dt_add_property_cells(consoles, "#size-cells", 0);
	}

	con = dt_new_addr(consoles, "serial", index);
	assert(con);

	snprintf(buffer, sizeof(buffer), "ibm,opal-console-%s", type);
	dt_add_property_string(con, "compatible", buffer);

	dt_add_property_cells(con, "#write-buffer-size", write_buffer_size);
	dt_add_property_cells(con, "reg", index);
	dt_add_property_string(con, "device_type", "serial");

	return con;
}

void clear_console(void)
{
	memset(con_buf, 0, INMEM_CON_LEN);
}

static inline int conbuf_get(int offset)
{
	offset %= memcons.obuf_size;

	if (offset == con_in)
		return -1;

	return con_buf[offset];
}

extern int loghdr_size;
extern int __parse_loghdr(const char *buffer);
static int parse_loghdr(int start)
{
	char buffer[32];
	int i, c;

	for (i = 0; i < loghdr_size; i++) {
		c = conbuf_get(start + i);
		if (c < 0 || c == '\n')
			return -1;

		buffer[i] = c;
	}

	/*
	 * we use this little wrapper around the "real" parser
	 * so we can keep the inmem console in here and limit
	 * exposure to the ringbuffer to inside of console.c
	 */
	return __parse_loghdr(buffer);
}

static int find_eol(int start)
{
	int c, len = 0;

	while (1) {
		c = conbuf_get(start + len);
		if (c < 0)
			break;

		/* the newline char need to be included in the flushed string */
		len++;
		if (c == '\n')
			break;
	}

	return len;
}

/*
 * Flush the console buffer into the driver. Returns true
 * if there is more to go, but that only happens when the
 * underlying driver failed so don't call it again.
 */
static bool __flush_console(void)
{
	int flush_lvl = debug_descriptor.console_log_levels & 0xf;
	struct cpu_thread *cpu = this_cpu();
	static bool in_flush;

	/* Is there anything to flush ? Bail out early if not */
	if (con_in == con_out || !con_driver)
		return false;

	/*
	 * Console flushing is suspended on this CPU, typically because
	 * some critical locks are held that would potentially cause a
	 * flush to deadlock
	 */
	if (cpu->con_suspend) {
		cpu->con_need_flush = true;
		return false;
	}
	cpu->con_need_flush = false;

	/*
	 * We must call the underlying driver with the console lock
	 * dropped otherwise we get some deadlocks if anything down
	 * that path tries to printf() something.
	 *
	 * So instead what we do is we keep a static in_flush flag
	 * set/released with the lock held, which is used to prevent
	 * concurrent attempts at flushing the same chunk of buffer
	 * by other processors.
	 */
	if (in_flush)
		return false;

	in_flush = true;

	do {
		int start, req, len, log_lvl;

		start = con_out;

		/*
		 * NB: Input that does not start with a valid log skiboot
		 * log header is always flushed. This can happen due to
		 * writes into the dummy OPAL console or because a line
		 * was only partially flushed.
		 */
		log_lvl = parse_loghdr(start);
		req = find_eol(start);

		if (log_lvl <= flush_lvl) {
			/*
			 * It this messages crosses the ring buffer edge then
			 * truncate and write the rest on the next iteration.
			 */
			int end = (start + req) % memcons.obuf_size;
			if (end < start)
				req = memcons.obuf_size - start;

			unlock(&con_lock);
			len = con_driver->write(con_buf + con_out, req);
			lock(&con_lock);
		} else
			len = req;

		con_out += len;
		con_out %= memcons.obuf_size;

		if (len < req)
			goto bail;

	} while (con_out != con_in);

bail:
	in_flush = false;
	return con_out != con_in;
}

bool flush_console(void)
{
	bool ret;

	lock(&con_lock);
	ret = __flush_console();
	unlock(&con_lock);

	return ret;
}

static void inmem_write(char c)
{
	uint32_t opos;

	if (!c)
		return;
	con_buf[con_in++] = c;
	if (con_in >= INMEM_CON_OUT_LEN) {
		con_in = 0;
		con_wrapped = true;
	}

	/*
	 * We must always re-generate memcons.out_pos because
	 * under some circumstances, the console script will
	 * use a broken putmemproc that does RMW on the full
	 * 8 bytes containing out_pos and in_prod, thus corrupting
	 * out_pos
	 */
	opos = con_in;
	if (con_wrapped)
		opos |= MEMCONS_OUT_POS_WRAP;
	lwsync();
	memcons.out_pos = opos;

	/* If head reaches tail, push tail around & drop chars */
	if (con_in == con_out)
		con_out = (con_in + 1) % INMEM_CON_OUT_LEN;
}

static size_t inmem_read(char *buf, size_t req)
{
	size_t read = 0;
	char *ibuf = (char *)memcons.ibuf_phys;

	while (req && memcons.in_prod != memcons.in_cons) {
		*(buf++) = ibuf[memcons.in_cons];
		lwsync();
		memcons.in_cons = (memcons.in_cons + 1) % INMEM_CON_IN_LEN;
		req--;
		read++;
	}
	return read;
}

static void write_char(char c)
{
#ifdef MAMBO_DEBUG_CONSOLE
	mambo_console_write(&c, 1);
#endif
	inmem_write(c);
}

ssize_t console_write(const void *buf, size_t count)
{
	/* We use recursive locking here as we can get called
	 * from fairly deep debug path
	 */
	bool need_unlock = lock_recursive(&con_lock);
	const char *cbuf = buf;

	while(count--) {
		char c = *(cbuf++);
		if (c == '\n')
			write_char('\r');
		write_char(c);
	}

	__flush_console();

	if (need_unlock)
		unlock(&con_lock);

	return count;
}

ssize_t write(int fd __unused, const void *buf, size_t count)
{
	return console_write(buf, count);
}

ssize_t read(int fd __unused, void *buf, size_t req_count)
{
	bool need_unlock = lock_recursive(&con_lock);
	size_t count = 0;

	if (con_driver && con_driver->read)
		count = con_driver->read(buf, req_count);
	if (!count)
		count = inmem_read(buf, req_count);
	if (need_unlock)
		unlock(&con_lock);
	return count;
}

/* Helper function to perform a full synchronous flush */
void console_complete_flush(void)
{
	/*
	 * Using term 0 here is a dumb hack that works because the UART
	 * only has term 0 and the FSP doesn't have an explicit flush method.
	 */
	int64_t ret = opal_con_driver->flush(0);

	if (ret == OPAL_UNSUPPORTED || ret == OPAL_PARAMETER)
		return;

	while (ret != OPAL_SUCCESS) {
		ret = opal_con_driver->flush(0);
	}
}

/*
 * set_console()
 *
 * This sets the driver used internally by Skiboot. This is different to the
 * OPAL console driver.
 */
void set_console(struct con_ops *driver)
{
	con_driver = driver;
	if (driver)
		flush_console();
}

/*
 * set_opal_console()
 *
 * Configure the console driver to handle the console provided by the OPAL API.
 * They are different to the above in that they are typically buffered, and used
 * by the host OS rather than skiboot.
 */
static bool opal_cons_init = false;

void set_opal_console(struct opal_con_ops *driver)
{
	assert(!opal_cons_init);
	opal_con_driver = driver;
}

void init_opal_console(void)
{
	assert(!opal_cons_init);
	opal_cons_init = true;

	if (dummy_console_enabled() && opal_con_driver != &dummy_opal_con) {
		prlog(PR_WARNING, "OPAL: Dummy console forced, %s ignored\n",
		      opal_con_driver->name);

		opal_con_driver = &dummy_opal_con;
	}

	prlog(PR_NOTICE, "OPAL: Using %s\n", opal_con_driver->name);

	if (opal_con_driver->init)
		opal_con_driver->init();

	opal_register(OPAL_CONSOLE_READ, opal_con_driver->read, 3);
	opal_register(OPAL_CONSOLE_WRITE, opal_con_driver->write, 3);
	opal_register(OPAL_CONSOLE_FLUSH, opal_con_driver->flush, 1);
	opal_register(OPAL_CONSOLE_WRITE_BUFFER_SPACE,
			opal_con_driver->space, 2);
}

void memcons_add_properties(void)
{
	dt_add_property_u64(opal_node, "ibm,opal-memcons", (u64) &memcons);
}

/*
 * The default OPAL console.
 *
 * In the absence of a "real" OPAL console driver we handle the OPAL_CONSOLE_*
 * calls by writing into the skiboot log buffer. Reads are a little more
 * complicated since they can come from the in-memory console (BML) or from the
 * internal skiboot console driver.
 */
static int64_t dummy_console_write(int64_t term_number, int64_t *length,
				   const uint8_t *buffer)
{
	if (term_number != 0)
		return OPAL_PARAMETER;

	if (!opal_addr_valid(length) || !opal_addr_valid(buffer))
		return OPAL_PARAMETER;

	write(0, buffer, *length);

	return OPAL_SUCCESS;
}

static int64_t dummy_console_write_buffer_space(int64_t term_number,
						int64_t *length)
{
	if (term_number != 0)
		return OPAL_PARAMETER;

	if (!opal_addr_valid(length))
		return OPAL_PARAMETER;

	if (length)
		*length = INMEM_CON_OUT_LEN;

	return OPAL_SUCCESS;
}

static int64_t dummy_console_read(int64_t term_number, int64_t *length,
				  uint8_t *buffer)
{
	if (term_number != 0)
		return OPAL_PARAMETER;

	if (!opal_addr_valid(length) || !opal_addr_valid(buffer))
		return OPAL_PARAMETER;

	*length = read(0, buffer, *length);
	opal_update_pending_evt(OPAL_EVENT_CONSOLE_INPUT, 0);

	return OPAL_SUCCESS;
}

static int64_t dummy_console_flush(int64_t term_number __unused)
{
	return OPAL_UNSUPPORTED;
}

static void dummy_console_poll(void *data __unused)
{
	bool has_data = false;

	lock(&con_lock);
	if (con_driver && con_driver->poll_read)
		has_data = con_driver->poll_read();
	if (memcons.in_prod != memcons.in_cons)
		has_data = true;
	if (has_data)
		opal_update_pending_evt(OPAL_EVENT_CONSOLE_INPUT,
					OPAL_EVENT_CONSOLE_INPUT);
	else
		opal_update_pending_evt(OPAL_EVENT_CONSOLE_INPUT, 0);
	unlock(&con_lock);
}

void dummy_console_add_nodes(void)
{
	struct dt_property *p;

	add_opal_console_node(0, "raw", memcons.obuf_size);

	/* Mambo might have left a crap one, clear it */
	p = __dt_find_property(dt_chosen, "linux,stdout-path");
	if (p)
		dt_del_property(dt_chosen, p);

	dt_add_property_string(dt_chosen, "linux,stdout-path",
			       "/ibm,opal/consoles/serial@0");

	opal_add_poller(dummy_console_poll, NULL);
}

struct opal_con_ops dummy_opal_con = {
	.name = "Dummy Console",
	.init = dummy_console_add_nodes,
	.read = dummy_console_read,
	.write = dummy_console_write,
	.space = dummy_console_write_buffer_space,
	.flush = dummy_console_flush,
};
