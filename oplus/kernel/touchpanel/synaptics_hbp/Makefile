#
# Makefile for the Synaptics TCM touchscreen driver.
#

# Each configuration option enables a list of files.
subdir-ccflags-$(CONFIG_TOUCHPANEL_MTK_PLATFORM) += -I$(srctree)/drivers/misc/mediatek/include/
subdir-ccflags-$(CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY) += -I$(srctree)/drivers/misc/mediatek/include/


TCM_CORE=tcm/
ccflags-y += \
	-I$(srctree)/$(src) \
	-I$(srctree)/$(src)/../oplus_touchscreen_v2

subdir-ccflags-$(CONFIG_TOUCHPANEL_MTK_PLATFORM) += -I$(srctree)/drivers/misc/mediatek/include/
subdir-ccflags-$(CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY) += -I$(srctree)/drivers/misc/mediatek/include/

synaptics_tcm2-y := syna_tcm2.o

synaptics_tcm2-y += \
					$(TCM_CORE)synaptics_touchcom_core_v1.o \
					$(TCM_CORE)synaptics_touchcom_core_v2.o \
 					$(TCM_CORE)synaptics_touchcom_func_base.o \
 					$(TCM_CORE)synaptics_touchcom_func_touch.o \
					$(TCM_CORE)synaptics_touchcom_func_reflash.o \
					$(TCM_CORE)synaptics_touchcom_func_romboot.o

synaptics_tcm2-y += syna_tcm2_platform_spi.o
synaptics_tcm2-y += syna_tcm2_sysfs.o
synaptics_tcm2-y += syna_tcm2_testing.o
synaptics_tcm2-y += synaptics_common.o
synaptics_tcm2-y += touchpanel_proc.o
synaptics_tcm2-y += touch_comon_api/touch_comon_api.o
synaptics_tcm2-y += touchpanel_autotest/touchpanel_autotest.o
synaptics_tcm2-y += touchpanel_healthinfo/touchpanel_healthinfo.o
synaptics_tcm2-y += touchpanel_healthinfo/touchpanel_exception.o

obj-m += oplus_bsp_synaptics_tcm2.o
oplus_bsp_synaptics_tcm2-y := $(synaptics_tcm2-y)
ccflags-y += \
	-DCONFIG_TOUCHPANEL_OPLUS_MODULE \
	-DBUILD_BY_BAZEL \
	-DTOUCHPANEL_STATS_TRACE_INCLUDE_PATH=$(srctree)/$(src)/touchpanel_healthinfo

KBUILD_EXTRA_SYMBOLS := $(M)/../oplus_touchscreen_v2/touch_custom/Module.symvers
