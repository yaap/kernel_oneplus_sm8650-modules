# Build audio kernel driver
ifneq ($(TARGET_BOARD_AUTO),true)
ifeq ($(TARGET_USES_QMAA),true)
  ifeq ($(TARGET_USES_QMAA_OVERRIDE_BLUETOOTH), true)
     ifeq ($(call is-board-platform-in-list,$(TARGET_BOARD_PLATFORM)),true)
        BT_KERNEL_DRIVER := $(KERNEL_MODULES_OUT)/btpower.ko\
                            $(KERNEL_MODULES_OUT)/bt_fm_slim.ko
        ifeq (,$(filter niobe anorak61 neo61 pitti taro blair kalama, $(TARGET_BOARD_PLATFORM)))
           BT_KERNEL_DRIVER += $(KERNEL_MODULES_OUT)/btfmcodec.ko \
                               $(KERNEL_MODULES_OUT)/bt_fm_swr.ko
        endif
        ifeq (,$(filter niobe anorak61 neo61 pitti seraph blair, $(TARGET_BOARD_PLATFORM)))
           BT_KERNEL_DRIVER += $(KERNEL_MODULES_OUT)/radio-i2c-rtc6226-qca.ko
        endif
        BOARD_VENDOR_KERNEL_MODULES += $(BT_KERNEL_DRIVER)
     endif
  endif
else
  ifeq ($(call is-board-platform-in-list,$(TARGET_BOARD_PLATFORM)),true)
     BT_KERNEL_DRIVER := $(KERNEL_MODULES_OUT)/btpower.ko\
                         $(KERNEL_MODULES_OUT)/bt_fm_slim.ko
     ifeq (,$(filter niobe anorak61 neo61 pitti taro blair kalama, $(TARGET_BOARD_PLATFORM)))
        BT_KERNEL_DRIVER += $(KERNEL_MODULES_OUT)/btfmcodec.ko \
                            $(KERNEL_MODULES_OUT)/bt_fm_swr.ko
     endif
     ifeq (,$(filter niobe anorak61 neo61 pitti seraph blair, $(TARGET_BOARD_PLATFORM)))
        BT_KERNEL_DRIVER += $(KERNEL_MODULES_OUT)/radio-i2c-rtc6226-qca.ko
     endif
     BOARD_VENDOR_KERNEL_MODULES += $(BT_KERNEL_DRIVER)
  endif
endif
endif
