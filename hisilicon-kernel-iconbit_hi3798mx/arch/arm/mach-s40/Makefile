#
# Makefile for the linux kernel.
#


KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=softvfp+vfp)
LDFLAGS		+=--no-warn-mismatch

obj-y					:= core.o clock.o timer.o setup.o
obj-$(CONFIG_HW_PERF_EVENTS)		+= pmu.o
obj-$(CONFIG_CMA)			+= hi_cma.o
obj-$(CONFIG_PM)			+= hi_pm_sleep.o hipm.o
obj-$(CONFIG_SMP)			+= platsmp.o headsmp.o
obj-$(CONFIG_HOTPLUG_CPU)		+= hotplug.o
obj-$(CONFIG_SUPPORT_SRAM_MANAGER)	+= sram.o
obj-$(CONFIG_CACHE_L2X0)		+= l2cache.o
obj-$(CONFIG_PM_HIBERNATE)		+= snapshot.o
obj-y					+= cpu/

