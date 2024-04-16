load(":video_modules.bzl", "video_driver_modules")
load(":video_driver_build.bzl", "define_consolidate_gki_modules")

def define_pineapple():
    define_consolidate_gki_modules(
        target = "pineapple",
        registry = video_driver_modules,
        modules = [
            "msm_video",
            "video",
        ],
        config_options = [
            "CONFIG_MSM_VIDC_PINEAPPLE",
        ],
    )

def define_volcano():
    define_consolidate_gki_modules(
        target = "volcano",
        registry = video_driver_modules,
        modules = [
            "msm_video",
        ],
        config_options = [
            "CONFIG_MSM_VIDC_VOLCANO",
        ],
    )