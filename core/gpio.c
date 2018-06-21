/* Copyright 2018 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define pr_fmt(fmt) "GPIO: " fmt

#include <skiboot.h>
#include <opal.h>
#include <gpio.h>

#include <ccan/list/list.h>

static struct lock gpio_list_lock = LOCK_UNLOCKED;
static LIST_HEAD(gpio_list);

void gpio_add(struct gpio *g)
{
	lock(&gpio_list_lock);
	list_add(&gpio_list, &g->list);
	unlock(&gpio_list_lock);
}

struct gpio *gpio_find(uint32_t chip_id, const char *name)
{
	struct gpio *g;

	lock(&gpio_list_lock); // necessary?

	list_for_each(&gpio_list, g, list) {
		if (!streq(g->name, name))
			continue;

		if (g->chip_id == chip_id || chip_id == ANY_CHIP)
			break;
	}

	unlock(&gpio_list_lock);

	if (!g)
		prlog(PR_DEBUG, "Unable to find %s on %u\n", name, chip_id);

	return g;
}

int64_t gpio_set(struct gpio *g, int state)
{
	uint64_t rc;

	if (!g->ops->set)
		return OPAL_UNSUPPORTED;

	lock(&g->lock);
	rc = g->ops->set(g, state);
	unlock(&g->lock);

	return rc;
}

int64_t gpio_get(struct gpio *g, int *state)
{
	uint64_t rc;

	if (!g->ops->set)
		return OPAL_UNSUPPORTED;

	lock(&g->lock);
	rc = g->ops->get(g, state);
	unlock(&g->lock);

	return rc;
}

