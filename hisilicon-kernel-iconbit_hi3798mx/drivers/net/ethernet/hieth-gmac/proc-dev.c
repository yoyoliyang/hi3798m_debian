#include "sockioctl.h"

#define GMAC_MAGIC	'g'
#define SET_MODE	_IOW(GMAC_MAGIC, 0, int)
#define GET_MODE	_IOR(GMAC_MAGIC, 1, int)
#define SET_FWD		_IOW(GMAC_MAGIC, 2, int)
#define GET_FWD		_IOR(GMAC_MAGIC, 3, int)
#define SET_PM		_IOW(GMAC_MAGIC, 4, struct pm_config)
#define SET_SUSPEND	_IOW(GMAC_MAGIC, 5, int)
#define SET_RESUME	_IOW(GMAC_MAGIC, 6, int)

static int set_work_mode(int mode, int master)
{
	struct higmac_adapter *adapter = get_adapter();
	struct net_device *netdev;
	unsigned long flags;
	int i;

	if (mode > MODE3 || master > CONFIG_GMAC_NUMS)
		return -EINVAL;

	for_each_gmac_netdev(netdev, i)
		if (netif_running(netdev))
			return -EBUSY;

	if ((mode == adapter->work_mode) && (master == adapter->master))
		return 0;

	spin_lock_irqsave(&adapter->lock, flags);

	adapter->work_mode = mode;
	adapter->master = master;
	fwd_setup(adapter);

	spin_unlock_irqrestore(&adapter->lock, flags);

	return 0;
}

static int set_forcing_fwd(int val)
{
	struct higmac_adapter *adapter = get_adapter();
	unsigned long flags;

	if (val == adapter->forcing_fwd)
		return 0;

	spin_lock_irqsave(&adapter->lock, flags);

	adapter->forcing_fwd = val;

	spin_unlock_irqrestore(&adapter->lock, flags);

	return 0;
}
/* debug code */
static int set_suspend(int eth_n)
{
	pm_message_t state = {.event = PM_EVENT_SUSPEND, };
	higmac_dev_driver.suspend(&higmac_platform_device, state);
	return 0;
}
/* debug code */
static int set_resume(int eth_n)
{
	higmac_dev_driver.resume(&higmac_platform_device);
	return 0;
}

static int hw_states_read(struct seq_file *m, void *v)
{
	struct higmac_netdev_local *ld;
	int  i;

#define sprintf_io(name, base)					\
	seq_printf(m, name, readl(io_base + base))

#define sprintf_pkts(name, rx_base, tx_base)			\
	seq_printf(m, "rx_" name "%u\t\t\ttx_" name "%u\n",	\
				readl(io_base + rx_base),	\
				readl(io_base + tx_base))

	for_each_gmac_netdev_local_priv(ld, i) {
		void __iomem *io_base = ld->gmac_iobase;
		if (!ld->phy)
			continue;
		if (!netif_running(ld->netdev))
			continue;

		seq_printf(m, "----------------gmac[%d]----------------\n", i);
		sprintf_pkts("ok_bytes:", 0x80, 0x100);
		sprintf_pkts("bad_bytes:", 0x84, 0x104);
		sprintf_pkts("uc_pkts:", 0x88, 0x108);
		sprintf_pkts("mc_pkts:", 0x8c, 0x10c);
		sprintf_pkts("bc_pkts:", 0x90, 0x110);
		sprintf_pkts("pkts_64B:", 0x94, 0x114);
		sprintf_pkts("pkts_65_127B:", 0x98, 0x118);
		sprintf_pkts("pkts_128_255B:", 0x9c, 0x11c);
		sprintf_pkts("pkts_256_511B:", 0xa0, 0x120);
		sprintf_pkts("pkts_512_1023B:", 0xa4, 0x124);
		sprintf_pkts("pkts_1024_1518B:", 0xa8, 0x128);
		sprintf_pkts("pkts_1519_MAX:", 0xac, 0x12c);
		sprintf_io("rx_fcs_errors:%u\n", 0xb0);
		/* add more here */

	}
#undef sprintf_io
#undef sprintf_pkts

	return 0;
}

static int hw_fwd_mac_tbl_read(struct seq_file *m, void *v)
{
	void __iomem *io_base = get_adapter()->fwdctl_iobase;
	struct fwd_mac_tbl tbl;
	int  i;

	if (!io_base)
		return 0;

	seq_puts(m, "hw_index owner\tto-cpu ""to-other\t\tmac-addr\n");
	for (i = 0; i < FWD_TBL_ENTRY_NUMS; i++) {
		tbl.mac_tbl_l.val = readl(io_base + FW_MAC_TBL_L + i * 8);
		tbl.mac_tbl_h.val = readl(io_base + FW_MAC_TBL_H + i * 8);
		if (!tbl.mac_tbl_h.bits.valid) /* tbl entry valid */
			continue;

		seq_printf(m, "%5d\t  %s\t%5d %5d\t\t"
				"%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx\n",
				i,
				tbl.mac_tbl_h.bits.owner ? "eth1" : "eth0",
				tbl.mac_tbl_h.bits.to_cpu,
				tbl.mac_tbl_h.bits.to_other,
				tbl.mac_tbl_h.bits.mac5,
				tbl.mac_tbl_h.bits.mac4,
				tbl.mac_tbl_l.mac[3],
				tbl.mac_tbl_l.mac[2],
				tbl.mac_tbl_l.mac[1],
				tbl.mac_tbl_l.mac[0]);
	}

	return 0;
}

static const char * const mode_text[] = {
	[STANDALONE] = "standalone mode",
	[MODE1] = "mode1",
	[MODE2] = "mode2",
	[MODE3] = "mode3",
};

static int work_mode_proc_read(struct seq_file *m, void *v)
{
	struct higmac_adapter *adapter = get_adapter();

	seq_printf(m, "%s(%d)",	mode_text[adapter->work_mode],
			adapter->work_mode);

	if (adapter->work_mode == MODE1 || adapter->work_mode == MODE2)
		seq_printf(m, ", master(%s)",
			       adapter->master ? "eth1" : "eth0");

	seq_puts(m, "\n");

	return 0;
}

static ssize_t work_mode_proc_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	int mode, master, ret;
	char line[8] = {0}, *token, *first_token;

	if (count > 8)
		count = 8;
	ret = copy_from_user(line, buf, count);
	if (ret)
		return -EFAULT;

	token = &line[0];
	first_token = strsep(&token, ",");
	ret = kstrtoul(first_token, 0, (unsigned long *)&mode);
	if (ret)
		return ret;

	if (token) {
		while (*token == ' ')
			token++;
		ret = kstrtoul(token, 0, (unsigned long *)&master);
		if (ret)
			return ret;
	} else
		master = 0;

	ret = set_work_mode(mode, master);

	return (ssize_t)ret ? : count;
}

static int fwd_proc_read(struct seq_file *m, void *v)
{
	struct higmac_adapter *adapter = get_adapter();
	int  fwd = adapter->forcing_fwd;

	seq_printf(m, "%d", fwd);
	switch (fwd) {
	case 1:
		seq_puts(m, " : eth0 -> eth1");
		break;
	case 2:
		seq_puts(m, " : eth0 <- eth1");
		break;
	case 3:
		seq_puts(m, " : eth0 <-> eth1");
		break;
	default:
		break;
	}

	seq_puts(m, "\n");

	return 0;
}

static ssize_t fwd_proc_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *pos)
{
	int val, ret;
	char line[8];

	if (count > 8)
		count = 8;
	ret = copy_from_user(line, buf, count);
	if (ret)
		return -EFAULT;
	ret = kstrtoul(&line[0], 0, (unsigned long *)&val);
	if (ret)
		return ret;
	ret = set_forcing_fwd(val);

	return (ssize_t)ret ? : count;
}

static int debug_level_proc_read(struct seq_file *m, void *v)
{
	struct higmac_adapter *adapter = get_adapter();
	struct higmac_netdev_local *ld;
	int i, j;

	seq_printf(m, "debug_level:0x%x\n", adapter->debug_level);

	for_each_gmac_netdev_local_priv(ld, i) {
		if (!ld->phy)
			continue;
		if (!netif_running(ld->netdev))
			continue;
		seq_printf(m, "\neth%d: '0'=empty '1'=filled\n", ld->index);
		for (j = 0; j < ld->rx_fq.count; j++) {
			seq_printf(m, "%s", ld->rx_fq.skb[j] ? "1" : "0");
			if ((j + 1) % 100 == 0)
				seq_puts(m, "\n");
		}
		seq_puts(m, "\n");
	}

	return 0;
}

static ssize_t debug_level_proc_write(struct file *file, const char __user *buf,
				  size_t count, loff_t *pos)
{
	struct higmac_adapter *adapter = get_adapter();
	char line[8];
	int ret;

	if (count > 8)
		count = 8;
	ret = copy_from_user(line, buf, count);
	if (ret)
		return -EFAULT;

	ret = kstrtoul(&line[0], 0, (unsigned long *)&(adapter->debug_level));
	if (ret)
		return -EFAULT;

	return (ssize_t)count;
}

static struct proc_dir_entry *higmac_proc_root;

#define proc_open(name)	\
static int proc_open_##name(struct inode *inode, struct file *file) \
{ \
	return single_open(file, name, PDE_DATA(inode)); \
} \

proc_open(hw_states_read);
proc_open(hw_fwd_mac_tbl_read);
proc_open(work_mode_proc_read);
proc_open(fwd_proc_read);
proc_open(debug_level_proc_read);

static struct proc_file {
	char *name;
	const struct file_operations ops;

} proc_file[] = {
	{
		.name = "hw_stats",
		.ops = {
			.open           = proc_open_hw_states_read,
			.read           = seq_read,
			.llseek         = seq_lseek,
			.release        = single_release,
		},
	}, {
		.name = "hw_fwd_mac_tbl",
		.ops = {
			.open           = proc_open_hw_fwd_mac_tbl_read,
			.read           = seq_read,
			.llseek         = seq_lseek,
			.release        = single_release,
		},
	}, {
		.name = "work_mode",
		.ops = {
			.open           = proc_open_work_mode_proc_read,
			.read           = seq_read,
			.llseek         = seq_lseek,
			.release        = single_release,
			.write		= work_mode_proc_write,
		},
	}, {
		.name = "force_forwarding",
		.ops = {
			.open           = proc_open_fwd_proc_read,
			.read           = seq_read,
			.llseek         = seq_lseek,
			.release        = single_release,
			.write		= fwd_proc_write,
		},
	}, {
		.name = "debug_info",
		.ops = {
			.open           = proc_open_debug_level_proc_read,
			.read           = seq_read,
			.llseek         = seq_lseek,
			.release        = single_release,
			.write		= debug_level_proc_write,
		}
	}
};

/* /proc/higmac/
 *	|---hw_stats
 *	|---hw_fwd_mac_tbl
 *	|---work_mode
 *	|---force_forwarding
 *	|---debug_level
 *	|---skb_pools
 */
void higmac_proc_create(void)
{
	struct proc_dir_entry *entry;
	int i;

	higmac_proc_root = proc_mkdir("higmac", NULL);
	if (!higmac_proc_root)
		return;

	for (i = 0; i < ARRAY_SIZE(proc_file); i++) {
		entry = proc_create(proc_file[i].name, 0,
					  higmac_proc_root, &proc_file[i].ops);
	}
}

void higmac_proc_destroy(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(proc_file); i++)
		remove_proc_entry(proc_file[i].name, higmac_proc_root);

	remove_proc_entry("higmac", NULL);
}

static long gmacdev_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct pm_config pm_config;
	int val;

	switch (cmd) {
	case SET_MODE:
		if (copy_from_user(&val, argp, sizeof(val)))
			return -EFAULT;
		return set_work_mode(val >> 16, val & 0xFFFF);
	case GET_MODE:
		val = get_adapter()->work_mode;
		val <<= 16;
		val |= get_adapter()->master;
		if (copy_to_user(argp, &val, sizeof(val)))
			return -EFAULT;
		break;
	case SET_FWD:
		if (copy_from_user(&val, argp, sizeof(val)))
			return -EFAULT;
		return set_forcing_fwd(val);
	case GET_FWD:
		val = get_adapter()->forcing_fwd;
		if (copy_to_user(argp, &val, sizeof(val)))
			return -EFAULT;
		break;
	case SET_PM:
		if (copy_from_user(&pm_config, argp, sizeof(pm_config)))
			return -EFAULT;
		return pmt_config(&pm_config);
	case SET_SUSPEND:
		if (copy_from_user(&val, argp, sizeof(val)))
			return -EFAULT;
		return set_suspend(val);
	case SET_RESUME:
		if (copy_from_user(&val, argp, sizeof(val)))
			return -EFAULT;
		return set_resume(val);
	}

	return 0;
}

int higmac_ioctl(struct net_device *net_dev, struct ifreq *rq, int cmd)
{
	struct higmac_netdev_local *ld = netdev_priv(net_dev);
	struct pm_config pm_config;
	int val = 0;

	switch (cmd) {
	case SIOCGETMODE:
		val = get_adapter()->work_mode;
		val <<= 16;
		val |= get_adapter()->master;
		if (copy_to_user(rq->ifr_data, &val, sizeof(val)))
			return -EFAULT;
		break;

	case SIOCSETMODE:
		if (copy_from_user(&val, rq->ifr_data, sizeof(val)))
			return -EFAULT;
		return set_work_mode(val >> 16, val & 0xFFFF);

	case SIOCGETFWD:
		val = get_adapter()->forcing_fwd;
		if (copy_to_user(rq->ifr_data, &val, sizeof(val)))
			return -EFAULT;
		break;

	case SIOCSETFWD:
		if (copy_from_user(&val, rq->ifr_data, sizeof(val)))
			return -EFAULT;
		return set_forcing_fwd(val);

	case SIOCSETPM:
		if (copy_from_user(&pm_config, rq->ifr_data, sizeof(pm_config)))
			return -EFAULT;
		return pmt_config(&pm_config);

	case SIOCSETSUSPEND:
		if (copy_from_user(&val, rq->ifr_data, sizeof(val)))
			return -EFAULT;
		return set_suspend(val);

	case SIOCSETRESUME:
		if (copy_from_user(&val, rq->ifr_data, sizeof(val)))
			return -EFAULT;
		return set_resume(val);

	default:
		if (!netif_running(net_dev))
			return -EINVAL;

		if (!ld->phy)
			return -EINVAL;

		return phy_mii_ioctl(ld->phy, rq, cmd);
	}
	return 0;
}

static int gmacdev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int gmacdev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations gmac_fops = {
	.open = gmacdev_open,
	.release = gmacdev_release,
	.compat_ioctl = gmacdev_ioctl,
	.unlocked_ioctl = gmacdev_ioctl,
};

static struct miscdevice gmac_dev = {
	MISC_DYNAMIC_MINOR,
	"gmac",
	&gmac_fops
};
