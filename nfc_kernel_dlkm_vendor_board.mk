# Build NFC kernel driver
ifeq ($(TARGET_BOARD_PLATFORM),kalama)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/stm_nfc_i2c.ko
endif
