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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#define __TEST__

unsigned long tb_hz = 512000000;

static inline unsigned long mftb(void)
{
	return 42;
}

#define pr_fmt(f) "PREFIX: " f
#include "../../libc/include/stdio.h"
#include "../console-log.c"
#include "../../libc/stdio/snprintf.c"
#include "../../libc/stdio/vsnprintf.c"

struct debug_descriptor debug_descriptor;
char console_buffer[4096];

ssize_t console_write(const void *buf, size_t count)
{
	memcpy(console_buffer, buf, count);
	return count;
}

int main(void)
{
	debug_descriptor.console_log_levels = 0x75;

	/* check basic functionality */
	prlog(PR_EMERG, "Hello World");
	assert(strcmp(console_buffer, "[    0.000000042,0] PREFIX: Hello World") == 0);

	memset(console_buffer, 0, sizeof(console_buffer));

	/* Below log level is filtered */
	prlog(PR_TRACE, "Hello World");
	assert(console_buffer[0] == 0);

	/* check printf */
	printf("Hello World");
	assert(strcmp(console_buffer, "[    0.000000042,5] PREFIX: Hello World") == 0);

	return 0;
}
