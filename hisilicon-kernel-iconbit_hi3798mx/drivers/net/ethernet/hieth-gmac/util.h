#ifndef __HIGMAC_UTIL_H__
#define __HIGMAC_UTIL_H__

#define HIGMAC_TRACE_LEVEL 10

#define higmac_trace(level, msg...) do { \
        if ((level) >= HIGMAC_TRACE_LEVEL) { \
            printk(KERN_INFO "higmac_trace:%s:%d: ", __FILE__, __LINE__); \
            printk(msg); \
            printk("\n"); \
        } \
    } while (0)

#define higmac_error(args...) do { \
        printk(KERN_ERR "higmac:%s:%d: ", __FILE__, __LINE__); \
        printk(args); \
        printk("\n"); \
    } while (0)

#define higmac_assert(cond) do { \
        if (!(cond)) \
            printk("Assert:higmac:%s:%d\n", \
                   __FILE__, \
                   __LINE__);\
    } while (0)

#define higmac_readl(reg)	readl(ld->gmac_iobase + reg)
#define higmac_writel(v, reg)	writel(v, ld->gmac_iobase + reg)

#define higmac_writel_bits(ld, v, ofs, bits_desc) do { \
			unsigned long _bits_desc = bits_desc; \
			unsigned long _shift = (_bits_desc) >> 16; \
			unsigned long _reg  = higmac_readl(ofs); \
			unsigned long _mask = ((_bits_desc & 0x3F) < 32) ?\
			(((1 << (_bits_desc & 0x3F)) - 1) << (_shift)) : 0xffffffff; \
			higmac_writel((_reg & (~_mask)) | (((v) << (_shift)) & _mask), ofs); \
    } while (0)

#define higmac_readl_bits(ld, ofs, bits_desc) ({ \
			unsigned long _bits_desc = bits_desc; \
			unsigned long _shift = (_bits_desc) >> 16; \
			unsigned long _mask = ((_bits_desc & 0x3F) < 32) ? \
			(((1 << (_bits_desc & 0x3F)) - 1) << (_shift)) : 0xffffffff; \
			(higmac_readl(ofs) & _mask) >> (_shift); })

#define MK_BITS(shift, nbits) ((((shift) & 0x1F) << 16) | ((nbits) & 0x3F))

#endif
