# Build BT kernel drivers
ifneq ($(TARGET_BOARD_PLATFORM), niobe)
PRODUCT_PACKAGES += $(KERNEL_MODULES_OUT)/btpower.ko\
	$(KERNEL_MODULES_OUT)/bt_fm_slim.ko \
	$(KERNEL_MODULES_OUT)/radio-i2c-rtc6226-qca.ko
endif
