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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>

#include <compiler.h>

#include "../../ccan/list/list.c"

unsigned long top_of_ram = 16ULL * 1024 * 1024 * 1024;

void _prlog(int log_level __attribute__((unused)), const char* fmt, ...) __attribute__((format (printf, 2, 3)));

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif
#define prlog(l, f, ...) do { _prlog(l, pr_fmt(f), ##__VA_ARGS__); } while(0)

void _prlog(int log_level __attribute__((unused)), const char* fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        if (log_level < 7)
		vfprintf(stderr, fmt, ap);
        va_end(ap);
}

/*
 * Skiboot malloc stubs
 *
 * The actual prototypes for these are defined in mem_region-malloc.h,
 * but that file also #defines malloc, and friends so we don't pull that in
 * directly.
 */

#define DEFAULT_ALIGN __alignof__(long)

void *__memalign(size_t blocksize, size_t bytes, const char *location __unused);
void *__memalign(size_t blocksize, size_t bytes, const char *location __unused)
{
	return memalign(blocksize, bytes);
}

void *__malloc(size_t bytes, const char *location);
void *__malloc(size_t bytes, const char *location)
{
	return __memalign(DEFAULT_ALIGN, bytes, location);
}

void __free(void *p, const char *location __unused);
void __free(void *p, const char *location __unused)
{
	free(p);
}

void *__realloc(void *ptr, size_t size, const char *location __unused);
void *__realloc(void *ptr, size_t size, const char *location __unused)
{
	return realloc(ptr, size);
}

void *__zalloc(size_t bytes, const char *location);
void *__zalloc(size_t bytes, const char *location)
{
	void *p = __malloc(bytes, location);

	if (p)
		memset(p, 0, bytes);
	return p;
}

/* Add any stub functions required for linking here. */
static void stub_function(void)
{
	abort();
}

static void noop_function(void)
{
	return;
}

#define STUB(fnname) \
	void fnname(void) __attribute__((weak, alias ("stub_function")))

#define NOOP_STUB(fnname) \
	void fnname(void) __attribute__((weak, alias ("noop_function")))

STUB(op_display);
STUB(fsp_preload_lid);
STUB(fsp_wait_lid_loaded);
STUB(fsp_adjust_lid_side);
STUB(backtrace);
NOOP_STUB(mem_reserve_hw);
NOOP_STUB(early_uart_init);
