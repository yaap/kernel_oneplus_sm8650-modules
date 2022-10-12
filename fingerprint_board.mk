ifeq ($(call is-board-platform-in-list,pineapple), true)
BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/qbt_handler.ko
endif
