ccflags-y += -Idrivers/mtd/nand

obj-$(CONFIG_MTD_NAND_HINFC504) += hinfc504.o hinfc504_os.o hinfc504_gen.o hinfc504_dbg_inf.o \
	hinfc504_read_retry_hynix_bg_cdie.o \
	hinfc504_read_retry_hynix_bg_bdie.o \
	hinfc504_read_retry_hynix_cg_adie.o \
	hinfc504_read_retry_micron.o  \
	hinfc504_read_retry_samsung.o \
	hinfc504_read_retry_toshiba.o

obj-$(CONFIG_HINFC504_DBG_NAND_DEBUG) += hinfc504_dbg.o hinfc504_dbg_ecc_dump.o
obj-$(CONFIG_HINFC504_DBG_NAND_DUMP) += hinfc504_dbg_dump.o
obj-$(CONFIG_HINFC504_DBG_NAND_ERASE_COUNT) += hinfc504_dbg_erase_count.o
obj-$(CONFIG_HINFC504_DBG_NAND_ECC_COUNT) += hinfc504_dbg_ecc_count.o
obj-$(CONFIG_HINFC504_DBG_NAND_READ_RETRY) += hinfc504_dbg_read_retry.o
