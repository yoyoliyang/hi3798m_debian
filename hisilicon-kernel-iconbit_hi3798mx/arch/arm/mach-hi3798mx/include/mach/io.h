#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

#define __io(a)		__typesafe_io(a)
#define __mem_pci(a)	(a)

/*  phys_addr		virt_addr 
 * 0xF800_0000 <-----> 0xFD00_0000
 */
#define S40_IOCH1_VIRT	(0xFD000000)
#define S40_IOCH1_PHYS	(0xF8000000)
#define S40_IOCH1_SIZE	(0x02000000)

#define IO_ADDRESS(x)   (x + S40_IOCH1_VIRT - S40_IOCH1_PHYS)

#endif
