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
 * Console Log routines
 * Wraps libc and console lower level functions
 * does fancy-schmancy things like timestamps and priorities
 * Doesn't make waffles.
 */

#include "skiboot.h"
#include "unistd.h"
#include "stdio.h"
#include "console.h"
#include "timebase.h"
#include "ctype.h"
#include "opal-api.h"
#include "opal-internal.h"

static int vprlog(int log_level, const char *fmt, va_list ap)
{
	int count;
	char buffer[320];
	unsigned long tb = mftb();

	/* It's safe to return 0 when we "did" something here
	 * as only printf cares about how much we wrote, and
	 * if you change log_level to below PR_PRINTF then you
	 * get everything you deserve.
	 * By default, only PR_DEBUG and higher are stored in memory.
	 * PR_TRACE and PR_INSANE are for those having a bad day.
	 */
	if (log_level > (debug_descriptor.console_log_levels >> 4))
		return 0;

	/* if this changes parse_loghdr() needs to be updated too */
	count = snprintf(buffer, sizeof(buffer), "[%5lu.%09lu,%d] ",
			 tb_to_secs(tb), tb_remaining_nsecs(tb), log_level);
	count+= vsnprintf(buffer+count, sizeof(buffer)-count, fmt, ap);

	console_write(buffer, count);

	if (log_level <= PR_ERR)
		opal_set_pending_evt(OPAL_EVENT_LOG_PENDING);

	return count;
}

/*
 * This parses a log entry to find it's log-level. If the log entry does not
 * have a valid log header it'll return -1.
 */
static const char pattern[] = "[ddddd.ddddddddd,l] ";
int loghdr_size = sizeof(pattern) - 1;

/*
 * No prototype since this *should* be in console.c, but since it needs to be
 * kept in sync with the output format of prlog() it's better off here.
 */
int __parse_loghdr(const char *buf);
int __parse_loghdr(const char *buf)
{
	int log_level = -1;
	int i;

	for (i = 0; i < loghdr_size; i++) {
		int c = buf[i];

		/* terminated early */
		if (!c)
			return -i;

		switch (pattern[i]) {
		case 'd':
			if (!isdigit(c) && !isspace(c))
				return -2;
			break;
		case 'l':
			if (isdigit(c))
				log_level = c - '0';

			break;
		default:
			if (c != pattern[i])
				return -3;
		}
	}

	return log_level;
}


/* we don't return anything as what on earth are we going to do
 * if we actually fail to print a log message? Print a log message about it?
 * Callers shouldn't care, prlog and friends should do something generically
 * sane in such crazy situations.
 */
void _prlog(int log_level, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprlog(log_level, fmt, ap);
	va_end(ap);
}

int _printf(const char* fmt, ...)
{
	int count;
	va_list ap;

	va_start(ap, fmt);
	count = vprlog(PR_PRINTF, fmt, ap);
	va_end(ap);

	return count;
}
