MOD_NAME := hi_runtime

obj-y += $(MOD_NAME).o


$(MOD_NAME)-y := runtime_module.o	\
		 		 sha1.o	

#===============================================================================
#   rules
#===============================================================================
.PHONY: all clean

all:
	$(AT)make -C $(LINUX_DIR) ARCH=arm CROSS_COMPILE=$(CFG_HI_TOOLCHAINS_NAME)- M=$(CURDIR) modules

clean:
	$(AT)make -C $(LINUX_DIR) M=$(CURDIR) clean
	$(AT)rm -rf  *.o
	$(AT)rm -rf  .*.o.cmd
	$(AT)rm -rf .tmp_versions
	$(AT)rm -rf  Module.symvers
	$(AT)rm -rf  modules.order
