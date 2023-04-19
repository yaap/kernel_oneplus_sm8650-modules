# SPDX-License-Identifier: GPL-2.0-only

LOCAL_PATH := $(call my-dir)
LOCAL_MODULE_DDK_BUILD := true

include $(CLEAR_VARS)
# For incremental compilation
LOCAL_SRC_FILES   := $(wildcard $(LOCAL_PATH)/*)
LOCAL_EXPORT_KO_INCLUDE_DIRS    := $(LOCAL_PATH)/include/uapi
LOCAL_EXPORT_KO_INCLUDE_DIRS    += $(LOCAL_PATH)/include/kernel
LOCAL_MODULE      := ubwcp.ko
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
include $(DLKM_DIR)/Build_external_kernelmodule.mk
