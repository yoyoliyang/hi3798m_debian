#define SDIO_REG_BASE_CRG               IO_ADDRESS(0xF8A22000)
#define SD_LDO_BASE_CRG                 IO_ADDRESS(0xF8A2011c)

/* SDIO0 REG */
#define PERI_CRG39			0x9C
#define SDIO0_DRV_PS_SEL_MASK    	(0x7 << 16)
#define SDIO0_SAP_PS_SEL_MASK    	(0x7 << 12)
#define SDIO0_CLK_SEL_MASK		(0x3 << 8)

#define SDIO0_DRV_PS_SEL_0    		(0b000 << 16)
#define SDIO0_DRV_PS_SEL_45    		(0b001 << 16)
#define SDIO0_DRV_PS_SEL_90    		(0b010 << 16)
#define SDIO0_DRV_PS_SEL_135   		(0b011 << 16)
#define SDIO0_DRV_PS_SEL_180    	(0b100 << 16)
#define SDIO0_DRV_PS_SEL_225    	(0b101 << 16)
#define SDIO0_DRV_PS_SEL_270   		(0b110 << 16)
#define SDIO0_DRV_PS_SEL_315    	(0b111 << 16)

#define SDIO0_SAP_PS_SEL_0    		(0b000 << 12)
#define SDIO0_SAP_PS_SEL_45    		(0b001 << 12)
#define SDIO0_SAP_PS_SEL_90   		(0b010 << 12)
#define SDIO0_SAP_PS_SEL_135   		(0b011 << 12)
#define SDIO0_SAP_PS_SEL_180  		(0b100 << 12)
#define SDIO0_SAP_PS_SEL_225  		(0b101 << 12)
#define SDIO0_SAP_PS_SEL_270    	(0b110 << 12)
#define SDIO0_SAP_PS_SEL_315    	(0b111 << 12)

#define SDIO0_CLK_SEL_75M		(0b00 << 8)
#define SDIO0_CLK_SEL_100M		(0b01 << 8)
#define SDIO0_CLK_SEL_50M		(0b10 << 8)
#define SDIO0_CLK_SEL_24M		(0b11 << 8)

#define SDIO0_SRST_REQ			(0x1 << 4)
#define SDIO0_CKEN			(0x1 << 1)
#define SDIO0_BUS_CKEN			(0x1 << 0)

/* SDIO1 REG */
#define PERI_CRG40			0xA0
#define SDIO1_DRV_PS_SEL_MASK    	(0x7 << 16)
#define SDIO1_SAP_PS_SEL_MASK    	(0x7 << 12)
#define SDIO1_CLK_SEL_MASK		(0x3 << 8)

#define SDIO1_DRV_PS_SEL_0    		(0b000 << 16)
#define SDIO1_DRV_PS_SEL_45    		(0b001 << 16)
#define SDIO1_DRV_PS_SEL_90    		(0b010 << 16)
#define SDIO1_DRV_PS_SEL_135   		(0b011 << 16)
#define SDIO1_DRV_PS_SEL_180    	(0b100 << 16)
#define SDIO1_DRV_PS_SEL_225    	(0b101 << 16)
#define SDIO1_DRV_PS_SEL_270   		(0b110 << 16)
#define SDIO1_DRV_PS_SEL_315    	(0b111 << 16)

#define SDIO1_SAP_PS_SEL_0    		(0b000 << 12)
#define SDIO1_SAP_PS_SEL_45    		(0b001 << 12)
#define SDIO1_SAP_PS_SEL_90   		(0b010 << 12)
#define SDIO1_SAP_PS_SEL_135   		(0b011 << 12)
#define SDIO1_SAP_PS_SEL_180  		(0b100 << 12)
#define SDIO1_SAP_PS_SEL_225  		(0b101 << 12)
#define SDIO1_SAP_PS_SEL_270    	(0b110 << 12)
#define SDIO1_SAP_PS_SEL_315    	(0b111 << 12)

#define SDIO1_CLK_SEL_75M		(0b00 << 8)
#define SDIO1_CLK_SEL_100M		(0b01 << 8)
#define SDIO1_CLK_SEL_50M		(0b10 << 8)
#define SDIO1_CLK_SEL_24M		(0b11 << 8)

#define SDIO1_SRST_REQ			(0x1 << 4)
#define SDIO1_CKEN			(0x1 << 1)
#define SDIO1_BUS_CKEN			(0x1 << 0)

static void hi_mci_sys_ctrl_init(struct himci_host *host,
				 resource_size_t host_crg_addr)
{
	unsigned int tmp_reg;

	if ((SDIO_REG_BASE_CRG + PERI_CRG39) == (unsigned int)host_crg_addr) {
		/* enable SDIO clock */
		if ((_HI3719MV100 == get_chipid()) || (_HI3718MV100 == get_chipid())) {
			tmp_reg = himci_readl(host_crg_addr);

			tmp_reg &= ~SDIO0_CLK_SEL_MASK;
			tmp_reg &= ~SDIO0_DRV_PS_SEL_MASK;
			tmp_reg &= ~SDIO0_SAP_PS_SEL_MASK;
			tmp_reg |= SDIO0_CLK_SEL_100M | SDIO0_DRV_PS_SEL_135
				| SDIO0_SAP_PS_SEL_45;
			himci_writel(tmp_reg, host_crg_addr);
		} else if ((_HI3798MV100_A == get_chipid()) || (_HI3798MV100 == get_chipid())
			    || (_HI3796MV100 == get_chipid())) {
			tmp_reg = himci_readl(host_crg_addr);

			tmp_reg &= ~SDIO0_CLK_SEL_MASK;
			tmp_reg &= ~SDIO0_DRV_PS_SEL_MASK;
			tmp_reg &= ~SDIO0_SAP_PS_SEL_MASK;
			tmp_reg |= SDIO0_CLK_SEL_100M | SDIO0_DRV_PS_SEL_180
				| SDIO0_SAP_PS_SEL_90;
			himci_writel(tmp_reg, host_crg_addr);
		} else {
			tmp_reg = himci_readl(host_crg_addr);

			tmp_reg &= ~SDIO0_CLK_SEL_MASK;
			tmp_reg &= ~SDIO0_DRV_PS_SEL_MASK;
			tmp_reg &= ~SDIO0_SAP_PS_SEL_MASK;
			tmp_reg |= SDIO0_CLK_SEL_100M | SDIO0_DRV_PS_SEL_135
				| SDIO0_SAP_PS_SEL_90;
			himci_writel(tmp_reg, host_crg_addr);
		}

		/* SDIO soft reset */
		tmp_reg = himci_readl(host_crg_addr);
		tmp_reg |= SDIO0_SRST_REQ;
		himci_writel(tmp_reg, host_crg_addr);
		udelay(1000);
		tmp_reg &= ~SDIO0_SRST_REQ;
		tmp_reg |= SDIO0_CKEN | SDIO0_BUS_CKEN;
		himci_writel(tmp_reg, host_crg_addr);
		return;

	}

	if ((SDIO_REG_BASE_CRG + PERI_CRG40) == (unsigned int)host_crg_addr) {

		/* SDIO clock phase */
		if (_HI3716CV200ES == get_chipid()) {
			tmp_reg = himci_readl(host_crg_addr);
			tmp_reg &= ~(SDIO1_CLK_SEL_MASK | SDIO1_DRV_PS_SEL_MASK
					   | SDIO1_SAP_PS_SEL_MASK);
			tmp_reg |= SDIO1_CLK_SEL_50M | SDIO1_DRV_PS_SEL_45;
			himci_writel(tmp_reg, host_crg_addr);
		} else {
			tmp_reg = himci_readl(host_crg_addr);
			tmp_reg &= ~SDIO1_CLK_SEL_MASK;
			tmp_reg |= SDIO1_CLK_SEL_100M;
			himci_writel(tmp_reg, host_crg_addr);
		}

		/* SDIO soft reset */
		tmp_reg |= SDIO1_SRST_REQ;
		himci_writel(tmp_reg, host_crg_addr);
		udelay(1000);
		tmp_reg &= ~SDIO1_SRST_REQ;
		tmp_reg |= SDIO1_CKEN | SDIO1_BUS_CKEN;
		himci_writel(tmp_reg, host_crg_addr);
		return;
	}

	return;
}

static void hi_mci_sys_ctrl_suspend(struct himci_host *host,
				    resource_size_t host_crg_addr)
{
	unsigned int tmp_reg;

	if ((SDIO_REG_BASE_CRG + PERI_CRG39) == (unsigned int)host_crg_addr) {

		/* SDIO soft reset */
		tmp_reg = himci_readl(host_crg_addr);
		tmp_reg |= SDIO0_SRST_REQ;
		himci_writel(tmp_reg, host_crg_addr);
		udelay(1000);

		/* disable SDIO clock */
		tmp_reg &= ~(SDIO0_CKEN | SDIO0_BUS_CKEN);
		himci_writel(tmp_reg, host_crg_addr);
		return;

	}

	if ((SDIO_REG_BASE_CRG + PERI_CRG40) == (unsigned int)host_crg_addr) {

		/* SDIO soft reset */
		tmp_reg = himci_readl(host_crg_addr);
		tmp_reg |= SDIO1_SRST_REQ;
		himci_writel(tmp_reg, host_crg_addr);
		udelay(1000);
		tmp_reg &= ~(SDIO0_CKEN | SDIO0_BUS_CKEN);
		himci_writel(tmp_reg, host_crg_addr);
		return;
	}
}

static void himci_ldo_config(unsigned int flag)
{
	if (flag == 0) {
		/* 3.3v output */
		himci_writel(0x60, SD_LDO_BASE_CRG);
	} else {
		/* 1.8v output */
		himci_writel(0x20, SD_LDO_BASE_CRG);
	}

}
