// SPDX-License-Identifier: Apache-2.0
#ifndef __OPAL_PRD_NVDIMM_H__
#define __OPAL_PRD_NVDIMM_H__

#include <stdint.h>

struct nvdimm_msg {
	uint32_t type;
	uint32_t unused; /* just to keep each nvdimm msg on a seperate line in hexdump */
	uint32_t d1;
	uint32_t d2;
};

#define NVDIMM_STATUS_PROTECTED 0x1

enum nvdimm_query_types {
	/* returns a NVDIMM_REPLY_CHIP
	 *
	 * node ID is the linux NUMA node id corresponding to that chip.
	 */
	NVDIMM_QUERY_CHIP      = 0x10,  /* d1: node_id, d2: ignored */
	NVDIMM_QUERY_CHIP_ALL  = 0x11,  /* d1: ignored, d2: ignored */
	NVDIMM_QUERY_CHIP_SET  = 0x1f,  /* d1: node_id, d2: new_status XXX TEST COMMAND */

	/* returns a NVDIMM_REPLY_BDEV
	 *
	 * bdev_id = (bdev_major << 16) | bdev_minor
	 */
	NVDIMM_QUERY_BDEV       = 0x20,  /* d1: bdev_id, d2: ignored */
	NVDIMM_QUERY_BDEV_ALL   = 0x21,  /* d1: ignored, d2: ignored */
	NVDIMM_REPLY_HELLO	= 0x80,  /* d1: server version, d2: zero */
	NVDIMM_REPLY_ERROR	= 0x81,  /* d1: 0xFFs,   d2: 0xFFs */
	NVDIMM_REPLY_CHIP 	= 0x90,  /* d1: numa_id, d2: status */
	NVDIMM_REPLY_BDEV	= 0xA0,  /* d1: bdev_id, d2: status */
};

#endif /*  __OPAL_PRD_NVDIMM_H__ */
