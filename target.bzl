load(":touch_modules.bzl", "touch_driver_modules")
load(":touch_modules_build.bzl", "define_consolidate_gki_modules")

def define_pineapple():
    define_consolidate_gki_modules(
        target = "pineapple",
        registry = touch_driver_modules,
        modules = [
            "nt36xxx-i2c",
            "atmel_mxt_ts",
            "dummy_ts",
            "goodix_ts"
        ],
        config_options = [
            "TOUCH_DLKM_ENABLE",
            "CONFIG_ARCH_PINEAPPLE",
            "CONFIG_MSM_TOUCH",
            "CONFIG_TOUCHSCREEN_GOODIX_BRL",
            "CONFIG_TOUCHSCREEN_NT36XXX_I2C",
            "CONFIG_TOUCHSCREEN_ATMEL_MXT",
            "CONFIG_TOUCHSCREEN_DUMMY"
        ],
)
