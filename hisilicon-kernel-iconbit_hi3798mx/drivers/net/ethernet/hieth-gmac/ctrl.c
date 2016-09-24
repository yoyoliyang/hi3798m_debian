#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include "util.h"
#include "higmac.h"
#include "forward.h"
#include "ctrl.h"

#ifdef CONFIG_ARCH_S40
#include "board-s40.c"
#endif

void higmac_hw_get_mac_addr(struct higmac_netdev_local *ld, unsigned char *mac)
{
	unsigned long reg;

	reg = higmac_readl(STATION_ADDR_HIGH);
	mac[0] = (reg >> 8) & 0xff;
	mac[1] = reg & 0xff;

	reg = higmac_readl(STATION_ADDR_LOW);
	mac[2] = (reg >> 24) & 0xff;
	mac[3] = (reg >> 16) & 0xff;
	mac[4] = (reg >> 8) & 0xff;
	mac[5] = reg & 0xff;
}

int higmac_hw_set_mac_addr(struct higmac_netdev_local *ld, unsigned char *mac)
{
	unsigned long reg;

	reg = mac[1] | (mac[0] << 8);
	higmac_writel(reg, STATION_ADDR_HIGH);

	reg = mac[5] | (mac[4] << 8) | (mac[3] << 16) | (mac[2] << 24);
	higmac_writel(reg, STATION_ADDR_LOW);

	/* add uc addr in fwd table, use entry0--eth0, entry1--eth1 */
	fwd_uc_mc_tbl_add(ld, mac, ld->index, ADD_UC);

	return 0;
}

/* config hardware queue depth */
void higmac_hw_set_desc_queue_depth(struct higmac_netdev_local *ld)
{
	if (HIGMAC_HWQ_RX_FQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH
		|| HIGMAC_HWQ_RX_BQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH
		|| HIGMAC_HWQ_TX_BQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH
		|| HIGMAC_HWQ_TX_RQ_DEPTH > HIGMAC_MAX_QUEUE_DEPTH
	   )
		BUG();

	higmac_writel_bits(ld, 1, RX_FQ_REG_EN, BITS_RX_FQ_DEPTH_EN);
	higmac_writel_bits(ld, HIGMAC_HWQ_RX_FQ_DEPTH << 3, RX_FQ_DEPTH, \
		BITS_RX_FQ_DEPTH);
	higmac_writel_bits(ld, 0, RX_FQ_REG_EN, BITS_RX_FQ_DEPTH_EN);

	higmac_writel_bits(ld, 1, RX_BQ_REG_EN, BITS_RX_BQ_DEPTH_EN);
	higmac_writel_bits(ld, HIGMAC_HWQ_RX_BQ_DEPTH << 3, RX_BQ_DEPTH, \
		BITS_RX_BQ_DEPTH);
	higmac_writel_bits(ld, 0, RX_BQ_REG_EN, BITS_RX_BQ_DEPTH_EN);

	higmac_writel_bits(ld, 1, TX_BQ_REG_EN, BITS_TX_BQ_DEPTH_EN);
	higmac_writel_bits(ld, HIGMAC_HWQ_TX_BQ_DEPTH << 3, TX_BQ_DEPTH, \
		BITS_TX_BQ_DEPTH);
	higmac_writel_bits(ld, 0, TX_BQ_REG_EN, BITS_TX_BQ_DEPTH_EN);

	higmac_writel_bits(ld, 1, TX_RQ_REG_EN, BITS_TX_RQ_DEPTH_EN);
	higmac_writel_bits(ld, HIGMAC_HWQ_TX_RQ_DEPTH << 3, TX_RQ_DEPTH, \
		BITS_TX_RQ_DEPTH);
	higmac_writel_bits(ld, 0, TX_RQ_REG_EN, BITS_TX_RQ_DEPTH_EN);
}

void higmac_hw_phy_gpio_reset(void)
{
	u32 gpio_base, gpio_bit, v;
	int i;
#define RESET_DATA      (1)

	for (i = 0; i < CONFIG_GMAC_NUMS; i++) {
		gpio_base = higmac_board_info[i].gpio_base;
		gpio_bit = higmac_board_info[i].gpio_bit;

		if (!gpio_base)
			continue;

		gpio_base = IO_ADDRESS(gpio_base);

		/* config gpio[x] dir to output */
		v = readb((void __iomem *)(gpio_base + 0x400));
		v |= (1 << gpio_bit);
		writeb(v, (void __iomem *)(gpio_base + 0x400));

		/* output 1--0--1 */
		writeb(RESET_DATA << gpio_bit,
				(void __iomem *)(gpio_base + (4 << gpio_bit)));
		msleep(20);
		writeb((!RESET_DATA) << gpio_bit,
				(void __iomem *)(gpio_base + (4 << gpio_bit)));
		msleep(20);
		writeb(RESET_DATA << gpio_bit,
				(void __iomem *)(gpio_base + (4 << gpio_bit)));
	}
}

void higmac_hw_mac_core_init(struct higmac_netdev_local *ld)
{
	/* disable and clear all interrupts */
	writel(0, ld->gmac_iobase + ENA_PMU_INT);
	writel(~0, ld->gmac_iobase + RAW_PMU_INT);

	/* enable CRC erro packets filter */
	higmac_writel_bits(ld, 1, REC_FILT_CONTROL, BIT_CRC_ERR_PASS);

	/* fix bug for udp and ip error check */
	writel(CONTROL_WORD_CONFIG, ld->gmac_iobase + CONTROL_WORD);

	writel(0, ld->gmac_iobase + COL_SLOT_TIME);

	writel(DUPLEX_HALF, ld->gmac_iobase + MAC_DUPLEX_HALF_CTRL);

	/* FIXME: interrupt when rcv packets >= RX_BQ_INT_THRESHOLD */
	higmac_writel_bits(ld, RX_BQ_INT_THRESHOLD, IN_QUEUE_TH,
			BITS_RX_BQ_IN_TH);
	higmac_writel_bits(ld, TX_RQ_INT_THRESHOLD, IN_QUEUE_TH,
			BITS_TX_RQ_IN_TH);

	/* FIXME: rx_bq/tx_rq in timeout threshold */
	higmac_writel_bits(ld, 0x10000, RX_BQ_IN_TIMEOUT_TH,
			BITS_RX_BQ_IN_TIMEOUT_TH);

	higmac_writel_bits(ld, 0x50000, TX_RQ_IN_TIMEOUT_TH,
			BITS_TX_RQ_IN_TIMEOUT_TH);

	higmac_hw_set_desc_queue_depth(ld);
}

void higmac_set_rx_fq_hwq_addr(struct higmac_netdev_local *ld,
		dma_addr_t phy_addr)
{
	higmac_writel_bits(ld, 1, RX_FQ_REG_EN, \
		BITS_RX_FQ_START_ADDR_EN);

	higmac_writel(phy_addr, RX_FQ_START_ADDR);

	higmac_writel_bits(ld, 0, RX_FQ_REG_EN, \
		BITS_RX_FQ_START_ADDR_EN);
}

void higmac_set_rx_bq_hwq_addr(struct higmac_netdev_local *ld,
		dma_addr_t phy_addr)
{
	higmac_writel_bits(ld, 1, RX_BQ_REG_EN, \
		BITS_RX_BQ_START_ADDR_EN);

	higmac_writel(phy_addr, RX_BQ_START_ADDR);

	higmac_writel_bits(ld, 0, RX_BQ_REG_EN, \
		BITS_RX_BQ_START_ADDR_EN);
}

void higmac_set_tx_bq_hwq_addr(struct higmac_netdev_local *ld,
		dma_addr_t phy_addr)
{
	higmac_writel_bits(ld, 1, TX_BQ_REG_EN, \
		BITS_TX_BQ_START_ADDR_EN);

	higmac_writel(phy_addr, TX_BQ_START_ADDR);

	higmac_writel_bits(ld, 0, TX_BQ_REG_EN, \
		BITS_TX_BQ_START_ADDR_EN);
}

void higmac_set_tx_rq_hwq_addr(struct higmac_netdev_local *ld,
		dma_addr_t phy_addr)
{
	higmac_writel_bits(ld, 1, TX_RQ_REG_EN, \
		BITS_TX_RQ_START_ADDR_EN);

	higmac_writel(phy_addr, TX_RQ_START_ADDR);

	higmac_writel_bits(ld, 0, TX_RQ_REG_EN, \
		BITS_TX_RQ_START_ADDR_EN);
}

void higmac_hw_set_desc_queue_addr(struct higmac_netdev_local *ld)
{
	higmac_set_rx_fq_hwq_addr(ld, ld->rx_fq.phys_addr);
	higmac_set_rx_bq_hwq_addr(ld, ld->rx_bq.phys_addr);
	higmac_set_tx_rq_hwq_addr(ld, ld->tx_rq.phys_addr);
	higmac_set_tx_bq_hwq_addr(ld, ld->tx_bq.phys_addr);
}

int higmac_read_irqstatus(struct higmac_netdev_local *ld)
{
	int status;

	status = higmac_readl(RAW_PMU_INT);

	return status;
}

int higmac_clear_irqstatus(struct higmac_netdev_local *ld, int irqs)
{
	int status;

	higmac_writel(irqs, RAW_PMU_INT);
	status = higmac_read_irqstatus(ld);

	return status;
}

void higmac_irq_enable(struct higmac_netdev_local *ld)
{
	higmac_writel(RX_BQ_IN_INT | RX_BQ_IN_TIMEOUT_INT
			| TX_RQ_IN_INT | TX_RQ_IN_TIMEOUT_INT,
			ENA_PMU_INT);
}

void higmac_irq_disable(struct higmac_netdev_local *ld)
{
	higmac_writel(0, ENA_PMU_INT);
}

void higmac_hw_desc_enable(struct higmac_netdev_local *ld)
{
	writel(0xF, ld->gmac_iobase + DESC_WR_RD_ENA);
}

void higmac_hw_desc_disable(struct higmac_netdev_local *ld)
{
	writel(0, ld->gmac_iobase + DESC_WR_RD_ENA);
}

void higmac_port_enable(struct higmac_netdev_local *ld)
{
	higmac_writel_bits(ld, 1, PORT_EN, BITS_TX_EN);
	higmac_writel_bits(ld, 1, PORT_EN, BITS_RX_EN);
}

void higmac_port_disable(struct higmac_netdev_local *ld)
{
	higmac_writel_bits(ld, 0, PORT_EN, BITS_TX_EN);
	higmac_writel_bits(ld, 0, PORT_EN, BITS_RX_EN);
}

void higmac_rx_port_disable(struct higmac_netdev_local *ld)
{
	higmac_writel_bits(ld, 0, PORT_EN, BITS_RX_EN);
}

void higmac_rx_port_enable(struct higmac_netdev_local *ld)
{
	higmac_writel_bits(ld, 1, PORT_EN, BITS_RX_EN);
}

int higmac_xmit_release_skb(struct higmac_netdev_local *ld)
{
	struct sk_buff *skb = NULL;
	int tx_rq_wr_offset, tx_rq_rd_offset, pos;
	struct higmac_desc *tx_rq_desc;
	dma_addr_t dma_addr;
	int ret = 0;
	int release = false;

	tx_rq_wr_offset = higmac_readl(TX_RQ_WR_ADDR);/* logic write */
	tx_rq_rd_offset = higmac_readl(TX_RQ_RD_ADDR);/* software read */

	while (tx_rq_rd_offset != tx_rq_wr_offset) {
		pos = tx_rq_rd_offset >> 5;
reload:
		tx_rq_desc = ld->tx_rq.desc + pos;

		skb = tx_rq_desc->skb_buff_addr;
		if (!skb) {
			pr_err("tx_rq: desc consistent warning:"
				"tx_rq_rd_offset = 0x%x, "
				"tx_rq_wr_offset = 0x%x, "
				"tx_fq.skb[%d](0x%p)\n",
				tx_rq_rd_offset, tx_rq_wr_offset,
				pos, ld->tx_bq.skb[pos]);
			/*
			 * logic bug, cause it update tx_rq_wr pointer first
			 * before loading the desc from fifo into tx_rq.
			 * so try to read it again until desc reached the tx_rq
			 * FIXME: use volatile or __iomem to avoid compiler
			 * optimize?
			 */
			goto reload;
		}

		if (ld->tx_bq.skb[pos] != skb) {
			pr_err("wired, tx skb[%d](%p) != skb(%p)\n",
					pos, ld->tx_bq.skb[pos], skb);
			if (ld->tx_bq.skb[pos] == SKB_MAGIC)
				goto next;
		}

		/* data consistent check */
		ld->tx_rq.use_cnt++;
		if (ld->tx_rq.use_cnt != tx_rq_desc->reserve5)
			pr_err("desc pointer ERROR!!! ld->tx_rq.use_cnt is 0x%x, "
					"tx_rq_desc->reserve5 = 0x%x\n",
					ld->tx_rq.use_cnt, tx_rq_desc->reserve5);

		dma_addr = tx_rq_desc->data_buff_addr;
		dma_unmap_single(ld->dev, dma_addr, skb->len, DMA_TO_DEVICE);

		ld->tx_bq.skb[pos] = NULL;
		dev_kfree_skb_any(skb);
next:
		tx_rq_desc->skb_buff_addr = 0;

		tx_rq_rd_offset += DESC_SIZE;
		if (tx_rq_rd_offset == (HIGMAC_HWQ_TX_RQ_DEPTH << 5))
				tx_rq_rd_offset = 0;

		higmac_writel(tx_rq_rd_offset, TX_RQ_RD_ADDR);
		release = true;
	}

	if (release == true && netif_queue_stopped(ld->netdev)) {
		netif_wake_queue(ld->netdev);
		if (debug(HW_TX_DESC))
			pr_info("netif_wake_queue(gmac%d)\n", ld->index);
	}

	return ret;
}

int higmac_xmit_real_send(struct higmac_netdev_local *ld, struct sk_buff *skb)
{
	int tx_bq_wr_offset, tx_bq_rd_offset, tmp, pos;
	struct higmac_desc *tx_bq_desc;
	unsigned long txflags;

	/* software write pointer */
	tx_bq_wr_offset = higmac_readl(TX_BQ_WR_ADDR);
	/* logic read pointer */
	tx_bq_rd_offset = higmac_readl(TX_BQ_RD_ADDR);

	pos = tx_bq_wr_offset >> 5;
	tmp = tx_bq_wr_offset + DESC_SIZE;
	if (tmp == (HIGMAC_HWQ_TX_BQ_DEPTH << 5))
		tmp = 0;
	if (tmp == tx_bq_rd_offset || ld->tx_bq.skb[pos]) {
		if (debug(HW_TX_DESC))
			pr_err("tx queue is full! tx_bq: wr=0x%x, rd=0x%x, "
				"tx_rq: wr=0x%x, rd=0x%x, sf=0x%x, hw=0x%x\n",
				tx_bq_wr_offset, tx_bq_rd_offset,
				higmac_readl(TX_RQ_WR_ADDR),
				higmac_readl(TX_RQ_RD_ADDR),
				pos, higmac_readl(TX_RQ_WR_ADDR) >> 5);
		/* we will stop the queue outside of this func */
		return -EBUSY;
	}

	spin_lock_irqsave(&ld->txlock, txflags);
	if (unlikely(ld->tx_bq.skb[pos])) {
		spin_unlock_irqrestore(&ld->txlock, txflags);
		return -EBUSY;
	} else
		ld->tx_bq.skb[pos] = skb;
	spin_unlock_irqrestore(&ld->txlock, txflags);

	tx_bq_desc = ld->tx_bq.desc + pos;

	tx_bq_desc->skb_buff_addr = skb;
	tx_bq_desc->data_buff_addr =
		dma_map_single(ld->dev, skb->data, skb->len, DMA_TO_DEVICE);
	WARN_ON(dma_mapping_error(ld->dev,tx_bq_desc->data_buff_addr));
	tx_bq_desc->buffer_len = HIETH_MAX_FRAME_SIZE - 1;
	tx_bq_desc->data_len = skb->len;
	tx_bq_desc->fl = DESC_FL_FULL;
	tx_bq_desc->descvid = DESC_VLD_BUSY;

	ld->tx_bq.use_cnt++;
	tx_bq_desc->reserve5 = ld->tx_bq.use_cnt;

	tx_bq_wr_offset += DESC_SIZE;
	if (tx_bq_wr_offset >= (HIGMAC_HWQ_TX_BQ_DEPTH << 5))
		tx_bq_wr_offset = 0;
	/* update software write pointer */
	higmac_writel(tx_bq_wr_offset, TX_BQ_WR_ADDR);

	return 0;
}

int higmac_feed_hw(struct higmac_netdev_local *ld)
{
	int rx_fq_wr_offset, rx_fq_rd_offset;
	struct higmac_desc *rx_fq_desc;
	struct sk_buff *skb;
	int wr_rd_dist;
	int i = 0;
	int start, end, num = 0;

	rx_fq_wr_offset = higmac_readl(RX_FQ_WR_ADDR);/* software write pointer */
	rx_fq_rd_offset = higmac_readl(RX_FQ_RD_ADDR);/* logic read pointer */

	if (rx_fq_wr_offset >= rx_fq_rd_offset)
		wr_rd_dist = (HIGMAC_HWQ_RX_FQ_DEPTH << 5)
				- (rx_fq_wr_offset - rx_fq_rd_offset);
	else
		wr_rd_dist = rx_fq_rd_offset - rx_fq_wr_offset;

	wr_rd_dist >>= 5;/* offset was counted on bytes, desc size = 2^5 */

	higmac_trace(5, "rx_fq_wr_offset is %x, rx_fq_rd_offset is %x\n",
			rx_fq_wr_offset, rx_fq_rd_offset);

	start = rx_fq_wr_offset >> 5;
	/*
	 * wr_rd_dist - 1 for logic reason.
	 * Logic think the desc pool is full filled, ...?
	 */
	for (i = 0; i < wr_rd_dist - 1; i++) {
		int pos = rx_fq_wr_offset >> 5;

		if (ld->rx_fq.skb[pos])
			goto out;
		else {
			skb = dev_alloc_skb(SKB_SIZE);
			if (!skb)
				goto out;

			ld->rx_fq.skb[pos] = skb;
			num++;
		}

		rx_fq_desc = ld->rx_fq.desc + (rx_fq_wr_offset >> 5);

		skb_reserve(skb, 2);
		rx_fq_desc->data_buff_addr =
			dma_map_single(ld->dev, skb->data,
					HIETH_MAX_FRAME_SIZE, DMA_FROM_DEVICE);
		WARN_ON(dma_mapping_error(ld->dev,rx_fq_desc->data_buff_addr));
		rx_fq_desc->buffer_len = HIETH_MAX_FRAME_SIZE - 1;
		rx_fq_desc->data_len = 0;
		rx_fq_desc->fl = 0;
		rx_fq_desc->descvid = DESC_VLD_FREE;
		rx_fq_desc->skb_buff_addr = skb;

		ld->rx_fq.use_cnt++;
		rx_fq_desc->reserve5 = ld->rx_fq.use_cnt;

		rx_fq_wr_offset += DESC_SIZE;
		if (rx_fq_wr_offset >= (HIGMAC_HWQ_RX_FQ_DEPTH << 5))
			rx_fq_wr_offset = 0;

		higmac_writel(rx_fq_wr_offset, RX_FQ_WR_ADDR);
	}
out:
	end = rx_fq_wr_offset >> 5;
	if (debug(HW_RX_DESC))
		pr_info("gmac%d feed skb[%d-%d)\n", ld->index, start, end);

	return end - start;
}
