# Build NFC kernel driver
ifeq ($(call is-board-platform-in-list, pineapple blair),true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/nxp-nci.ko
endif

ifeq ($(call is-board-platform-in-list, blair),true)
TARGET_ENABLE_PERIPHERAL_CONTROL := false
endif
