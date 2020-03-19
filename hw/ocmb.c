// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
/*
 * Open Capi Memory Buffer chip
 *
 * Copyright 2020 IBM Corp.
 */

#include <skiboot.h>
#include <xscom.h>
#include <device.h>
#include <ocmb.h>

static int64_t ocmb_fake_scom_write(struct fake_scom *f, uint32_t chip_id, uint32_t offset, uint64_t val)
{
    (void) f;
    (void) chip_id;
    (void) offset;
    (void) val;

    // TODO: crazy _xscom_write translation for ocmbs..

    return 0;
}

static int64_t ocmb_fake_scom_read(struct fake_scom *f, uint32_t chip_id, uint32_t offset, uint64_t *val)
{
    (void) f;
    (void) chip_id;
    (void) offset;
    (void) *val;

    // TODO: crazy _xscom_read translation for ocmbs..

    return 0;
}

static bool ocmb_register_fake_scom(uint32_t chip_id)
{
    struct fake_scom *f = zalloc(sizeof(*f));

    if (!f)
        return false;

    f->private = NULL;
    f->min_id = chip_id;
    f->max_id = chip_id;

    f->write = ocmb_fake_scom_write;
    f->read = ocmb_fake_scom_read;

    return true;
}

void ocmb_init(void)
{
    struct dt_node *cn;

    dt_for_each_compatible(dt_root, cn, "ibm,explorer") {

        uint32_t chip_id;
        chip_id = dt_prop_get_u32(cn, "ibm,chip-id");
        if (ocmb_register_fake_scom(chip_id)) {
            // TODO: Move ibm,scom-controller property_add here instead of hdat parsing
            (void) cn;
        }
    }
}