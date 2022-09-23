# SPDX-License-Identifier: GPL-2.0-only

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
# For incremental compilation
LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/*)
LOCAL_MODULE      := ubwcpx.ko
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
