#
# Makefile for the touchscreen drivers.
#
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
	include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6763/

# Each configuration option enables a list of files.
oplus_bsp_tp_nt36532_noflash-y += nvt_drivers_nt36532_noflash.o
obj-m += oplus_bsp_tp_nt36532_noflash.o
#endif
