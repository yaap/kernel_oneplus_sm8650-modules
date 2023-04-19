load(":mm_modules.bzl", "mm_driver_modules")
load(":mm_module_build.bzl", "define_consolidate_gki_modules")

def define_pineapple():
    define_consolidate_gki_modules(
        target = "pineapple",
        registry = mm_driver_modules,
        modules = [
            "hw_fence",
            "msm_ext_display",
            "sync_fence",
        ],
        config_options = [
            "CONFIG_DEBUG_FS",
        ],
)