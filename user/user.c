#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

void do_it(void *);

int printf(const char *fmt, ...)
{
	int len;
	va_list args;

	va_start(args, fmt);
	len = vprintf(fmt, args);
	va_end(args);

	return len;
}

void userspace_console_write(const char *l, int length);
void userspace_console_write(const char *l, int length)
{
	fwrite(l, 1, length, stderr);
}

int memfd;
#include <errno.h>

void map_one(uint64_t base_addr, uint64_t size)
{
	// mask off the top two bits to fit in our virt addr space
	uint64_t map_at = base_addr & ~0x3000000000000;
	void *res;

	size = (size + 65535) & (~(65536 - 1)); // size gets aligned up
	map_at = map_at & (~(65536 - 1)); // base addr gets aligned down
	base_addr = base_addr & (~(65536 - 1)); // base addr gets aligned down

	res = mmap((void *)map_at, size, PROT_READ | PROT_WRITE,
		MAP_FIXED | MAP_SHARED, memfd, base_addr);

	if (res == MAP_FAILED) {
		char buf[512];
		exit(2);
	}

//	return res;
}

char fdt_buf[128 * 1024];

int main(void)
{
	FILE *fdt_fd = fopen("/sys/firmware/fdt", "r");
	void *stacks_base = (void *) 0x30000000;
	// skiboot area up to the end of the stack area, combination of
	// the firmware-code, firmware-data, firmware-heap, and firmware-stacks reservations
	// beyond those are the reservations made by local_alloc(), such as the PHB in memory
	// tables which we need to be able to map into our address space.
	size_t stack_size = 0x01c80000;
	memfd = open("/dev/mem", O_RDWR);

//	size_t stack_size = 64 * 1024 * 1024; // map the whole skiboot area, this is probs overkill

	void *res = mmap(stacks_base, stack_size, PROT_READ| PROT_WRITE,
			 MAP_FIXED | MAP_ANON | MAP_PRIVATE, 0, 0);

	if (memfd == -1) {
		perror("opening /dev/mem");
		return 1;
	}

	if(!fdt_fd) {
		perror("fuck");
		return 1;
	}

	fread(fdt_buf, 1, sizeof(fdt_buf), fdt_fd);


	fprintf(stderr, "mmap: %p\n", res);
	perror("mmap");
	fprintf(stderr, "hello world!\n");

	fflush(stderr);
	fflush(stdout);

	do_it(fdt_buf);

	fprintf(stderr, "goodbye world!\n");

	return 0;
}
