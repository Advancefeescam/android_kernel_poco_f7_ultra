// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <uapi/linux/pci_regs.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include "xring_pcie_hdma.h"
#include "uft_pcie_ep.h"

static inline uint32_t xring_pcie_hdma_reg_read(struct pcie_hdma *hdma,
						uint32_t reg_addr)
{
	return readl_relaxed(hdma->base + reg_addr);
}

static inline void xring_pcie_hdma_reg_write(struct pcie_hdma *hdma,
					     uint32_t reg_addr,
					     uint32_t val)
{
	writel_relaxed(val, hdma->base + reg_addr);
}

static void xring_hdma_abort_handler(struct pcie_hdma *hdma,
				     struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;

	dev_info(dev, "pcie hdma transfer abort,dir:%d,chan id:%d\n", chan_ctrl->dir, chan_ctrl->id);
}

static void xring_hdma_watermark_handler(struct pcie_hdma *hdma,
					 struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;

	dev_info(dev, "pcie hdma ll transfer done\n");
}

static void xring_hdma_irq_comman_handler(struct pcie_hdma *hdma,
					 struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;

	if (chan_ctrl->int_status & HDMA_INT_STATUS_ABORT)
		xring_hdma_abort_handler(hdma, chan_ctrl);
	if (chan_ctrl->int_status & HDMA_INT_STATUS_WATERMARK)
		xring_hdma_watermark_handler(hdma, chan_ctrl);
	if (chan_ctrl->int_status & HDMA_INT_STATUS_STOP)
		dev_info(dev, "pcie hdma transfer done,dir:%d,chan id:%d\n", chan_ctrl->dir, chan_ctrl->id);
}

static irqreturn_t xring_hdma_irq_rd_ch_handler(int irq, void *dev_id)
{
	struct pcie_hdma *hdma = dev_id;
	struct pcie_hdma_chan_ctrl *chan_ctrl;
	enum pcie_hdma_dir dir = HDMA_FROM_DEVICE;
	uint8_t chan_id;

	for (chan_id = 0; chan_id < PCIE_HDMA_CHAN_NUM; chan_id++) {
		chan_ctrl = &hdma->r_chans[chan_id];

		chan_ctrl->int_status = xring_pcie_hdma_reg_read(hdma,
							HDMA_INT_STATUS(chan_id, dir));
		if (!chan_ctrl->int_status || !chan_ctrl->inuse)
			continue;

		xring_hdma_irq_comman_handler(hdma, chan_ctrl);

		complete(&chan_ctrl->comp);
	}

	return IRQ_HANDLED;
}

static irqreturn_t xring_hdma_irq_wr_ch_handler(int irq, void *dev_id)
{
	struct pcie_hdma *hdma = dev_id;
	struct pcie_hdma_chan_ctrl *chan_ctrl;
	enum pcie_hdma_dir dir = HDMA_TO_DEVICE;
	uint8_t chan_id;

	for (chan_id = 0; chan_id < PCIE_HDMA_CHAN_NUM; chan_id++) {
		chan_ctrl = &hdma->w_chans[chan_id];

		chan_ctrl->int_status = xring_pcie_hdma_reg_read(hdma,
								 HDMA_INT_STATUS(chan_id, dir));
		if (!chan_ctrl->int_status || !chan_ctrl->inuse)
			continue;

		xring_hdma_irq_comman_handler(hdma, chan_ctrl);

		complete(&chan_ctrl->comp);
	}

	return IRQ_HANDLED;
}

static int xring_hdma_alloc_ll_mem(struct pcie_hdma *hdma,
				   struct pcie_hdma_mm_blocks *mm_blocks,
				   enum pcie_hdma_dir dir)
{
	unsigned int i, elmnt_len;

	if (mm_blocks->blk_cnt > PCIE_HDMA_MAX_XFER_ELMNT_CNT)
		return -EINVAL;

	elmnt_len = XRING_HDMA_BUF_SIZE / XRING_LL_BLOCK_CNT;
	for (i = 0; i < mm_blocks->blk_cnt; i++) {
		if (dir == HDMA_TO_DEVICE) {
			mm_blocks->blks[i].src_addr = XRING_HDMA_EP_BUF_ADDR + (i * elmnt_len);
			mm_blocks->blks[i].dst_addr = hdma->dma + (i * elmnt_len);
		} else {
			mm_blocks->blks[i].src_addr = hdma->dma + (i * elmnt_len);
			mm_blocks->blks[i].dst_addr = XRING_HDMA_EP_BUF_ADDR + (i * elmnt_len);
		}
		mm_blocks->blks[i].size = elmnt_len;
	}

	return 0;
}

int xring_hdma_ll_transfer(struct pcie_hdma *hdma, enum pcie_hdma_dir dir)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl;
	struct device *dev = &hdma->pci_dev->dev;
	struct pcie_hdma_mm_blocks mm_blocks;
	int ret = 0;

	chan_ctrl = xring_hdma_request_chan(hdma, dir);
	if (IS_ERR(chan_ctrl)) {
		ret = PTR_ERR(chan_ctrl);
		dev_err(dev, "request hdma channel failed: %d\n", ret);
		return ret;
	}

	memset(hdma->va, 0xbc, XRING_HDMA_BUF_SIZE);
	flush_cache_all();

	mm_blocks.blk_cnt = XRING_LL_BLOCK_CNT;
	ret = xring_hdma_alloc_ll_mem(hdma, &mm_blocks, dir);
	if (ret) {
		dev_err(dev, "alloc ll mem failed: %d\n", ret);
		goto release_chan;
	}

	ret = xring_hdma_xfer_add_ll_blocks(chan_ctrl, &mm_blocks);
	if (ret) {
		dev_err(dev, "add ll blocks failed: %d\n", ret);
		goto release_chan;
	}

	ret = xring_pcie_hdma_start_and_wait_end(hdma, chan_ctrl);
	if (ret) {
		dev_err(dev, "hdma transfer failed: %d\n", ret);
		goto release_chan;
	}

release_chan:
	xring_hdma_release_chan(chan_ctrl);
	return ret;
}
EXPORT_SYMBOL_GPL(xring_hdma_ll_transfer);

int xring_hdma_single_trans_pa(struct pcie_hdma *hdma, enum pcie_hdma_dir dir,
			    u32 rc_addr, int len, u32 ep_addr)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl = NULL;
	struct device *dev = &hdma->pci_dev->dev;
	int ret = 0;

	chan_ctrl = xring_hdma_request_chan(hdma, dir);
	if (IS_ERR(chan_ctrl)) {
		ret = PTR_ERR(chan_ctrl);
		dev_err(dev, "request hdma channel failed: %d\n", ret);
		return ret;
	}

	if (dir == HDMA_TO_DEVICE) {
		ret = xring_hdma_xfer_add_block(chan_ctrl, ep_addr,
						rc_addr, len);
		if (ret) {
			dev_err(dev, "add read xfer block failed: %d\n", ret);
			goto release_chan;
		}
	} else {
		ret = xring_hdma_xfer_add_block(chan_ctrl, rc_addr, ep_addr,
						len);
		if (ret) {
			dev_err(dev, "add write xfer block failed: %d\n", ret);
			goto release_chan;
		}
	}

	ret = xring_pcie_hdma_start_and_wait_end(hdma, chan_ctrl);
	if (ret) {
		dev_err(dev, "hdma transfer failed: %d\n", ret);
		goto release_chan;
	}

release_chan:
	xring_hdma_release_chan(chan_ctrl);
	return ret;
}
EXPORT_SYMBOL_GPL(xring_hdma_single_trans_pa);

int xring_hdma_single_trans(struct pcie_hdma *hdma, enum pcie_hdma_dir dir,
			    void *data, int len, u32 ep_addr)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl = NULL;
	struct device *dev = &hdma->pci_dev->dev;
	int ret = 0;

	chan_ctrl = xring_hdma_request_chan(hdma, dir);
	if (IS_ERR(chan_ctrl)) {
		ret = PTR_ERR(chan_ctrl);
		dev_err(dev, "request hdma channel failed: %d\n", ret);
		return ret;
	}

	if (dir == HDMA_FROM_DEVICE)
		memcpy(chan_ctrl->va, data, len);

	if (dir == HDMA_TO_DEVICE) {
		ret = xring_hdma_xfer_add_block(chan_ctrl, ep_addr,
						chan_ctrl->dma, len);
		if (ret) {
			dev_err(dev, "add read xfer block failed: %d\n", ret);
			goto release_chan;
		}
	} else {
		ret = xring_hdma_xfer_add_block(chan_ctrl, chan_ctrl->dma, ep_addr,
						len);
		if (ret) {
			dev_err(dev, "add write xfer block failed: %d\n", ret);
			goto release_chan;
		}
	}

	ret = xring_pcie_hdma_start_and_wait_end(hdma, chan_ctrl);
	if (ret) {
		dev_err(dev, "hdma transfer failed: %d\n", ret);
		goto release_chan;
	}

	if (dir == HDMA_TO_DEVICE)
		memcpy(data, chan_ctrl->va, len);

release_chan:
	xring_hdma_release_chan(chan_ctrl);

	return ret;
}

int xring_hdma_single_trans_va(struct pcie_hdma *hdma, enum pcie_hdma_dir dir,
				void *data, int len, u32 ep_addr)
{
	int count = len / XRING_HDMA_BUF_SIZE;
	int reserved = len % XRING_HDMA_BUF_SIZE;
	int i, ret;
	int offset = 0;

	for (i = 0; i < count; i++) {
		ret = xring_hdma_single_trans(hdma, dir, &data[offset], XRING_HDMA_BUF_SIZE, ep_addr + offset);
		offset += XRING_HDMA_BUF_SIZE;
	}
	if (reserved)
		ret = xring_hdma_single_trans(hdma, dir, &data[offset], reserved, ep_addr + offset);

	return ret;

}
EXPORT_SYMBOL_GPL(xring_hdma_single_trans_va);

static void xring_hdma_single_xfer_config(struct pcie_hdma *hdma, uint8_t chan_id,
					  enum pcie_hdma_dir dir,
					  struct pcie_hdma_xfer_cfg *xfer_cfg)
{
	uint32_t src_addr = xfer_cfg->ll_elmnt[0].sar_low;
	uint32_t dst_addr = xfer_cfg->ll_elmnt[0].dar_low;
	uint32_t size = xfer_cfg->ll_elmnt[0].tx_size;
	uint32_t val;

	/* Enable chan */
	xring_pcie_hdma_reg_write(hdma, HDMA_EN(chan_id, dir), HDMA_CHAN_EN);

	/* Configure transfer size*/
	xring_pcie_hdma_reg_write(hdma, HDMA_XFERSIZE(chan_id, dir), size);

	/* Configure src addr */
	xring_pcie_hdma_reg_write(hdma, HDMA_SAR_LOW(chan_id, dir), src_addr);
	xring_pcie_hdma_reg_write(hdma, HDMA_SAR_HIGH(chan_id, dir), 0);

	/* Configure dst addr */
	xring_pcie_hdma_reg_write(hdma, HDMA_DAR_LOW(chan_id, dir), dst_addr);
	xring_pcie_hdma_reg_write(hdma, HDMA_DAR_HIGH(chan_id, dir), 0);

	/* Disable linked list mode */
	val = xring_pcie_hdma_reg_read(hdma, HDMA_CONTROL1(chan_id, dir));
	val &= ~HDMA_CONTROL1_LLEN;
	xring_pcie_hdma_reg_write(hdma, HDMA_DAR_HIGH(chan_id, dir), val);
}

static void xring_hdma_ll_xfer_config(struct pcie_hdma *hdma, uint8_t chan_id,
				      enum pcie_hdma_dir dir,
				      struct pcie_hdma_xfer_cfg *cfg)
{
	struct pcie_hdma_ll_link_elmnt *link_elmnt =
			(struct pcie_hdma_ll_link_elmnt *)&cfg->ll_elmnt[cfg->ll_elmnt_cnt];
	uint32_t val;

	/* Enable chan */
	xring_pcie_hdma_reg_write(hdma, HDMA_EN(chan_id, dir), HDMA_CHAN_EN);

	xring_pcie_hdma_reg_write(hdma, HDMA_LLP_LOW(chan_id, dir), link_elmnt->llp_low);
	xring_pcie_hdma_reg_write(hdma, HDMA_LLP_HIGH(chan_id, dir), link_elmnt->llp_high);

	/* Enable linked list mode */
	val = xring_pcie_hdma_reg_read(hdma, HDMA_CONTROL1(chan_id, dir));
	val |= HDMA_CONTROL1_LLEN;
	xring_pcie_hdma_reg_write(hdma, HDMA_DAR_HIGH(chan_id, dir), val);

	/* Setup CCS and CB */
	val = xring_pcie_hdma_reg_read(hdma, HDMA_CYCLE(chan_id, dir));
	val = HDMA_CYCLE_CYCLE_STATE | HDMA_CYCLE_CYCLE_BIT;
	xring_pcie_hdma_reg_write(hdma, HDMA_CYCLE(chan_id, dir), val);
}

static void xring_pcie_hdma_chan_reset(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	chan_ctrl->inuse = false;
	chan_ctrl->xfer_cfg.ll_elmnt_cnt = 0;
	chan_ctrl->xfer_cfg.total_len = 0;
}

static bool xring_hdma_ll_elmnt_validate(struct pcie_hdma_ll_data_elmnt *elmnt)
{
	if (elmnt->sar_low == elmnt->dar_low)
		return false;

	if (!elmnt->tx_size)
		return false;

	return true;
}

static bool xring_hdma_single_blk_xfer_cfg_validate(struct pcie_hdma_xfer_cfg *xfer_cfg)
{
	if (xfer_cfg->ll_elmnt_cnt != 1)
		return false;

	if (!xring_hdma_ll_elmnt_validate(&xfer_cfg->ll_elmnt[0]))
		return false;

	return true;
}

static bool xring_hdma_ll_xfer_cfg_validate(struct pcie_hdma_xfer_cfg *xfer_cfg)
{
	uint32_t i;

	if (xfer_cfg->ll_elmnt_cnt <= 1)
		return false;

	for (i = 0; i < xfer_cfg->ll_elmnt_cnt; i++) {
		if (xring_hdma_ll_elmnt_validate(&xfer_cfg->ll_elmnt[i]))
			return false;
	}

	return true;
}

static bool xring_pcie_hdma_xfer_cfg_validate(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	if (chan_ctrl->xfer_mode == HDMA_SINGLE_BLK_MODE)
		return xring_hdma_single_blk_xfer_cfg_validate(&chan_ctrl->xfer_cfg);

	if (chan_ctrl->xfer_mode == HDMA_LL_MODE)
		return xring_hdma_ll_xfer_cfg_validate(&chan_ctrl->xfer_cfg);

	return false;
}

static bool xring_hdma_chan_partial_validate(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	if (!chan_ctrl->inuse)
		return false;

	if (chan_ctrl->dir != HDMA_TO_DEVICE && chan_ctrl->dir != HDMA_FROM_DEVICE)
		return false;

	if (chan_ctrl->id >= PCIE_HDMA_CHAN_NUM)
		return false;

	if (chan_ctrl->wait_type != POLLING && chan_ctrl->wait_type != INTERRUPT)
		return false;

	return true;
}

static bool xring_hdma_chan_full_validate(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	if (!xring_hdma_chan_partial_validate(chan_ctrl))
		return false;

	if (!xring_pcie_hdma_xfer_cfg_validate(chan_ctrl))
		return false;

	return true;
}

static void xring_hdma_xfer_config(struct pcie_hdma *hdma,
				   struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	if (chan_ctrl->xfer_mode == HDMA_SINGLE_BLK_MODE)
		xring_hdma_single_xfer_config(hdma, chan_ctrl->id, chan_ctrl->dir,
					      &chan_ctrl->xfer_cfg);
	else
		xring_hdma_ll_xfer_config(hdma, chan_ctrl->id, chan_ctrl->dir,
					  &chan_ctrl->xfer_cfg);
}

static void xring_hdma_add_link_elmnt(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct pcie_hdma_xfer_cfg *xfer_cfg = &chan_ctrl->xfer_cfg;
	struct pcie_hdma_ll_link_elmnt *link_elment = NULL;
	uint32_t link_elemnt_idx = xfer_cfg->ll_elmnt_cnt;

	link_elment = (struct pcie_hdma_ll_link_elmnt *)&xfer_cfg->ll_elmnt[link_elemnt_idx];
	link_elment->cfg = HDMA_LINK_ELMNT_LLP | HDMA_LINK_ELMNT_TCB;
	link_elment->llp_high = 0;

	xfer_cfg->ll_elmnt_cnt++; /* add count for the link elmnt */
}

/****************************************************************************
 * Name: xring_hdma_request_chan
 *
 * Description:
 *	Request an HDMA channel according to dir.
 *
 ****************************************************************************/
struct pcie_hdma_chan_ctrl *xring_hdma_request_chan(struct pcie_hdma *hdma,
						    enum pcie_hdma_dir dir)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl = NULL;
	struct device *dev = &hdma->pci_dev->dev;
	unsigned long flags;
	uint32_t i;

	if (dir != HDMA_FROM_DEVICE && dir != HDMA_TO_DEVICE) {
		dev_err(dev, "Invalid HDMA direction\n");
		return NULL;
	}

	for (i = 0; i < PCIE_HDMA_CHAN_NUM; i++) {
		chan_ctrl = (dir == HDMA_FROM_DEVICE) ? &hdma->r_chans[i] : &hdma->w_chans[i];
		spin_lock_irqsave(&chan_ctrl->chan_lock, flags);
		if (!chan_ctrl->inuse) {
			chan_ctrl->inuse = true;
			chan_ctrl->timeout_ms = PCIE_HDMA_DEF_TIMEOUT_MS;
			chan_ctrl->wait_type = INTERRUPT;
			spin_unlock_irqrestore(&chan_ctrl->chan_lock, flags);
			/*alloc dma buffer*/
			chan_ctrl->va = dma_alloc_coherent(dev, XRING_HDMA_BUF_SIZE, &chan_ctrl->dma, GFP_KERNEL);
			if (!chan_ctrl->va) {
				spin_unlock_irqrestore(&chan_ctrl->chan_lock, flags);
				return NULL;
			}
			return chan_ctrl;
		}
		spin_unlock_irqrestore(&chan_ctrl->chan_lock, flags);
	}

	dev_err(dev, "no available channels now!\n");
	return NULL;
}
EXPORT_SYMBOL_GPL(xring_hdma_request_chan);

void xring_hdma_release_chan(struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	unsigned long flags;

	if (!chan_ctrl->inuse)
		return;

	dma_free_coherent(&chan_ctrl->hdma->pci_dev->dev, XRING_HDMA_BUF_SIZE, chan_ctrl->va, chan_ctrl->dma);
	spin_lock_irqsave(&chan_ctrl->chan_lock, flags);
	xring_pcie_hdma_chan_reset(chan_ctrl);
	spin_unlock_irqrestore(&chan_ctrl->chan_lock, flags);
}
EXPORT_SYMBOL_GPL(xring_hdma_release_chan);

int32_t xring_hdma_xfer_add_block(struct pcie_hdma_chan_ctrl *chan_ctrl,
				  uint32_t src_addr, uint32_t dst_addr,
				  uint32_t size)
{
	struct pcie_hdma_xfer_cfg *xfer_cfg = NULL;
	uint32_t first_slot;

	if (!chan_ctrl->inuse)
		return -EINVAL;

	xfer_cfg = &chan_ctrl->xfer_cfg;
	if (xfer_cfg->ll_elmnt_cnt + 1 > PCIE_HDMA_MAX_XFER_ELMNT_CNT)
		return -EINVAL;

	first_slot = xfer_cfg->ll_elmnt_cnt;
	xfer_cfg->ll_elmnt[first_slot].cfg |= HDMA_CYCLE_CYCLE_BIT;
	xfer_cfg->ll_elmnt[first_slot].tx_size = size;
	xfer_cfg->ll_elmnt[first_slot].sar_low = src_addr;
	xfer_cfg->ll_elmnt[first_slot].sar_high = 0;
	xfer_cfg->ll_elmnt[first_slot].dar_low = dst_addr;
	xfer_cfg->ll_elmnt[first_slot].dar_high = 0;

#ifdef XRING_PCIE_PERDORMANCE
	xfer_cfg->total_len += size;
#endif
	xfer_cfg->ll_elmnt_cnt += 1;
	if (xfer_cfg->ll_elmnt_cnt == 1)
		chan_ctrl->xfer_mode = HDMA_SINGLE_BLK_MODE;
	else
		chan_ctrl->xfer_mode = HDMA_LL_MODE;

	return 0;
}
EXPORT_SYMBOL_GPL(xring_hdma_xfer_add_block);

int32_t xring_hdma_xfer_add_ll_blocks(struct pcie_hdma_chan_ctrl *chan_ctrl,
				      struct pcie_hdma_mm_blocks *blocks)
{
	struct pcie_hdma_xfer_cfg *xfer_cfg = NULL;
	uint32_t first_slot;
	uint32_t i;

	if (!chan_ctrl || !blocks)
		return -EINVAL;

	if (!blocks->blk_cnt)
		return -EINVAL;

	if (!chan_ctrl->inuse)
		return -EBUSY;

	xfer_cfg = &chan_ctrl->xfer_cfg;
	if (blocks->blk_cnt + xfer_cfg->ll_elmnt_cnt > PCIE_HDMA_MAX_XFER_ELMNT_CNT)
		return -EINVAL;

	first_slot = xfer_cfg->ll_elmnt_cnt;
	for (i = 0; i < blocks->blk_cnt; i++) {
#ifdef XRING_PCIE_PERDORMANCE
		xfer_cfg->total_len += blocks->blks[i].size;
#endif
		xfer_cfg->ll_elmnt[first_slot + i].cfg |= HDMA_CYCLE_CYCLE_BIT;
		xfer_cfg->ll_elmnt[first_slot + i].tx_size = blocks->blks[i].size;
		xfer_cfg->ll_elmnt[first_slot + i].sar_low = blocks->blks[i].src_addr;
		xfer_cfg->ll_elmnt[first_slot + i].sar_high = 0;
		xfer_cfg->ll_elmnt[first_slot + i].dar_low = blocks->blks[i].dst_addr;
		xfer_cfg->ll_elmnt[first_slot + i].dar_high = 0;
	}

	xfer_cfg->ll_elmnt_cnt += blocks->blk_cnt;
	if (xfer_cfg->ll_elmnt_cnt > 1)
		chan_ctrl->xfer_mode = HDMA_LL_MODE;
	else
		chan_ctrl->xfer_mode = HDMA_SINGLE_BLK_MODE;

	return 0;
}
EXPORT_SYMBOL_GPL(xring_hdma_xfer_add_ll_blocks);

static int xring_hdma_do_complete(struct pcie_hdma *hdma,
				  struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;
	uint32_t int_status;
#ifdef XRING_PCIE_PERDORMANCE
	uint64_t cost_ns;
#endif

	int_status = xring_pcie_hdma_reg_read(hdma, HDMA_STATUS(chan_ctrl->id, chan_ctrl->dir));

	if (int_status == HDMA_STATUS_STOPPED) {
#ifdef XRING_PCIE_PERDORMANCE
		chan_ctrl->end_ns = do_get_timeofday();
		cost_ns = chan_ctrl->end_ns - chan_ctrl->start_ns;

		dev_info(dev, "\tlen: %luBytes, cost %luns, speed: %lluMB/s\n",
			 chan_ctrl->xfer_cfg.total_len, cost_ns,
			 (chan_ctrl->xfer_cfg.total_len * NSEC_PER_SEC) / (cost_ns * 1024 * 1024));
#endif
		dev_info(dev, "%d Finished!\n", __LINE__);

		return 0;
	}

	if (int_status == HDMA_STATUS_ABORTED) {
		int_status = xring_pcie_hdma_reg_read(hdma,
						HDMA_INT_STATUS(chan_ctrl->id, chan_ctrl->dir));
		dev_info(dev, "%d Aborted! INT_STATUS: 0x%x,dir:%d,chan id %d\n", __LINE__, int_status, chan_ctrl->dir, chan_ctrl->id);
	} else
		dev_info(dev, "%d Unknown HDMA status: 0x%x,dir:%d,chan id %d\n", __LINE__, int_status, chan_ctrl->dir, chan_ctrl->id);

	return -EINVAL;

}

static int32_t xring_hdma_wait_for_complete(struct pcie_hdma *hdma,
					    struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev = &hdma->pci_dev->dev;
	enum pcie_hdma_dir dir;
#ifdef XRING_PCIE_PERDORMANCE
	uint64_t wait_start_ms;
#endif
	uint8_t chan_id;
	uint32_t ret = 0;

	chan_id = chan_ctrl->id;
	dir = chan_ctrl->dir;

#ifdef XRING_PCIE_PERDORMANCE
	if (chan_ctrl->wait_type == POLLING) {
		wait_start_ms = do_get_timeofday();
		while (xring_pcie_hdma_reg_read(hdma, HDMA_STATUS(chan_id, dir)) == HDMA_STATUS_RUNNING) {
			if (do_get_timeofday() - wait_start_ms > chan_ctrl->timeout_ms) {
				dev_info(dev, "%s timeout!\n", chan_ctrl->chan_name);
				return -EINVAL;
			}
		}

	} else {
#endif /* XRING_PCIE_PERDORMANCE */
		ret = wait_for_completion_timeout(&chan_ctrl->comp,
				msecs_to_jiffies(chan_ctrl->timeout_ms));
		if (!ret) {
			dev_info(dev, "wait for completion timeout!,dir:%d,chan id %d\n", chan_ctrl->dir, chan_ctrl->id);
			ret = -ETIMEDOUT;
			return ret;
		}
#ifdef XRING_PCIE_PERDORMANCE
	}
#endif

	return xring_hdma_do_complete(hdma, chan_ctrl);
}

static int xring_hdma_msi_conf(struct pcie_hdma *hdma, struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct pci_dev *pci_dev = hdma->pci_dev;
	unsigned int msi_addr_low, msi_data;
	unsigned short ctl_val;
	unsigned int msi_addr_high = 0;

	pci_read_config_word(pci_dev, pci_dev->msi_cap + PCI_MSI_FLAGS, &ctl_val);

	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_LO, &msi_addr_low);

	if (!(ctl_val & PCI_MSI_FLAGS_ENABLE))
		return -EINVAL;

	if (ctl_val & PCI_MSI_FLAGS_64BIT) {
		pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_DATA_64, &msi_data);
		pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_HI, &msi_addr_high);
	} else
		pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_DATA_32, &msi_data);

	xring_pcie_hdma_reg_write(hdma, HDMA_MSI_STOP_LOW(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_low);
	xring_pcie_hdma_reg_write(hdma, HDMA_MSI_STOP_HIGH(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_high);

	xring_pcie_hdma_reg_write(hdma, HDMA_MSI_WATERMARK_LOW(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_low);
	xring_pcie_hdma_reg_write(hdma, HDMA_MSI_WATERMARK_HIGH(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_high);

	xring_pcie_hdma_reg_write(hdma, HDMA_MSI_ABORT_LOW(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_low);
	xring_pcie_hdma_reg_write(hdma, HDMA_MSI_ABORT_HIGH(chan_ctrl->id, chan_ctrl->dir),
				  msi_addr_high);

	if (chan_ctrl->dir == HDMA_TO_DEVICE)
		msi_data += PCIE_HDMA_MSI_NUM;
	else
		msi_data += PCIE_HDMA_MSI_NUM + 1;

	xring_pcie_hdma_reg_write(hdma, HDMA_MSI_MSGD(chan_ctrl->id, chan_ctrl->dir),
				  msi_data);

	return 0;
}

static void xring_pcie_hdma_start(struct pcie_hdma *hdma,
				  struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	uint32_t enabled_int, val;

	xring_hdma_xfer_config(hdma, chan_ctrl);

#ifdef XRING_PCIE_PERDORMANCE
	chan_ctrl->start_ns = do_get_timeofday();
#endif
	enabled_int = HDMA_INT_LAIE | HDMA_INT_RAIE | HDMA_INT_LSIE | HDMA_INT_RSIE;
	xring_pcie_hdma_reg_write(hdma, HDMA_INT_SETUP(chan_ctrl->id, chan_ctrl->dir), enabled_int);

	val = xring_pcie_hdma_reg_read(hdma, HDMA_DOORBELL(chan_ctrl->id, chan_ctrl->dir));
	val |= HDMA_DB_START;
	xring_pcie_hdma_reg_write(hdma, HDMA_DOORBELL(chan_ctrl->id, chan_ctrl->dir), val);
}

int32_t xring_pcie_hdma_start_and_wait_end(struct pcie_hdma *hdma,
					   struct pcie_hdma_chan_ctrl *chan_ctrl)
{
	struct device *dev;
	int32_t ret = 0;

	if (!hdma) {
		pr_err("xring hdma is NULL !\n");
		return -EINVAL;
	}

	dev = &hdma->pci_dev->dev;

	ret = xring_hdma_chan_full_validate(chan_ctrl);
	if (!ret) {
		dev_err(dev, "hdma channel validate: %d\n", ret);
		return ret;
	}

	ret = xring_hdma_msi_conf(hdma, chan_ctrl);
	if (ret) {
		dev_err(dev, "hdma msi configure failed: %d\n", ret);
		return ret;
	}

	if (chan_ctrl->xfer_mode == HDMA_LL_MODE)
		xring_hdma_add_link_elmnt(chan_ctrl);

	xring_pcie_hdma_start(hdma, chan_ctrl);

	ret = xring_hdma_wait_for_complete(hdma, chan_ctrl);

	return ret;
}
EXPORT_SYMBOL_GPL(xring_pcie_hdma_start_and_wait_end);

static void xring_pcie_hdma_chan_init(struct pcie_hdma *hdma)
{
	struct pcie_hdma_chan_ctrl *chan_ctrl;
	uint32_t i;

	for (i = 0; i < PCIE_HDMA_CHAN_NUM; i++) {
		chan_ctrl = &hdma->r_chans[i];
		chan_ctrl->hdma = hdma;
		chan_ctrl->id = i;
		chan_ctrl->dir = HDMA_FROM_DEVICE;
		spin_lock_init(&chan_ctrl->chan_lock);
		init_completion(&chan_ctrl->comp);
		xring_pcie_hdma_chan_reset(chan_ctrl);

		chan_ctrl = &hdma->w_chans[i];
		chan_ctrl->hdma = hdma;
		chan_ctrl->id = i;
		chan_ctrl->dir = HDMA_TO_DEVICE;
		init_completion(&chan_ctrl->comp);
		spin_lock_init(&chan_ctrl->chan_lock);
		xring_pcie_hdma_chan_reset(chan_ctrl);
	}
}

struct pcie_hdma *xring_pcie_hdma_init(struct uft_pcie_ep *ep)
{
	struct pci_dev *pdev = ep->pdev;
	struct device *dev = &pdev->dev;
	struct pcie_hdma *hdma;
	struct uft_pcie_bar_ctrl *hdma_bar;
	uint16_t ctl;
	int ret = 0;

	hdma = devm_kzalloc(dev, sizeof(struct pcie_hdma), GFP_KERNEL);
	if (!hdma)
		return ERR_PTR(-ENOMEM);

	hdma_bar = ep->uft_bars[HDMA_ACCESS_BAR];
	hdma->base = hdma_bar->bar_virt;
	hdma->pci_dev = pdev;

	ret = pcie_capability_read_word(pdev, PCI_EXP_DEVCTL, &ctl);
	if (ret)
		return ERR_PTR(ret);

	if (ctl & PCI_EXP_DEVCTL_NOSNOOP_EN) {
		ctl &= ~PCI_EXP_DEVCTL_NOSNOOP_EN;
		pcie_capability_write_word(pdev, PCI_EXP_DEVCTL, ctl);
	}

	/*request wr ch handler*/
	ret = devm_request_threaded_irq(dev, pdev->irq + PCIE_HDMA_MSI_NUM,
					NULL, xring_hdma_irq_wr_ch_handler,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					"xring_hdma_wr_ch", hdma);
	if (ret) {
		dev_err(dev, "request hdma write channel msi irq failed!\n");
		return ERR_PTR(ret);
	}

	/*request rd ch handler*/
	ret = devm_request_threaded_irq(dev, pdev->irq + PCIE_HDMA_MSI_NUM + 1,
					NULL, xring_hdma_irq_rd_ch_handler,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					"xring_hdma_rd_ch", hdma);
	if (ret) {
		dev_err(dev, "request hdma read channel msi irq failed!\n");
		return ERR_PTR(ret);
	}

	xring_pcie_hdma_chan_init(hdma);

#ifdef XRING_DEBUGFS
	xring_debugfs_add_pcie_hdma(hdma);
#endif

	return hdma;
}
EXPORT_SYMBOL_GPL(xring_pcie_hdma_init);

MODULE_LICENSE("GPL");
