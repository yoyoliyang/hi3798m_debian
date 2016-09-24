
#define HI_REG_CRG_BASE                 __io_address(0xF8A22000)
#define HI_SATA3_CTRL                   (HI_REG_CRG_BASE + 0xA8)
#define HI_SATA3_PHY                    (HI_REG_CRG_BASE + 0xAC)

#define HI_SATA3_CKO_ALIVE_SRST         (1 << 9)
#define HI_SATA3_BUS_SRST               (1 << 8)
#define HI_SATA3_REFCLK_CKEN            (1<<4)
#define HI_SATA3_MPLL_DWORD_CKEN        (1<<3)
#define HI_SATA3_CKO_ALIVE_CKEN         (1<<2)
#define HI_SATA3_RX0_CKEN               (1<<1)
#define HI_SATA3_BUS_CKEN               (1<<0)

#define HI_SATA3_PHY_REFCLK_SEL         (1<<8)
#define HI_SATA3_PHY_REF_CKEN           (1<<0)

void hi_sata_init_s40(void __iomem *mmio)
{
#ifdef CONFIG_S40_FPGA
	unsigned int tmp_val;
	int i;

	tmp_val = readl(HI_SATA3_CTRL);
	tmp_val |= HI_SATA3_BUS_SRST;
	writel(tmp_val, HI_SATA3_CTRL);

	msleep(100);

	tmp_val = readl(HI_SATA3_CTRL);
	tmp_val &=  ~HI_SATA3_BUS_SRST ;
	writel(tmp_val, HI_SATA3_CTRL);

	msleep(100);

	tmp_val = readl(HI_SATA3_CTRL);
	tmp_val |= HI_SATA3_CKO_ALIVE_SRST;
	writel(tmp_val, HI_SATA3_CTRL);

	msleep(100);

	tmp_val = readl(HI_SATA3_CTRL);
	tmp_val &= ~HI_SATA3_CKO_ALIVE_SRST ;
	writel(tmp_val, HI_SATA3_CTRL);

	msleep(100);

	writel(0x0e03615f, (mmio + 0x100 + i*0x80 + HI_SATA_PORT_PHYCTL));

	msleep(100);
#else
	unsigned int tmp_val;

	/* Power on SATA disk */
	tmp_val = readl(__io_address(0xF8A20008));
	tmp_val |= 1<<10;
	writel(tmp_val, __io_address(0xF8A20008));
	msleep(10);

	/* Config SATA clock */
	writel(0x1f, __io_address(0xF8A220A8));
	msleep(10);
	writel(0x1, __io_address(0xF8A220AC));
	msleep(10);

	/* Config and reset the SATA PHY, SSC enabled */
	writel(0x49000679, __io_address(0xF99000A0));
	msleep(10);
	writel(0x49000678, __io_address(0xF99000A0));
	msleep(10);

	/* Config PHY controller register 1 */
	writel(0x345cb8, __io_address(0xF9900148));
	msleep(10);

	/* Config PHY controller register 2, and reset SerDes lane */
	writel(0x00060545, __io_address(0xF990014C));
	msleep(10);
	writel(0x00020545, __io_address(0xF990014C));
	msleep(10);

	/* Data invert between phy and sata controller*/
	writel(0x8, __io_address(0xF99000A4));
	msleep(10);

	/* Config Spin-up */
	writel(0x600000, __io_address(0xF9900118));
	msleep(10);
	writel(0x600002, __io_address(0xF9900118));
	msleep(10);

	/*
	 * Config SATA Port phy controller.
	 * To take effect for 0xF990014C, 
	 * we should force controller to 1.5G mode first
	 * and then force it to 6G mode.
	 */
	writel(0xE100000, __io_address(0xF9900174));
	msleep(10);
	writel(0xE5A0000, __io_address(0xF9900174));
	msleep(10);
	writel(0xE4A0000, __io_address(0xF9900174));
	msleep(10);

	writel(0xE250000, __io_address(0xF9900174));
	msleep(10);

#endif
}

void hi_sata_exit_s40(void)
{
#ifdef CONFIG_S40_FPGA
	unsigned int tmp_val;

	tmp_val = readl(HI_SATA3_CTRL);
	tmp_val |= HI_SATA3_BUS_SRST;
	writel(tmp_val, HI_SATA3_CTRL);

	msleep(100);

	tmp_val = readl(HI_SATA3_CTRL);
	tmp_val |= HI_SATA3_CKO_ALIVE_SRST;
	writel(tmp_val, HI_SATA3_CTRL);

	msleep(100);
#else
	unsigned int tmp_val;

	/* Config SATA clock */
	writel(0x300, __io_address(0xF8A220A8));
	writel(0x0, __io_address(0xF8A220AC));
	/* Power off SATA disk */
	tmp_val = readl(__io_address(0xF8A20008));
	tmp_val &= ~(1<<10);
	writel(tmp_val, __io_address(0xF8A20008));
#endif
}
