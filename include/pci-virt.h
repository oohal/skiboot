/* Copyright 2013-2016 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __PCI_VIRT_H
#define __PCI_VIRT_H

#include <ccan/list/list.h>

enum {
	PCI_VIRT_CFG_NORMAL,
	PCI_VIRT_CFG_RDONLY,
	PCI_VIRT_CFG_W1CLR,
	PCI_VIRT_CFG_MAX
};

struct pci_virt_device {
	uint32_t		bdfn;
	uint32_t		cfg_size;
	uint8_t			*config[PCI_VIRT_CFG_MAX];
	struct list_head	pcrf;
	struct list_node	node;
	void			*data;
};

extern void pci_virt_cfg_read_raw(struct pci_virt_device *pvd,
				  uint32_t space, uint32_t offset,
				  uint32_t size, uint32_t *data);
extern void pci_virt_cfg_write_raw(struct pci_virt_device *pvd,
				   uint32_t space, uint32_t offset,
				   uint32_t size, uint32_t data);
extern struct pci_cfg_reg_filter *pci_virt_add_filter(
					struct pci_virt_device *pvd,
					uint32_t start, uint32_t len,
					uint32_t flags, pci_cfg_reg_func func,
					void *data);
extern int64_t pci_virt_cfg_read(struct phb *phb, uint32_t bdfn,
				 uint32_t offset, uint32_t size,
				 uint32_t *data);
extern int64_t pci_virt_cfg_write(struct phb *phb, uint32_t bdfn,
				  uint32_t offset, uint32_t size,
				  uint32_t data);
extern struct pci_virt_device *pci_virt_find_device(struct phb *phb,
						    uint32_t bdfn);
extern struct pci_virt_device *pci_virt_add_device(struct phb *phb,
						   uint32_t bdfn,
						   uint32_t cfg_size,
						   void *data);

/*
 * The virtual config spaces is made up of three arrays (spaces) for:
 *
 * 	Normal
 * 	Read only (RO)
 * 	Write 1 to clear (W1C)
 *
 * The normal space is the "real" config space and if you read from it directly
 * you will see the contents of the PCI config space. The other two come into
 * play when writing. Writes are masked against the contents of the RO space
 * to ensure that any bits that were marked as read only when the virtual config
 * space was initialised will remain the same. The W1C space works similarly,
 * with it's contents being used to generate a mask of bits to be cleared.
 *
 *
 * Legend:
 *
 * d - struct pci_virt_device
 * o - byte offset to read/write from
 * s - Size of the config space IO (1/2/4)
 * v - Value to write or the pointer to read into
 *
 * r - value to write into the RO space
 * w - value to write into the W1C space
 *
 * In general, when setting up a config space use the PCI_VIRT_CFG_*_*() macros.
 */

#define PCI_VIRT_CFG_NORMAL_RD(d, o, s, v)	\
	pci_virt_cfg_read_raw(d, PCI_VIRT_CFG_NORMAL, o, s, v)
#define PCI_VIRT_CFG_NORMAL_WR(d, o, s, v)	\
	pci_virt_cfg_write_raw(d, PCI_VIRT_CFG_NORMAL, o, s, v)
#define PCI_VIRT_CFG_RDONLY_RD(d, o, s, v)	\
	pci_virt_cfg_read_raw(d, PCI_VIRT_CFG_RDONLY, o, s, v)
#define PCI_VIRT_CFG_RDONLY_WR(d, o, s, v)	\
	pci_virt_cfg_write_raw(d, PCI_VIRT_CFG_RDONLY, o, s, v)
#define PCI_VIRT_CFG_W1CLR_RD(d, o, s, v)	\
	pci_virt_cfg_read_raw(d, PCI_VIRT_CFG_W1CLR, o, s, v)
#define PCI_VIRT_CFG_W1CLR_WR(d, o, s, v)	\
	pci_virt_cfg_write_raw(d, PCI_VIRT_CFG_W1CLR, o, s, v)

#define PCI_VIRT_CFG_INIT(d, o, s, v, r, w)		\
	do {						\
		PCI_VIRT_CFG_NORMAL_WR(d, o, s, v);	\
		PCI_VIRT_CFG_RDONLY_WR(d, o, s, r);	\
		PCI_VIRT_CFG_W1CLR_WR(d, o, s, w);	\
	} while (0)
#define PCI_VIRT_CFG_INIT_RO(d, o, s, v)		\
	PCI_VIRT_CFG_INIT(d, o, s, v, 0xffffffff, 0)

/* templates for use with virtual PHBs */
int64_t pci_virt_cfg_read8(struct phb *, uint32_t, uint32_t, uint8_t *);
int64_t pci_virt_cfg_read16(struct phb *, uint32_t, uint32_t, uint16_t *);
int64_t pci_virt_cfg_read32(struct phb *, uint32_t, uint32_t, uint32_t *);

int64_t pci_virt_cfg_write8(struct phb *, uint32_t, uint32_t, uint8_t);
int64_t pci_virt_cfg_write16(struct phb *, uint32_t, uint32_t, uint16_t);
int64_t pci_virt_cfg_write32(struct phb *, uint32_t, uint32_t, uint32_t);

struct pci_slot *virt_slot_create(struct phb *phb);

#endif /* __VIRT_PCI_H */
