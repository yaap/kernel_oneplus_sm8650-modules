#
# Makefile for the touchscreen drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_TOUCHPANEL_SYNAPTICS_S3706) += S3706/
obj-$(CONFIG_TOUCHPANEL_SYNAPTICS_TCM_ONCELL) += Syna_tcm_oncell/
obj-y += Syna_tcm_S3910/
obj-y += Syna_tcm_S3908/
obj-y += TD4377_noflash/

oplus_bsp_tp_syna_comnon-y += synaptics_touch_panel_remote.o
oplus_bsp_tp_syna_comnon-y += synaptics_common.o
oplus_bsp_tp_syna_common-y += $(oplus_bsp_tp_syna_comnon-y)
obj-m += oplus_bsp_tp_syna_common.o
#endif

obj-$(CONFIG_KUNIT_OPLUS_SYNAPTICS) += kunit_synaptics.o
