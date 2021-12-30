ifneq ($(TARGET_BOARD_PLATFORM),qssi)
ST54SPI_GPIO_PLATFORMS_LIST := lahaina
ST54SPI_GPIO_PLATFORMS_LIST := taro

ifeq ($(call is-board-platform-in-list, $(ST54SPI_GPIO_PLATFORMS_LIST)),true)
#Make file to create ST54SPI_GPIO_PLATFORMS_LIST
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := -Wno-macro-redefined -Wno-unused-function -Wall -Werror
LOCAL_CLANG :=true

LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
LOCAL_MODULE := stm_st54se_gpio.ko
LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/**/*) $(wildcard $(LOCAL_PATH)/*)

DLKM_DIR := $(TOP)/device/qcom/common/dlkm



include $(DLKM_DIR)/Build_external_kernelmodule.mk

endif #End of Check for target
endif #End of Check for qssi target
