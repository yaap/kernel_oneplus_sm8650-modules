load(":mm_module_build.bzl", "mm_driver_module_entry")

HW_FENCE_PATH = "hw_fence"
MSM_EXT_DISPLAY_PATH = "msm_ext_display"
SYNC_FENCE_PATH = "sync_fence"

mm_driver_modules = mm_driver_module_entry([":mm_drivers_headers"])
module_entry = mm_driver_modules.register

#--------------- MM-DRIVERS MODULES ------------------

module_entry(
    name = "hw_fence",
    path = HW_FENCE_PATH + "/src",
    config_option = "CONFIG_QTI_HW_FENCE",
    config_srcs = {
        "CONFIG_DEBUG_FS" : [
            "hw_fence_ioctl.c",
        ]
    },
    srcs = ["hw_fence_drv_debug.c",
            "hw_fence_drv_ipc.c",
            "hw_fence_drv_priv.c",
            "hw_fence_drv_utils.c",
            "msm_hw_fence.c",
            "msm_hw_fence_synx_translation.c"],
    deps =[
        "//vendor/qcom/opensource/synx-kernel:synx_headers"
    ]
)

module_entry(
    name = "msm_ext_display",
    path = MSM_EXT_DISPLAY_PATH + "/src",
    config_option = "CONFIG_MSM_EXT_DISPLAY",
    srcs = ["msm_ext_display.c"],
)

module_entry(
    name = "sync_fence",
    path = SYNC_FENCE_PATH + "/src",
    config_option = "CONFIG_QCOM_SPEC_SYNC",
    srcs = ["qcom_sync_file.c"],
)