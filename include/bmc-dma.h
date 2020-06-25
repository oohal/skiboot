#ifndef __BMC_DMA_H__
#define __BMC_DMA_H__

#include <stdint.h>
#include <stdbool.h>

void bmc_dma_probe(void);
void bmc_dma_exit(void);
bool bmc_dma_available(void);

void bmc_dma_reinit(void);
void bmc_dma_teardown(void);
bool bmc_dma_ok(void);

int64_t bmc_dma_tce_map(void *buf, size_t size);
int64_t bmc_dma_tce_unmap(void *buf, size_t size);

extern bool bmc_dma_ready;

void bmc_dma_wait(void);
void bmc_dma_poll(void);

#endif /* __BMC_DMA_H__ */
