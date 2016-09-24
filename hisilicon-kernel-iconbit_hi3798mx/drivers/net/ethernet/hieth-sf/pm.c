#include <linux/crc16.h>

#define N			(31)
#define FILTERS			(4)
struct pm_config {
	unsigned char index;		/* bit0--eth0 bit1--eth1 */
	unsigned char uc_pkts_enable;
	unsigned char magic_pkts_enable;
	unsigned char wakeup_pkts_enable;
	struct {
		unsigned int	mask_bytes : N;
		unsigned int	reserved   : 1;/* userspace ignore this bit */
		unsigned char	offset;	/* >= 12 */
		unsigned char	value[N];/* byte string */
		unsigned char	valid;	/* valid filter */
	} filter[FILTERS];
};

#define U_PMT_CTRL		0x0500
#define D_PMT_CTRL		0x2500

#define U_PMT_MASK0		0x0504
#define D_PMT_MASK0		0x2504

#define U_PMT_MASK1		0x0508
#define D_PMT_MASK1		0x2508

#define U_PMT_MASK2		0x050c
#define D_PMT_MASK2		0x250c

#define U_PMT_MASK3		0x0510
#define D_PMT_MASK3		0x2510

#define U_PMT_CMD		0x0514
#define D_PMT_CMD		0x2514

#define U_PMT_OFFSET		0x0518
#define D_PMT_OFFSET		0x2518

#define U_PMT_CRC1_0		0x051c
#define D_PMT_CRC1_0		0x251c

#define U_PMT_CRC3_2		0x0520
#define D_PMT_CRC3_2		0x2520

#define MASK_INVALID_BIT	(1 << 31)

void initCrcTable(void);
unsigned short computeCrc(char* message, int nBytes);
static unsigned short calculate_crc16(char *buf, unsigned int mask)
{
	char data[N];
	int i, len = 0;

	memset(data, 0, sizeof(data));

	for (i = 0; i < N; i++) {
		if (mask & 0x1)
			data[len++] = buf[i];

		mask >>= 1;
	}

	return computeCrc(data, len);
}


#define	PM_SET			(1)
#define PM_CLEAR		(0)
#define CFG_ETH_NUMS		2
static char pm_state[CFG_ETH_NUMS] = {PM_CLEAR, PM_CLEAR};

int pmt_config_eth(struct pm_config *config, struct hieth_netdev_local *ld)
{
	unsigned int v = 0, cmd = 0, offset = 0;
	unsigned short crc[FILTERS] = {0};
	int reg_mask = 0;
	int i;

	if (!ld)
		return -EINVAL;

	local_lock(ld);
	if (config->wakeup_pkts_enable) {
		/* disable wakeup_pkts_enable before reconfig? */
		v = readl(ld->iobase + UD_REG_NAME(PMT_CTRL));
		v &= ~(1 << 2);
		writel(v, ld->iobase + UD_REG_NAME(PMT_CTRL));/* any side effect? */
	} else
		goto config_ctrl;

/*
 * filter.valid		mask.valid	mask_bytes	effect
 *	0		*		*		no use the filter
 *	1		0		*		all pkts can wake-up(non-exist)
 *	1		1		0		all pkts can wake-up
 *	1		1		!0		normal filter
 */
	/* setup filter */
	for (i = 0; i < FILTERS; i++) {
		if (config->filter[i].valid) {
			if (config->filter[i].offset < 12)
				continue;
			/* offset and valid bit */
			offset |= config->filter[i].offset << (i * 8);
			cmd    |= 1 << (i * 8); /* valid bit */
			/* mask */
			reg_mask = UD_REG_NAME(PMT_MASK0) + (i * 4);

			/*
			 * for logic, mask valid bit(bit31) must set to 0,
			 * 0 is enable
			 */
			v = config->filter[i].mask_bytes;
			v &= ~(1 << 31);
			writel(v, ld->iobase + reg_mask);

			/* crc */
			crc[i] = calculate_crc16(config->filter[i].value, v);
			if (i <= 1) {/* for filter0 and filter 1 */
				v = readl(ld->iobase + UD_REG_NAME(PMT_CRC1_0));
				v &= ~(0xFFFF << (16 * i));
				v |= crc[i] << (16 * i);
				writel(v, ld->iobase + UD_REG_NAME(PMT_CRC1_0));
			} else {/* filter2 and filter3 */
				v = readl(ld->iobase + UD_REG_NAME(PMT_CRC3_2));
				v &= ~(0xFFFF << (16 * (i - 2)));
				v |= crc[i] << (16 * (i - 2));
				writel(v, ld->iobase + UD_REG_NAME(PMT_CRC3_2));
			}
		}
	}

	if (cmd) {
		writel(offset, ld->iobase + UD_REG_NAME(PMT_OFFSET));
		writel(cmd, ld->iobase + UD_REG_NAME(PMT_CMD));
	}

config_ctrl:
	v = 0;
	if (config->uc_pkts_enable)
		v |= 1 << 9;	/* uc pkts wakeup */
	if (config->wakeup_pkts_enable)
		v |= 1 << 2;	/* use filter framework */
	if (config->magic_pkts_enable)
		v |= 1 << 1;	/* magic pkts wakeup */

	v |= 3 << 5;		/* clear irq status */
	writel(v, ld->iobase + UD_REG_NAME(PMT_CTRL));

	local_unlock(ld);

	return 0;
}

/* pmt_config will overwrite pre-config */
int pmt_config(struct pm_config *config)
{
	static int init;
	int map = config->index, i, ret = -EINVAL;
	struct hieth_netdev_local *ld;

	if (!init)
		initCrcTable();

	for (i = 0; i < CFG_ETH_NUMS; i++) {
		if (!hieth_devs_save[i])
			continue;
		
		ld = netdev_priv(hieth_devs_save[i]);

		if (map & 0x1) {
			ret = pmt_config_eth(config, ld);
			if (ret)
				return ret;
			else {
				pm_state[i] = PM_SET;
				device_set_wakeup_enable(ld->dev, 1);
			}
		}
		map >>= 1;
	}

	return ret;
}

bool inline pmt_enter(void)
{
	int i, v, pm = false;
	struct hieth_netdev_local *ld;
	
	for (i = 0; i < CFG_ETH_NUMS; i++) {
		if (!hieth_devs_save[i])
			continue;
		
		ld = netdev_priv(hieth_devs_save[i]);
		
		local_lock(ld);
		if (pm_state[i] == PM_SET) {

			v = readl(ld->iobase + UD_REG_NAME(PMT_CTRL));
			v |= 1 << 0;	/* enter power down */
			v |= 1 << 3;	/* enable wakeup irq */
			v |= 3 << 5;	/* clear irq status */
			writel(v, ld->iobase + UD_REG_NAME(PMT_CTRL));

			pm_state[i] = PM_CLEAR;
			pm = true;
		}
		local_unlock(ld);
	}
	return pm;
}

void inline pmt_exit(void)
{
	int i, v;
	struct hieth_netdev_local *ld;
	
	for (i = 0; i < CFG_ETH_NUMS; i++) {
		if (!hieth_devs_save[i])
			continue;
		
		ld = netdev_priv(hieth_devs_save[i]);
		
		/* logic auto exit power down mode */
		local_lock(ld);

		v = readl(ld->iobase + UD_REG_NAME(PMT_CTRL));
		v &= ~(1 << 0);	/* enter power down */
		v &= ~(1 << 3);	/* enable wakeup irq */

		v |= 3 << 5;	/* clear irq status */
		writel(v, ld->iobase + UD_REG_NAME(PMT_CTRL));

		local_unlock(ld);
	}
	
	//device_set_wakeup_enable(ld->dev, 0);
}

/* ========the following code copy from Synopsys DWC_gmac_crc_example.c====== */
#define CRC16			/* Change it to CRC16 for CRC16 Computation*/

#define FALSE	0
#define TRUE	!FALSE

#if defined(CRC16)
typedef unsigned short  crc;
#define CRC_NAME		"CRC-16"
#define POLYNOMIAL		0x8005
#define INITIAL_REMAINDER	0xFFFF
#define FINAL_XOR_VALUE		0x0000
#define REVERSE_DATA		TRUE
#define REVERSE_REMAINDER	FALSE
#endif

#define WIDTH    (8 * sizeof(crc))
#define TOPBIT   (1 << (WIDTH - 1))

#if (REVERSE_DATA == TRUE)
#undef  REVERSE_DATA
#define REVERSE_DATA(X)		((unsigned char) reverse((X), 8))
#else
#undef  REVERSE_DATA
#define REVERSE_DATA(X)		(X)
#endif

#if (REVERSE_REMAINDER == TRUE)
#undef  REVERSE_REMAINDER
#define REVERSE_REMAINDER(X)	((crc) reverse((X), WIDTH))
#else
#undef  REVERSE_REMAINDER
#define REVERSE_REMAINDER(X)	(X)
#endif

static crc crcTable[256];

/*
 * Reverse the data
 *
 * Input1: Data to be reversed
 * Input2: number of bits in the data
 * Output: The reversed data
 */
unsigned long reverse(unsigned long data, unsigned char nBits)
{
	unsigned long  reversed = 0x00000000;
	unsigned char  bit;

	/*
	 * Reverse the data about the center bit.
	 */
	for (bit = 0; bit < nBits; ++bit) {
		/*
		 * If the LSB bit is set, set the reflection of it.
		 */
		if (data & 0x01)
			reversed |= (1 << ((nBits - 1) - bit));

		data = (data >> 1);
	}
	return (reversed);
}

/*
 * This Initializes the partial CRC look up table
 */
void initCrcTable(void)
{
	crc remainder;
	int dividend;
	unsigned char  bit;

	/*
	 * Compute the remainder of each possible dividend.
	 */
	for (dividend = 0; dividend < 256; ++dividend) {
		/*
		 * Start with the dividend followed by zeros.
		 */
		remainder = (crc)(dividend << (WIDTH - 8));

		/*
		 * Perform modulo-2 division, a bit at a time.
		 */
		for (bit = 8; bit > 0; --bit) {
			/*
			 * Try to divide the current data bit.
			 */
			if (remainder & TOPBIT)
				remainder = (remainder << 1) ^ POLYNOMIAL;
			else
				remainder = (remainder << 1);
		}

		/*
		 * Store the result into the table.
		 */
		crcTable[dividend] = remainder;
	}
}

unsigned short computeCrc(char* message, int nBytes)
{
	crc	remainder = INITIAL_REMAINDER;
	int	byte;
	unsigned char  data;

	/*
	 * Divide the message by the polynomial, a byte at a time.
	 */
	for (byte = 0; byte < nBytes; ++byte) {
		data = REVERSE_DATA(message[byte]) ^ (remainder >> (WIDTH - 8));
		remainder = crcTable[data] ^ (remainder << 8);
	}

	/*
	 * The final remainder is the CRC.
	 */
	return (REVERSE_REMAINDER(remainder) ^ FINAL_XOR_VALUE);
}
