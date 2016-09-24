#
# drivers/devices/hisfc350/Makefile
#

obj-$(CONFIG_MTD_HISFC350)	+= hisfc350.o hisfc350_os.o

ifdef CONFIG_ARCH_S40
obj-$(CONFIG_MTD_HISFC350)	+= hisfc350_s40.o hisfc350_s40_spi_ids.o
endif

ifdef CONFIG_ARCH_GODBOX
obj-$(CONFIG_MTD_HISFC350)	+= hisfc350_godbox.o hisfc350_godbox_spi_ids.o
endif

