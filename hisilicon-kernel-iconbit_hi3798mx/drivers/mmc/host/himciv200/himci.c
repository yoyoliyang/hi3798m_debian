/*
 * himci.c - hisilicon MMC Host driver
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt) "himci: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sd.h>

#include <linux/ioport.h>
#include <linux/device.h>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/sizes.h>
#include <mach/hardware.h>
#include <linux/version.h>
#include <asm/io.h>

#include "himci_reg.h"
#include "himci.h"
#include "himci_dbg.h"

#include <mach/cpu-info.h>

/*************************************************************************/
#ifdef CONFIG_ARCH_GODBOX
#  include "himci_godbox.c"
#endif

#if defined(CONFIG_ARCH_S40) || defined(CONFIG_ARCH_HI3798MX)
#  include "himci_s40.c"
#endif

/*************************************************************************/
#define DRIVER_NAME "hi_mci"

static unsigned int detect_time = HI_MCI_DETECT_TIMEOUT;
static unsigned int retry_count = MAX_RETRY_COUNT;
static unsigned int request_timeout = HI_MCI_REQUEST_TIMEOUT;
int trace_level = HIMCI_TRACE_LEVEL;

#ifdef MODULE

module_param(detect_time, uint, 0600);
MODULE_PARM_DESC(detect_timer, "card detect time (default:500ms))");

module_param(retry_count, uint, 0600);
MODULE_PARM_DESC(retry_count, "retry count times (default:100))");

module_param(request_timeout, uint, 0600);
MODULE_PARM_DESC(request_timeout, "Request timeout time (default:3s))");

module_param(trace_level, int, 0600);
MODULE_PARM_DESC(trace_level, "HIMCI_TRACE_LEVEL");

#endif

/* get MMC host controler pointer for sdio wifi interface
 * hostid: sdio number ( sdio0:  0  sdio1:  1 )
*/
extern struct kset *devices_kset;
struct mmc_host * get_mmchost(int hostid)
{
	struct device *dev = NULL;
	struct mmc_host * host = NULL;
	struct list_head * list = NULL;
	char name[5] ="mmc1";

	/* sdio0 is mmc1; sdio1 is mmc0 because we register SDIO1 first*/
	if (hostid == 0) {
		name[3] = '1';
	} else if (hostid == 1) {
		name[3] = '0';
	} else {
		return NULL;
	}

	spin_lock(&devices_kset->list_lock);
	/* Walk the devices list backward */
	list = &devices_kset->list;
	while (!list_empty(list)) {
		dev = list_entry(list->prev, struct device,kobj.entry);

		get_device(dev);

		if (strcmp(dev_name(dev), name) == 0) {
			host = container_of(dev, struct mmc_host, class_dev);
			put_device(dev);
			spin_unlock(&devices_kset->list_lock);
			return host;
		}
		list = list->prev;

		put_device(dev);
	}
	spin_unlock(&devices_kset->list_lock);

	return host;
}
EXPORT_SYMBOL(get_mmchost);

/* reset MMC host controler */
static void himciv200_sys_reset(struct himci_host *host)
{
	unsigned int reg_value;
	unsigned long flags;

	local_irq_save(flags);

	reg_value = himci_readl(host->base + MCI_BMOD);
	reg_value |= BMOD_SWR;
	himci_writel(reg_value, host->base + MCI_BMOD);
	mdelay(10);

	reg_value = himci_readl(host->base + MCI_BMOD);
	reg_value |= BURST_16 | BURST_INCR;
	himci_writel(reg_value, host->base + MCI_BMOD);

	reg_value = himci_readl(host->base + MCI_CTRL);
	reg_value |= CTRL_RESET | FIFO_RESET | DMA_RESET;
	himci_writel(reg_value, host->base + MCI_CTRL);

	local_irq_restore(flags);
}

static void hi_mci_ctrl_power(struct himci_host *host, unsigned int flag, unsigned int force)
{
	himci_trace(2, "begin");

	if (host->power_status != flag || force == FORCE_ENABLE) {
		if (flag == POWER_OFF)
			himci_writel(0, host->base + MCI_RESET_N);

		himci_writel(flag, host->base + MCI_PWREN);

		if (flag == POWER_ON)
			himci_writel(1, host->base + MCI_RESET_N);

		if (in_interrupt())
			mdelay(100);
		else
			msleep(100);

		host->power_status = flag;
	}
}

/**********************************************
 *1: card off
 *0: card on
 ***********************************************/
static unsigned int hi_mci_sys_card_detect(struct himci_host *host)
{
	unsigned int card_status;

	card_status = readl(host->base + MCI_CDETECT);

	return card_status & HIMCI_CARD0;
}

/**********************************************
 *1: card readonly
 *0: card read/write
 ***********************************************/
static unsigned int hi_mci_ctrl_card_readonly(struct himci_host *host)
{
	unsigned int card_value = himci_readl(host->base + MCI_WRTPRT);
	return card_value & HIMCI_CARD0;
}

static int hi_mci_wait_cmd(struct himci_host *host)
{
	int wait_retry_count = 0;
	unsigned int reg_data = 0;
	unsigned long flags;

	while (1) {
		/*
		   Check if CMD::start_cmd bit is clear.
		   start_cmd = 0 means MMC Host controller has loaded registers
		   and next command can be loaded in.
		 */
		reg_data = readl(host->base + MCI_CMD);
		if ((reg_data & START_CMD) == 0)
			return 0;

		/* Check if Raw_Intr_Status::HLE bit is set. */
		spin_lock_irqsave(&host->lock, flags);
		reg_data = readl(host->base + MCI_RINTSTS);
		if (reg_data & HLE_INT_STATUS) {
			reg_data |= HLE_INT_STATUS;
			himci_writel(reg_data, host->base + MCI_RINTSTS);
			spin_unlock_irqrestore(&host->lock, flags);

			himci_trace(3, "Other CMD is running,"
				    "please operate cmd again!");
			return 1;
		}
		spin_unlock_irqrestore(&host->lock, flags);
		udelay(100);

		/* Check if number of retries for this are over. */
		wait_retry_count++;
		if (wait_retry_count >= retry_count) {
			himci_trace(3, "wait cmd complete is timeout!");
			return -1;
		}
		schedule();
	}
}

static void hi_mci_control_cclk(struct himci_host *host, unsigned int flag)
{
	unsigned int reg;
	cmd_arg_s cmd_reg;

	himci_trace(2, "begin");
	himci_assert(host);

	reg = himci_readl(host->base + MCI_CLKENA);
	if (flag == ENABLE)
		reg |= CCLK_ENABLE;
	else
		reg &= 0xffff0000;

	himci_writel(reg, host->base + MCI_CLKENA);

	cmd_reg.cmd_arg = himci_readl(host->base + MCI_CMD);
	cmd_reg.bits.start_cmd = 1;
	cmd_reg.bits.update_clk_reg_only = 1;
	himci_writel(cmd_reg.cmd_arg, host->base + MCI_CMD);

	if (hi_mci_wait_cmd(host) != 0)
		himci_trace(3, "disable or enable clk is timeout!");
}

static void hi_mci_set_cclk(struct himci_host *host, unsigned int cclk)
{
	unsigned int reg_value;
	cmd_arg_s clk_cmd;

	himci_trace(2, "begin");
	himci_assert(host);
	himci_assert(cclk);

	/*set card clk divider value, clk_divider = Fmmcclk/(Fmmc_cclk * 2) */
	reg_value = 0;
	if (cclk < MMC_CLK) {
		reg_value = MMC_CLK / (cclk * 2);
		if (MMC_CLK % (cclk * 2))
			reg_value++;
		if (reg_value > 0xFF)
			reg_value = 0xFF;
	}
	himci_writel(reg_value, host->base + MCI_CLKDIV);

	clk_cmd.cmd_arg = himci_readl(host->base + MCI_CMD);
	clk_cmd.bits.start_cmd = 1;
	clk_cmd.bits.update_clk_reg_only = 1;
	himci_writel(clk_cmd.cmd_arg, host->base + MCI_CMD);
	if (hi_mci_wait_cmd(host) != 0)
		himci_trace(3, "set card clk divider is failed!");
}

static void hi_mci_init_card(struct himci_host *host)
{
	unsigned int tmp_reg;
	unsigned long flags;

	himci_trace(2, "begin");
	himci_assert(host);

	hi_mci_ctrl_power(host, POWER_OFF, FORCE_ENABLE);
	/* card power on */
	hi_mci_ctrl_power(host, POWER_ON, FORCE_ENABLE);

	himciv200_sys_reset(host);

	/* clear MMC host intr */
	himci_writel(ALL_INT_CLR, host->base + MCI_RINTSTS);

	spin_lock_irqsave(&host->lock, flags);
	host->pending_events = 0;
	spin_unlock_irqrestore(&host->lock, flags);

	/* MASK MMC host intr */
	tmp_reg = himci_readl(host->base + MCI_INTMASK);
	tmp_reg &= ~ALL_INT_MASK;
	tmp_reg |= DTO_INT_MASK | CD_INT_MASK;
	himci_writel(tmp_reg, host->base + MCI_INTMASK);

	/* enable inner DMA mode and close intr of MMC host controler */
	tmp_reg = himci_readl(host->base + MCI_CTRL);
	tmp_reg &= ~INTR_EN;
	tmp_reg |= USE_INTERNAL_DMA | INTR_EN;
	himci_writel(tmp_reg, host->base + MCI_CTRL);

	/* set timeout param */
	himci_writel(DATA_TIMEOUT | RESPONSE_TIMEOUT, host->base + MCI_TIMEOUT);

	/* set FIFO param */
	tmp_reg = 0;
	tmp_reg |= BURST_SIZE | RX_WMARK | TX_WMARK;
	himci_writel(tmp_reg, host->base + MCI_FIFOTH);
}

static void hi_mci_detect_card(unsigned long arg)
{
	struct himci_host *host = (struct himci_host *)arg;
	unsigned int i, curr_status, status[3], detect_retry_count = 0;

	himci_assert(host);

	while (1) {
		for (i = 0; i < 3; i++) {
			status[i] = hi_mci_sys_card_detect(host);
			udelay(10);
		}
		if ((status[0] == status[1]) && (status[0] == status[2]))
			break;
		detect_retry_count++;
		if (detect_retry_count >= retry_count) {
			himci_error("this is a dithering,"
				    "card detect error!");
			goto err;
		}
	}
	curr_status = status[0];
	if (curr_status != host->card_status) {
		himci_trace(2, "begin card_status = %d\n", host->card_status);
		host->card_status = curr_status;
		if (curr_status != CARD_UNPLUGED) {
			hi_mci_init_card(host);
			printk(KERN_INFO "card connected!\n");
		} else
			printk(KERN_INFO "card disconnected!\n");

		mmc_detect_change(host->mmc, 0);
	}
err:
	mod_timer(&host->timer, jiffies + detect_time);
}

static void hi_mci_idma_start(struct himci_host *host)
{
	unsigned int tmp;

	himci_trace(2, "begin");
	himci_writel(host->dma_paddr, host->base + MCI_DBADDR);
	tmp = himci_readl(host->base + MCI_BMOD);
	tmp |= BMOD_DMA_EN;
	himci_writel(tmp, host->base + MCI_BMOD);
}

static void hi_mci_idma_stop(struct himci_host *host)
{
	unsigned int tmp_reg;

	himci_trace(2, "begin");
	tmp_reg = himci_readl(host->base + MCI_BMOD);
	tmp_reg &= ~BMOD_DMA_EN;
	himci_writel(tmp_reg, host->base + MCI_BMOD);
}

static int hi_mci_setup_data(struct himci_host *host, struct mmc_data *data)
{
	unsigned int sg_phyaddr, sg_length;
	unsigned int i, ret = 0;
	unsigned int data_size;
	unsigned int max_des, des_cnt;
	struct himci_des *des;

	himci_trace(2, "begin");
	himci_assert(host);
	himci_assert(data);

	host->data = data;

	if (data->flags & MMC_DATA_READ)
		host->dma_dir = DMA_FROM_DEVICE;
	else
		host->dma_dir = DMA_TO_DEVICE;

	host->dma_sg = data->sg;
	host->dma_sg_num = dma_map_sg(mmc_dev(host->mmc), data->sg,
				      data->sg_len, host->dma_dir);
	himci_assert(host->dma_sg_num);
	himci_trace(2, "host->dma_sg_num is %d\n", host->dma_sg_num);

	data_size = data->blksz * data->blocks;
	if (data_size > (DMA_BUFFER * MAX_DMA_DES)) {
		himci_error("mci request data_size is too big!\n");
		ret = -1;
		goto out;
	}

	himci_trace(2, "host->dma_paddr is 0x%08X,host->dma_vaddr is 0x%08X\n",
		    (unsigned int)host->dma_paddr,
		    (unsigned int)host->dma_vaddr);

	max_des = (PAGE_SIZE / sizeof(struct himci_des));
	des = (struct himci_des *)host->dma_vaddr;
	des_cnt = 0;

	for (i = 0; i < host->dma_sg_num; i++) {
		sg_length = sg_dma_len(&data->sg[i]);
		sg_phyaddr = sg_dma_address(&data->sg[i]);
		himci_trace(2, "sg[%d] sg_length is 0x%08X,"
			    " sg_phyaddr is 0x%08X\n",
			    i,
			    (unsigned int)sg_length, (unsigned int)sg_phyaddr);
		while (sg_length) {
			des[des_cnt].idmac_des_ctrl =
			    DMA_DES_OWN | DMA_DES_NEXT_DES;
			des[des_cnt].idmac_des_buf_addr = sg_phyaddr;
			/* idmac_des_next_addr is paddr for dma */
			des[des_cnt].idmac_des_next_addr = host->dma_paddr +
			    (des_cnt + 1) * sizeof(struct himci_des);

			if (sg_length >= 0x1F00) {
				des[des_cnt].idmac_des_buf_size = 0x1F00;
				sg_length -= 0x1F00;
				sg_phyaddr += 0x1F00;
			} else {
				/* data alignment */
				des[des_cnt].idmac_des_buf_size = sg_length;
				sg_length = 0;
			}
			himci_trace(2, "des[%d] vaddr  is 0x%08X",
				    i, (unsigned int)&des[i]);
			himci_trace(2, "des[%d].idmac_des_ctrl is 0x%08X",
				    i, (unsigned int)des[i].idmac_des_ctrl);
			himci_trace(2, "des[%d].idmac_des_buf_size is 0x%08X",
				    i, (unsigned int)des[i].idmac_des_buf_size);
			himci_trace(2, "des[%d].idmac_des_buf_addr 0x%08X",
				    i, (unsigned int)des[i].idmac_des_buf_addr);
			himci_trace(2, "des[%d].idmac_des_next_addr is 0x%08X",
				    i,
				    (unsigned int)des[i].idmac_des_next_addr);
			des_cnt++;
		}

		himci_assert(des_cnt < max_des);
	}
	des[0].idmac_des_ctrl |= DMA_DES_FIRST_DES;
	des[des_cnt - 1].idmac_des_ctrl |= DMA_DES_LAST_DES;
	des[des_cnt - 1].idmac_des_next_addr = 0;
out:
	return ret;
}

static int hi_mci_exec_cmd(struct himci_host *host,
			   struct mmc_command *cmd, struct mmc_data *data)
{
	volatile cmd_arg_s cmd_regs;

	himci_trace(2, "begin");
	himci_assert(host);
	himci_assert(cmd);

	host->cmd = cmd;

	himci_writel(cmd->arg, host->base + MCI_CMDARG);

	cmd_regs.cmd_arg = himci_readl(host->base + MCI_CMD);
	if (data) {
		cmd_regs.bits.data_transfer_expected = 1;
		if (data->flags & (MMC_DATA_WRITE | MMC_DATA_READ))
			cmd_regs.bits.transfer_mode = 0;
		if (data->flags & MMC_DATA_STREAM)
			cmd_regs.bits.transfer_mode = 1;
		if (data->flags & MMC_DATA_WRITE)
			cmd_regs.bits.read_write = 1;
		else if (data->flags & MMC_DATA_READ)
			cmd_regs.bits.read_write = 0;
	} else {
		cmd_regs.bits.data_transfer_expected = 0;
		cmd_regs.bits.transfer_mode = 0;
		cmd_regs.bits.read_write = 0;
	}

	if (cmd == host->mrq->stop) {
		cmd_regs.bits.stop_abort_cmd = 1;
		cmd_regs.bits.wait_prvdata_complete = 0;
	} else {
		cmd_regs.bits.stop_abort_cmd = 0;
		cmd_regs.bits.wait_prvdata_complete = 1;
	}

	switch (mmc_resp_type(cmd)) {
	case MMC_RSP_NONE:
		cmd_regs.bits.response_expect = 0;
		cmd_regs.bits.response_length = 0;
		cmd_regs.bits.check_response_crc = 0;
		break;
	case MMC_RSP_R1:
	case MMC_RSP_R1B:
		cmd_regs.bits.response_expect = 1;
		cmd_regs.bits.response_length = 0;
		cmd_regs.bits.check_response_crc = 1;
		break;
	case MMC_RSP_R2:
		cmd_regs.bits.response_expect = 1;
		cmd_regs.bits.response_length = 1;
		cmd_regs.bits.check_response_crc = 1;
		break;
	case MMC_RSP_R3:
		cmd_regs.bits.response_expect = 1;
		cmd_regs.bits.response_length = 0;
		cmd_regs.bits.check_response_crc = 0;
		break;
	default:
		himci_error("hi_mci: unhandled response type %02x\n",
			    mmc_resp_type(cmd));
		return -EINVAL;
	}

	himci_trace(3, "send cmd of card is cmd->opcode = %d cmd->arg = 0x%X\n", cmd->opcode, cmd->arg);
	if (cmd->opcode == MMC_GO_IDLE_STATE)
		cmd_regs.bits.send_initialization = 1;
	else
		cmd_regs.bits.send_initialization = 0;
	if (cmd->opcode == SD_SWITCH_VOLTAGE)
		cmd_regs.bits.volt_switch = 1;
	else
		cmd_regs.bits.volt_switch = 0;

	cmd_regs.bits.card_number = 0;
	cmd_regs.bits.cmd_index = cmd->opcode;
	cmd_regs.bits.send_auto_stop = 0;
	cmd_regs.bits.start_cmd = 1;
	cmd_regs.bits.update_clk_reg_only = 0;
	himci_writel(cmd_regs.cmd_arg, host->base + MCI_CMD);

	if (hi_mci_wait_cmd(host) != 0) {
		himci_trace(3, "send card cmd is failed!");
		return -EINVAL;
	}
	return 0;
}

static void hi_mci_finish_request(struct himci_host *host,
				  struct mmc_request *mrq)
{
	himci_trace(2, "begin");
	himci_assert(host);
	himci_assert(mrq);

	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;
	mmc_request_done(host->mmc, mrq);
}

static void hi_mci_cmd_done(struct himci_host *host, unsigned int stat)
{
	unsigned int i;
	struct mmc_command *cmd = host->cmd;

	himci_trace(2, "begin");
	himci_assert(host);
	himci_assert(cmd);

	host->cmd = NULL;

	for (i = 0; i < 4; i++) {
		if (mmc_resp_type(cmd) == MMC_RSP_R2)
			cmd->resp[i] = himci_readl(host->base +
						   MCI_RESP3 - i * 0x4);
		else
			cmd->resp[i] = himci_readl(host->base +
						   MCI_RESP0 + i * 0x4);
	}

	if (stat & RTO_INT_STATUS) {
		cmd->error = -ETIMEDOUT;
		himci_trace(3, "irq cmd status stat = 0x%x is timeout error!",
			    stat);
	} else if (stat & (RCRC_INT_STATUS | RE_INT_STATUS)) {
		cmd->error = -EILSEQ;
		himci_error( "irq cmd status stat = 0x%x is response error!",
			    stat);
	}
}

static void hi_mci_data_done(struct himci_host *host, unsigned int stat)
{
	struct mmc_data *data = host->data;

	himci_trace(2, "begin");
	himci_assert(host);
	himci_assert(data);

	dma_unmap_sg(mmc_dev(host->mmc), data->sg, data->sg_len, host->dma_dir);

	if (stat & (HTO_INT_STATUS | DRTO_INT_STATUS)) {
		data->error = -ETIMEDOUT;
		himci_error("irq data status stat = 0x%x is timeout error!",
			    stat);
	} else if (stat & (EBE_INT_STATUS | SBE_INT_STATUS |
			   FRUN_INT_STATUS | DCRC_INT_STATUS)) {
		data->error = -EILSEQ;
		himci_error("irq data status stat = 0x%x is data error!",
			    stat);
	}
	if (!data->error)
		data->bytes_xfered = data->blocks * data->blksz;
	else
		data->bytes_xfered = 0;
	host->data = NULL;
}

static int hi_mci_wait_cmd_complete(struct himci_host *host)
{
	unsigned int cmd_irq_reg = 0;
	struct mmc_command *cmd = host->cmd;
	unsigned long flags;
	long time = request_timeout;

	himci_trace(2, "begin");
	himci_assert(host);
	himci_assert(cmd);

	time = wait_event_timeout(host->intr_wait,
				  test_bit(HIMCI_PEND_CD_b,
					   &host->pending_events), time);

	if ((time <= 0)
		&& (!test_bit(HIMCI_PEND_CD_b, &host->pending_events))) {
		cmd->error = -ETIMEDOUT;
		spin_lock_irqsave(&host->lock, flags);
		cmd_irq_reg = himci_readl(host->base + MCI_RINTSTS);
		spin_unlock_irqrestore(&host->lock, flags);
		himci_trace(3, "wait cmd request complete is timeout! Raw interrupt status 0x%08X\n", cmd_irq_reg);
		return -1;
	}
	spin_lock_irqsave(&host->lock, flags);
	cmd_irq_reg = himci_readl(host->base + MCI_RINTSTS);
	himci_writel((CD_INT_STATUS | RTO_INT_STATUS |
		RCRC_INT_STATUS | RE_INT_STATUS), host->base + MCI_RINTSTS);
	host->pending_events &= ~HIMCI_PEND_CD_m;
	spin_unlock_irqrestore(&host->lock, flags);
	hi_mci_cmd_done(host, cmd_irq_reg);
	return 0;
}

static int hi_mci_wait_data_complete(struct himci_host *host)
{
	unsigned int data_irq_reg = 0;
	struct mmc_data *data = host->data;
	long time = request_timeout;
	unsigned long flags;

	himci_trace(2, "begin");
	himci_assert(host);
	himci_assert(data);

	time = wait_event_timeout(host->intr_wait,
				  test_bit(HIMCI_PEND_DTO_b,
					   &host->pending_events), time);

	if ((time <= 0)
	    && (!test_bit(HIMCI_PEND_DTO_b, &host->pending_events))) {
		data->error = -ETIMEDOUT;
		spin_lock_irqsave(&host->lock, flags);
		data_irq_reg = himci_readl(host->base + MCI_RINTSTS);
		spin_unlock_irqrestore(&host->lock, flags);
		himci_trace(3, "wait data request complete is timeout! 0x%08X",
			    data_irq_reg);
		return -1;
	}

	spin_lock_irqsave(&host->lock, flags);
	data_irq_reg = himci_readl(host->base + MCI_RINTSTS);
	himci_writel((HTO_INT_STATUS | DRTO_INT_STATUS | EBE_INT_STATUS
		      | SBE_INT_STATUS | FRUN_INT_STATUS | DCRC_INT_STATUS),
		     host->base + MCI_RINTSTS);

	host->pending_events &= ~HIMCI_PEND_DTO_m;
	spin_unlock_irqrestore(&host->lock, flags);

	hi_mci_idma_stop(host);
	hi_mci_data_done(host, data_irq_reg);

	return 0;
}

static int hi_mci_wait_card_complete(struct himci_host *host)
{
	unsigned int card_retry_count = 0;
	unsigned long card_jiffies_timeout;
	unsigned int card_status_reg = 0;

	himci_trace(2, "begin");
	himci_assert(host);

	card_jiffies_timeout = jiffies + request_timeout;
	while (1) {
		if (!time_before(jiffies, card_jiffies_timeout)) {
			himci_trace(3, "wait card ready complete is timeout!");
			return -1;
		}

		do {
			card_status_reg = readl(host->base + MCI_STATUS);
			if (!(card_status_reg & DATA_BUSY)) {
				himci_trace(2, "end");
				return 0;
			}
			card_retry_count++;
		} while (card_retry_count < retry_count);
		schedule();
	}
}

static void hi_mci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct himci_host *host = mmc_priv(mmc);
	unsigned int rw = 0;
	unsigned int ofblk = 0, nrblk = 0;
	unsigned int blk_size = 0;
	unsigned int tmp_reg, fifo_count = 0;
	int ret = 0;

	himci_trace(2, "begin");
	himci_assert(mmc);
	himci_assert(mrq);
	himci_assert(host);

	host->mrq = mrq;

	if (host->card_status == CARD_UNPLUGED) {
		mrq->cmd->error = -ENODEV;
		goto request_end;
	}

	ret = hi_mci_wait_card_complete(host);
	if (ret) {
		mrq->cmd->error = ret;
		goto request_end;
	}

	/* prepare data */
	if (mrq->data) {
		nrblk = mrq->data->blocks;

		if (mrq->cmd->opcode == MMC_READ_SINGLE_BLOCK ||
		    mrq->cmd->opcode == MMC_READ_MULTIPLE_BLOCK) {
			rw = MMC_DATA_READ;
			ofblk = mrq->cmd->arg;
		}

		if (mrq->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK ||
		    mrq->cmd->opcode == MMC_WRITE_BLOCK) {
			rw = MMC_DATA_WRITE;
			ofblk = mrq->cmd->arg;
		}

		ret = hi_mci_setup_data(host, mrq->data);
		if (ret) {
			mrq->data->error = ret;
			himci_trace(3, "data setup is error!");
			goto request_end;
		}

		blk_size = mrq->data->blksz * mrq->data->blocks;
		himci_writel(blk_size, host->base + MCI_BYTCNT);
		himci_writel(mrq->data->blksz, host->base + MCI_BLKSIZ);

		tmp_reg = himci_readl(host->base + MCI_CTRL);
		tmp_reg |= FIFO_RESET;
		himci_writel(tmp_reg, host->base + MCI_CTRL);

		do {
			tmp_reg = himci_readl(host->base + MCI_CTRL);
			fifo_count++;
			if (fifo_count >= retry_count) {
				printk(KERN_INFO "fifo reset is timeout!");
				return;
			}
		} while (tmp_reg & FIFO_RESET);

		/* start DMA */
		hi_mci_idma_start(host);
	} else {
		himci_writel(0, host->base + MCI_BYTCNT);
		himci_writel(0, host->base + MCI_BLKSIZ);
	}

	ret = hi_mci_exec_cmd(host, mrq->cmd, mrq->data);
	if (ret) {
		mrq->cmd->error = ret;
		himci_trace(3, "cmd execute is error!");
		goto request_end;
	}

	/* wait command send complete */
	ret = hi_mci_wait_cmd_complete(host);
	if (ret)
		goto request_end;
	if (!(mrq->data && !mrq->cmd->error))
		goto request_end;

	/* start data transfer */
	if (mrq->data) {
		/* wait data transfer complete */
		ret = hi_mci_wait_data_complete(host);
		if (ret)
			goto request_end;
		if (mrq->stop) {
			/* send stop command */
			ret = hi_mci_exec_cmd(host, host->mrq->stop,
					      host->data);
			if (ret) {
				mrq->cmd->error = ret;
				goto request_end;
			}
			ret = hi_mci_wait_cmd_complete(host);
			if (ret)
				goto request_end;
		}

		if (rw == MMC_DATA_WRITE)
			himci_dbg_rw(host->devid, 1, ofblk, nrblk);

		if (rw == MMC_DATA_READ)
			himci_dbg_rw(host->devid, 0, ofblk, nrblk);
	}

request_end:
	hi_mci_finish_request(host, mrq);
}

static int hi_mci_do_start_signal_voltage_switch(struct himci_host *host,
						 struct mmc_ios *ios)
{
	u32 ctrl;
	/*
	 * We first check whether the request is to set signalling voltage
	 * to 3.3V. If so, we change the voltage to 3.3V and return quickly.
	 */
	ctrl = himci_readl(host->base + MCI_UHS_REG);
	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		/* Set 1.8V Signal Enable in the MCI_UHS_REG to 1 */
		ctrl &= ~ENABLE_UHS_VDD_180;
		himci_writel(ctrl, host->base + MCI_UHS_REG);
		himci_ldo_config(0);

		/* Wait for 5ms */
		usleep_range(5000, 5500);

		/* 3.3V regulator output should be stable within 5 ms */
		ctrl = himci_readl(host->base + MCI_UHS_REG);
		if (!(ctrl & ENABLE_UHS_VDD_180)) {
			return 0;
		} else {
			himci_error(": Switching to 3.3V "
				    "signalling voltage failed\n");
			return -EIO;
		}
	} else if (!(ctrl & ENABLE_UHS_VDD_180) &&
		   (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)) {
		/* Stop SDCLK */
		hi_mci_control_cclk(host, DISABLE);

		/*
		 * Enable 1.8V Signal Enable in the MCI_UHS_REG
		 */
		ctrl |= ENABLE_UHS_VDD_180;
		himci_writel(ctrl, host->base + MCI_UHS_REG);
		himci_ldo_config(1);

		/* Wait for 8ms */
		usleep_range(8000, 8500);

		ctrl = himci_readl(host->base + MCI_UHS_REG);
		if (ctrl & ENABLE_UHS_VDD_180) {
			/* Provide SDCLK again and wait for 1ms */
			hi_mci_control_cclk(host, ENABLE);
			usleep_range(1000, 1500);

			/*
			 * If CMD11 return CMD down, then the card
			 * was successfully switched to 1.8V signaling.
			 */
			ctrl = himci_readl(host->base + MCI_RINTSTS);
			if ((ctrl & VOLT_SWITCH_INT_STATUS)
			    && (ctrl & CD_INT_STATUS)) {
				writel(VOLT_SWITCH_INT_STATUS | CD_INT_STATUS,
				       host->base + MCI_RINTSTS);
				return 0;
			}
		}

		/*
		 * If we are here, that means the switch to 1.8V signaling
		 * failed. We power cycle the card, and retry initialization
		 * sequence by setting S18R to 0.
		 */

		ctrl &= ~ENABLE_UHS_VDD_180;
		himci_writel(ctrl, host->base + MCI_UHS_REG);
		himci_ldo_config(0);

		/* Wait for 5ms */
		usleep_range(5000, 5500);

		hi_mci_ctrl_power(host, POWER_OFF, FORCE_DISABLE);
		/* Wait for 1ms as per the spec */
		usleep_range(1000, 1500);
		hi_mci_ctrl_power(host, POWER_ON, FORCE_DISABLE);

		hi_mci_control_cclk(host, DISABLE);
		/* Wait for 1ms as per the spec */
		usleep_range(1000, 1500);
		hi_mci_control_cclk(host, ENABLE);

		himci_error( ": Switching to 1.8V signalling "
			    "voltage failed, retrying with S18R set to 0\n");
		return -EAGAIN;
	} else{
		/* No signal voltage switch required */
		return 0;
	}
}

static int hi_mci_start_signal_voltage_switch(struct mmc_host *mmc,
					      struct mmc_ios *ios)
{
	struct himci_host *host = mmc_priv(mmc);
	int err;
	err = hi_mci_do_start_signal_voltage_switch(host, ios);
	return err;
}

static void hi_mci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct himci_host *host = mmc_priv(mmc);
	unsigned int tmp_reg;
	u32 ctrl;

	himci_trace(2, "begin");
	himci_assert(mmc);
	himci_assert(ios);
	himci_assert(host);

	himci_trace(3, "ios->power_mode = %d ", ios->power_mode);
	if (!ios->clock) {
		hi_mci_control_cclk(host, DISABLE);
	}

	switch (ios->power_mode) {
	case MMC_POWER_OFF:
		hi_mci_ctrl_power(host, POWER_OFF, FORCE_DISABLE);
		break;
	case MMC_POWER_UP:
	case MMC_POWER_ON:
		hi_mci_ctrl_power(host, POWER_ON, FORCE_DISABLE);
		break;
	}
	himci_trace(3, "ios->clock = %d ", ios->clock);
	if (ios->clock) {
		hi_mci_control_cclk(host, DISABLE);
		hi_mci_set_cclk(host, ios->clock);
		hi_mci_control_cclk(host, ENABLE);

		/* speed mode check ,if it is DDR50 set DDR mode */
		if ((ios->timing == MMC_TIMING_UHS_DDR50)) {
			ctrl = himci_readl(host->base + MCI_UHS_REG);
			if (!(ENABLE_UHS_DDR_MODE & ctrl)) {
				ctrl |= ENABLE_UHS_DDR_MODE;
				himci_writel(ctrl, host->base + MCI_UHS_REG);
			}
			ctrl = himci_readl(host->base + MCI_UHS_REG);
		}
	} else {
		hi_mci_control_cclk(host, DISABLE);
		if ((ios->timing != MMC_TIMING_UHS_DDR50)) {
			ctrl = himci_readl(host->base + MCI_UHS_REG);
			if (ENABLE_UHS_DDR_MODE & ctrl) {
				ctrl &= ~ENABLE_UHS_DDR_MODE;
				himci_writel(ctrl, host->base + MCI_UHS_REG);
			}
			ctrl = himci_readl(host->base + MCI_UHS_REG);
		}
	}

	/* set bus_width */
	himci_trace(3, "ios->bus_width = %d ", ios->bus_width);

	/* clear bus width to 1bit first */
	tmp_reg = himci_readl(host->base + MCI_CTYPE);
	tmp_reg &= ~(CARD_WIDTH_0 | CARD_WIDTH_1);

	if (ios->bus_width == MMC_BUS_WIDTH_8) {
		tmp_reg |= CARD_WIDTH_0;
		himci_writel(tmp_reg, host->base + MCI_CTYPE);
	} else if (ios->bus_width == MMC_BUS_WIDTH_4) {
		tmp_reg |= CARD_WIDTH_1;
		himci_writel(tmp_reg, host->base + MCI_CTYPE);
	} else {
		himci_writel(tmp_reg, host->base + MCI_CTYPE);
	}
}

static int hi_mci_get_ro(struct mmc_host *mmc)
{
	unsigned ret;
	struct himci_host *host = mmc_priv(mmc);

	himci_trace(2, "begin");
	himci_assert(mmc);

	ret = hi_mci_ctrl_card_readonly(host);

	return ret;
}

static void hi_mci_hw_reset(struct mmc_host *mmc)
{
	struct himci_host *host = mmc_priv(mmc);

	himci_writel(0, host->base + MCI_RESET_N);
	/* For eMMC, minimum is 1us but give it 10us for good measure */
	udelay(10);
	himci_writel(1, host->base + MCI_RESET_N);
	/* For eMMC, minimum is 200us but give it 300us for good measure */
	usleep_range(300, 1000);
}

static void hi_mci_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct himci_host *host = mmc_priv(mmc);
	unsigned int reg_value;

	reg_value = readl(host->base + MCI_INTMASK);
	if (enable) {
		reg_value |= SDIO_INT_MASK;
	} else {
		reg_value &= ~SDIO_INT_MASK;
	}
	writel(reg_value, host->base + MCI_INTMASK);
}
static int hi_mci_get_card_detect(struct mmc_host *mmc)
{
	unsigned ret;
	struct himci_host *host = mmc_priv(mmc);

	himci_trace(2, "begin");
	ret = hi_mci_sys_card_detect(host);

	if(ret)
		return 0;
	else
		return 1;
}

static const struct mmc_host_ops hi_mci_ops = {
	.request = hi_mci_request,
	.set_ios = hi_mci_set_ios,
	.get_ro = hi_mci_get_ro,
	//.start_signal_voltage_switch = hi_mci_start_signal_voltage_switch,
	.enable_sdio_irq = hi_mci_enable_sdio_irq,
	.hw_reset = hi_mci_hw_reset,
	.get_cd = hi_mci_get_card_detect,
};

static irqreturn_t hisd_irq(int irq, void *dev_id)
{
	struct himci_host *host = dev_id;
	u32 state = 0;
	unsigned int tmp_reg;
	unsigned long flags;
	int need_wakeup = 0;

	spin_lock_irqsave(&host->lock, flags);
	state = himci_readl(host->base + MCI_MINTSTS);
	himci_trace(3, "irq state 0x%08X\n", state);

	if (state & DTO_INT_STATUS) {
		tmp_reg = himci_readl(host->base + MCI_INTMASK);
		tmp_reg &= ~DTO_INT_MASK;
		himci_writel(tmp_reg, host->base + MCI_INTMASK);
		need_wakeup = 1;
		host->pending_events |= HIMCI_PEND_DTO_m;
		himci_writel(DTO_INT_STATUS, host->base + MCI_RINTSTS);

		tmp_reg = himci_readl(host->base + MCI_INTMASK);
		tmp_reg |= DTO_INT_MASK;
		himci_writel(tmp_reg, host->base + MCI_INTMASK);
	}

	if (state & CD_INT_STATUS) {
		tmp_reg = himci_readl(host->base + MCI_INTMASK);
		tmp_reg &= ~CD_INT_MASK;
		himci_writel(tmp_reg, host->base + MCI_INTMASK);
		need_wakeup = 1;
		host->pending_events |= HIMCI_PEND_CD_m;

		himci_writel(CD_INT_STATUS, host->base + MCI_RINTSTS);

		tmp_reg = himci_readl(host->base + MCI_INTMASK);
		tmp_reg |= CD_INT_MASK;
		himci_writel(tmp_reg, host->base + MCI_INTMASK);
	}

	if (state & SDIO_INT_STATUS) {
		himci_writel(SDIO_INT_STATUS , host->base + MCI_RINTSTS);
		mmc_signal_sdio_irq(host->mmc);
	}

	spin_unlock_irqrestore(&host->lock, flags);

	if (need_wakeup)
		wake_up(&host->intr_wait);

	return IRQ_HANDLED;
}

static int __init hi_mci_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct himci_host *host = NULL;
	struct resource *host_ioaddr_res = NULL;
	struct resource *host_crg_res = NULL;
	int ret = 0, irq;
	int des_size = PAGE_SIZE / sizeof(struct himci_des);

	himci_trace(2, "begin");
	himci_assert(pdev);

	mmc = mmc_alloc_host(sizeof(struct himci_host), &pdev->dev);
	if (!mmc) {
		himci_error("no mem for hi mci host controller!\n");
		ret = -ENOMEM;
		goto out;
	}
	mmc->ops = &hi_mci_ops;
	mmc->f_min = MMC_CCLK_MIN;
	mmc->f_max = MMC_CCLK_MAX;
	mmc->caps |= /* MMC_CAP_ERASE | */
	    MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED;
	host_ioaddr_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == host_ioaddr_res) {
		himci_error("no ioaddr rescources config!\n");
		ret = -ENODEV;
		goto out;
	}
#if defined(CONFIG_ARCH_GODBOX)
#if defined(CONFIG_HIMCIV200_SDIO0_BUS_WIDTH_8)
	mmc->caps |= MMC_CAP_8_BIT_DATA;
#endif
#elif defined(CONFIG_ARCH_S40) || defined(CONFIG_ARCH_HI3798MX)
	if (CONFIG_HIMCIV200_SDIO1_IOBASE == host_ioaddr_res->start) {
		mmc->caps |= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA
		| MMC_CAP_SD_HIGHSPEED
		| MMC_CAP_MMC_HIGHSPEED
		| MMC_CAP_HW_RESET;

		if (get_chipid() == _HI3798MV100
			|| get_chipid() == _HI3796MV100)
			mmc->caps |= MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR;
	}
#endif

	/* reload by this controller */
	mmc->max_blk_size = 65536;
	mmc->max_blk_count = des_size;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	mmc->max_segs = des_size;
#else
	mmc->max_hw_segs = des_size;
	mmc->max_phys_segs = des_size;
#endif
	mmc->max_seg_size = 0x1000;
	mmc->max_req_size = mmc->max_seg_size * mmc->max_blk_count;	/* see IP manual */
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->ocr = mmc->ocr_avail;

	himci_trace(3, "Max Block Size: %u\n", mmc->max_blk_size);
	himci_trace(3, "Max Block Count: %u\n", mmc->max_blk_count);
	himci_trace(3, "Max Block Queue Segments: %u\n", mmc->max_segs);
	himci_trace(3, "Max Block Queue Segment Size: %u\n", mmc->max_seg_size);
	himci_trace(3, "Max Bytes in One Request: %u\n", mmc->max_req_size);
	himci_trace(3, "\n");

	host = mmc_priv(mmc);
	host->dma_vaddr = dma_alloc_coherent(&pdev->dev, PAGE_SIZE,
					     &host->dma_paddr, GFP_KERNEL);
	if (!host->dma_vaddr) {
		himci_error("no mem for himci dma!\n");
		ret = -ENOMEM;
		goto out;
	}

	host->mmc = mmc;

	host->base = ioremap_nocache(host_ioaddr_res->start, HI_MCI_IO_SIZE);
	if (!host->base) {
		himci_error("no mem for himci base!\n");
		ret = -ENOMEM;
		goto out;
	}

	host_crg_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (NULL == host_crg_res) {
		himci_error("%s%d:no crg rescources config!\n", pdev->name,
			    pdev->id);
		ret = -ENODEV;
		goto out;
	}

	spin_lock_init(&host->lock);

	/* enable mmc clk */
	hi_mci_sys_ctrl_init(host, host_crg_res->start);

	host->power_status = POWER_OFF;

	/* enable card */
	hi_mci_init_card(host);

	host->card_status = hi_mci_sys_card_detect(host);
	if (host->card_status) {
		printk(KERN_NOTICE "%s%d: eMMC/MMC/SD Device NOT detected!\n",
		       pdev->name, pdev->id);
	}
	init_timer(&host->timer);

	host->timer.function = hi_mci_detect_card;
	host->timer.data = (unsigned long)host;
	host->timer.expires = jiffies + detect_time;

	platform_set_drvdata(pdev, mmc);
	add_timer(&host->timer);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		printk(KERN_ERR "no IRQ defined!\n");
		goto out;
	}

	init_waitqueue_head(&host->intr_wait);

	host->irq = irq;
	ret = request_irq(irq, hisd_irq, 0, DRIVER_NAME, host);
	if (ret) {
		printk(KERN_ERR "request_irq error!\n");
		goto out;
	}

	/* only support eMMC */
	host->devid = pdev->id;
	if (pdev->id == 1) {
		himci_dbg_init(host->devid);
	}

	mmc_add_host(mmc);

	return 0;
out:
	if (host) {
		if (host->base)
			iounmap(host->base);
		if (host->dma_vaddr)
			dma_free_coherent(&pdev->dev, PAGE_SIZE,
					  host->dma_vaddr, host->dma_paddr);
	}
	if (mmc)
		mmc_free_host(mmc);
	return ret;
}

static int __exit hi_mci_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	himci_trace(2, "begin");
	himci_assert(pdev);

	platform_set_drvdata(pdev, NULL);

	if (mmc) {
		struct himci_host *host = mmc_priv(mmc);

		free_irq(host->irq, host);
		del_timer_sync(&host->timer);
		mmc_remove_host(mmc);
		hi_mci_ctrl_power(host, POWER_OFF, FORCE_DISABLE);
		hi_mci_control_cclk(host, DISABLE);
		iounmap(host->base);
		dma_free_coherent(&pdev->dev, PAGE_SIZE, host->dma_vaddr,
				  host->dma_paddr);
		mmc_free_host(mmc);
	}
	return 0;
}

static void hi_mci_shutdown(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	if (mmc) {
		unsigned int val;
		struct himci_host *host = mmc_priv(mmc);

#ifdef CONFIG_ARCH_HI3798MX
		/* If Hi3798mx && sdio1, not shutdown here, reset card befor wdg. */
		if (CONFIG_HIMCIV200_SDIO1_INTR == host->irq) {
			return;
		}
#endif

		val = himci_readl(host->base + MCI_CTRL);
		val |= CTRL_RESET | FIFO_RESET | DMA_RESET;
		himci_writel(val, host->base + MCI_CTRL);

		hi_mci_ctrl_power(host, POWER_OFF, FORCE_DISABLE);
	}
}

#ifdef CONFIG_PM
static int hi_mci_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct himci_host *host;
	int ret = 0;

	himci_trace(2, "begin");
	himci_assert(dev);

	if (mmc) {
		struct resource *host_crg_res = NULL;
		ret = mmc_suspend_host(mmc);

		host = mmc_priv(mmc);
		del_timer_sync(&host->timer);

		host_crg_res = platform_get_resource(dev, IORESOURCE_MEM, 1);
		if (NULL == host_crg_res) {
			himci_error("%s%d:no crg rescources config!\n",
				    dev->name, dev->id);
			ret = -ENODEV;
			return ret;
		}

		hi_mci_sys_ctrl_suspend(host, host_crg_res->start);
	}

	himci_trace(2, "end");

	return ret;
}

static int hi_mci_resume(struct platform_device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	struct himci_host *host;
	int ret = 0;

	himci_trace(2, "begin");
	himci_assert(dev);

	if (mmc) {
		struct resource *host_crg_res = NULL;
		host = mmc_priv(mmc);
		host_crg_res = platform_get_resource(dev, IORESOURCE_MEM, 1);
		if (NULL == host_crg_res) {
			himci_error("%s%d:no crg rescources config!\n",
				    dev->name, dev->id);
			ret = -ENODEV;
			return ret;
		}
		/* enable mmc clk */
		hi_mci_sys_ctrl_init(host, host_crg_res->start);
		/* enable card */
		hi_mci_init_card(host);

		ret = mmc_resume_host(mmc);
		add_timer(&host->timer);
	}

	himci_trace(2, "end");

	return ret;
}
#else
#define hi_mci_suspend	NULL
#define hi_mci_resume	NULL
#endif

static struct resource hi_mci_sdio0_resources[] = {
	[0] = {
		.start          = CONFIG_HIMCIV200_SDIO0_IOBASE,
		.end            = CONFIG_HIMCIV200_SDIO0_IOBASE + HI_MCI_IO_SIZE - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = SDIO_REG_BASE_CRG + PERI_CRG39,
		.end            = SDIO_REG_BASE_CRG + PERI_CRG39,
		.flags          = IORESOURCE_MEM,
	},
	[2] = {
		.start          = CONFIG_HIMCIV200_SDIO0_INTR,
		.end            = CONFIG_HIMCIV200_SDIO0_INTR,
		.flags          = IORESOURCE_IRQ,
	},
};

static struct resource hi_mci_sdio1_resources[] = {
	[0] = {
		.start          = CONFIG_HIMCIV200_SDIO1_IOBASE,
		.end            = CONFIG_HIMCIV200_SDIO1_IOBASE + HI_MCI_IO_SIZE - 1,
		.flags          = IORESOURCE_MEM,
	},
	[1] = {
		.start          = SDIO_REG_BASE_CRG + PERI_CRG40,
		.end            = SDIO_REG_BASE_CRG + PERI_CRG40,
		.flags          = IORESOURCE_MEM,
	},
	[2] = {
		.start          = CONFIG_HIMCIV200_SDIO1_INTR,
		.end            = CONFIG_HIMCIV200_SDIO1_INTR,
		.flags          = IORESOURCE_IRQ,
	},
};

static void hi_mci_platdev_release(struct device *dev)
{
}

static u64 himmc_dmamask = DMA_BIT_MASK(32);

static struct platform_device hi_mci_sdio0_device = {
	.name = "hi_mci",
	.id = 0,
	.dev = {
		.release = hi_mci_platdev_release,
		.dma_mask = &himmc_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		},
	.num_resources = ARRAY_SIZE(hi_mci_sdio0_resources),
	.resource = hi_mci_sdio0_resources,
};

static struct platform_device hi_mci_sdio1_device = {
	.name = "hi_mci",
	.id = 1,
	.dev = {
		.release = hi_mci_platdev_release,
		.dma_mask = &himmc_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		},
	.num_resources = ARRAY_SIZE(hi_mci_sdio1_resources),
	.resource = hi_mci_sdio1_resources,
};

static struct platform_driver hi_mci_driver = {
	.probe = hi_mci_probe,
	.remove = hi_mci_remove,
	.shutdown = hi_mci_shutdown,
	.suspend = hi_mci_suspend,
	.resume = hi_mci_resume,
	.driver = {
		   .name = DRIVER_NAME,
		   },
};

static int __init hi_mci_init(void)
{
	int ret;

	himci_trace(2, "begin");

	/*
	 * We should register SDIO1 first to make sure that
	 * the eMMC device,which connected to SDIO1 is mmcblk0.
	 */
	ret = platform_device_register(&hi_mci_sdio1_device);
	if (ret) {
		himci_error("sdio1 device register failed!");
		return ret;
	}

	ret = platform_device_register(&hi_mci_sdio0_device);
	if (ret) {
		platform_device_unregister(&hi_mci_sdio1_device);
		himci_error("sdio1 device register failed!");
		return ret;
	}

	ret = platform_driver_register(&hi_mci_driver);
	if (ret) {
		platform_device_unregister(&hi_mci_sdio1_device);
		platform_device_unregister(&hi_mci_sdio0_device);
		himci_error("Himci driver register failed!");
		return ret;
	}
	return ret;
}

static void __exit hi_mci_exit(void)
{
	himci_trace(2, "begin");

	platform_driver_unregister(&hi_mci_driver);
	platform_device_unregister(&hi_mci_sdio1_device);
	platform_device_unregister(&hi_mci_sdio0_device);
}

module_init(hi_mci_init);
module_exit(hi_mci_exit);

#ifdef MODULE
MODULE_AUTHOR("Hisilicon Drive Group");
MODULE_DESCRIPTION("MMC/SD driver for the Hisilicon MMC/SD Host Controller");
MODULE_LICENSE("GPL");
#endif
