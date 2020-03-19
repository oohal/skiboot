// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
/*
 * Open Capi Memory Buffer chip
 *
 * Copyright 2020 IBM Corp.
 */


#define pr_fmt(fmt)	"OCMB: " fmt

#include <skiboot.h>
#include <xscom.h>
#include <device.h>
#include <ocmb.h>
#include <io.h>
#include <inttypes.h>

struct ocmb_range {
	uint64_t start;
	uint64_t end;
	uint64_t flags;

	/* flags come from hdat */
#define ACCESS_8B PPC_BIT(0)
#define ACCESS_4B PPC_BIT(1)
#define ACCESS_SIZE_MASK (ACCESS_8B | ACCESS_4B)
};

struct ocmb {
	struct fake_scom f;
	uint64_t base_addr;
	int range_count;
	struct ocmb_range ranges[];
};

static const struct ocmb_range *find_range(const struct ocmb *o, uint64_t offset)
{
	int i;

	for (i = 0; i < o->range_count; i++) {
		uint64_t start = o->ranges[i].start;
		uint64_t end = o->ranges[i].end;

		if (offset >= start && offset <= end)
			return &o->ranges[i];
	}

	return NULL;
}

static int64_t ocmb_fake_scom_write(struct fake_scom *f, uint32_t __unused chip_id,
				    uint64_t offset, uint64_t val)
{
	const struct ocmb *o = f->private;
	const struct ocmb_range *r;

	r = find_range(o, offset);
	if (!r) {
		prerror("no matching address range!\n");
		return OPAL_XSCOM_ADDR_ERROR;
	}

	switch (r->flags & ACCESS_SIZE_MASK) {
	case ACCESS_8B:
		if (offset & 0x7)
			return OPAL_XSCOM_ADDR_ERROR;
		out_be64((void *) offset, val);
		break;

	case ACCESS_4B:
		if (offset & 0x3)
			return OPAL_XSCOM_ADDR_ERROR;
		out_be32((void *) offset, val);
		break;
	default:
		prerror("bad flags? %llx\n", r->flags);
		return OPAL_XSCOM_ADDR_ERROR;
	}

	return OPAL_SUCCESS;
}

static int64_t ocmb_fake_scom_read(struct fake_scom *f, uint32_t chip_id __unused,
				   uint64_t offset, uint64_t *val)
{
	const struct ocmb *o = f->private;
	const struct ocmb_range *r = NULL;

	r = find_range(o, offset);
	if (!r) {
		prerror("no matching address range!\n");
		return OPAL_XSCOM_ADDR_ERROR;
	}


	switch (r->flags & ACCESS_SIZE_MASK) {
	case ACCESS_8B:
		if (offset & 0x7)
			return OPAL_XSCOM_ADDR_ERROR;
		*val = in_be64((void *) offset);
		break;

	case ACCESS_4B:
		if (offset & 0x3)
			return OPAL_XSCOM_ADDR_ERROR;
		*val = in_be32((void *) offset);
		break;
	default:
		prerror("bad flags? %llx\n", r->flags);
		return OPAL_XSCOM_ADDR_ERROR;
	}

	return OPAL_SUCCESS;
}

static bool ocmb_probe_one(struct dt_node *ocmb_node)
{
	uint64_t chip_id = dt_prop_get_u32(ocmb_node, "ibm,chip-id");
	struct dt_node *dn;
	struct ocmb *ocmb;
	int i = 0, num = 0;

	dt_for_each_child(ocmb_node, dn)
		num++;

	ocmb = zalloc(sizeof(*ocmb) + sizeof(*ocmb->ranges) * num);
	if (!ocmb)
		return false;

	ocmb->f.private = ocmb;
	ocmb->f.min_id = chip_id;
	ocmb->f.max_id = chip_id;
	ocmb->f.write = ocmb_fake_scom_write;
	ocmb->f.read = ocmb_fake_scom_read;
	ocmb->range_count = num;
	ocmb->base_addr = dt_prop_get_u64(ocmb_node, "base-addr");

	dt_for_each_child(ocmb_node, dn) {
		uint64_t start, size;

		start = dt_get_address(dn, 0, &size);

		ocmb->ranges[i].start = start;
		ocmb->ranges[i].end = start + size - 1;
		ocmb->ranges[i].flags = dt_prop_get_u64(dn, "flags");

		prlog(PR_DEBUG, "Added range:  %" PRIx64 " - [%llx - %llx]\n",
			chip_id, start, start + size - 1);

		i++;
	}

	if (xscom_register_special(&ocmb->f))
		prerror("error registienr fake socm\n");

	dt_add_property(ocmb_node, "scom-controller", NULL, 0);

	prerror("XXX: Added scom controller for %s\n", ocmb_node->name);

	return true;
}

void ocmb_init(void)
{
	struct dt_node *dn;

	dt_for_each_compatible(dt_root, dn, "ibm,explorer")
		ocmb_probe_one(dn);
}
