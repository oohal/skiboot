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

#ifndef __GPIO_H__
#define __GPIO_H__

#include <stdint.h>
#include <lock.h>

struct gpio {
	// gpio internal elements
	struct list_node list;
	struct lock lock;

	// driver provided elements
	struct dt_node *n;
	uint32_t chip_id;
	const char *name;
	const struct gpio_ops *ops;
};

struct gpio_ops {
	int64_t (*set)(struct gpio *, int state);
	int64_t (*get)(struct gpio *, int *state);

	// XXX: We probably should have some kind of explict async awareness
	bool slow; // true if using these ops require I2C operations or whatnot
};

/*  */
#define ANY_CHIP (~0u)
void gpio_add(struct gpio *g);
struct gpio *gpio_find(uint32_t chip_id, const char *name);

/* user facing API */

int64_t gpio_set(struct gpio *g, int state);
int64_t gpio_get(struct gpio *g, int *state);

/*
 * TODO: probably want explicitly sync versions of these that'll wait for the
 * 	 completion
 */

#endif
