PWR_PATH = "pwr"
SLIMBUS_PATH = "slimbus"
FMRTC_PATH = "rtc6226"

# This dictionary holds all the BT modules included in the bt-kernel
bt_modules = {}

def register_bt_modules(name, path = None, config_opt = None, srcs = {}, deps = []):
    """
    Register modules
    Args:
        name: Name of the module (which will be used to generate the name of the .ko file)
        path: Path in which the source files can be found
        config_opt: Config name used in Kconfig (not needed currently)
        srcs: source files and local headers
        deps: a list of dependent targets
    """
    module = struct(
        name = name,
        path = path,
        srcs = srcs,
        config_opt = config_opt,
        deps =  deps
    )
    bt_modules[name] = module

# --- BT Modules ---

register_bt_modules(
    name = "btpower",
    path = PWR_PATH,
    config_opt = "CONFIG_MSM_BT_PWR",
    srcs = ["btpower.c"]
)

register_bt_modules(
    name = "bt_fm_slim",
    path = SLIMBUS_PATH,
    config_opt = "CONFIG_BTFM_SLIM",
    srcs = [
        "btfm_slim.c",
        "btfm_slim.h",
        "btfm_slim_codec.c",
        "btfm_slim_slave.c",
        "btfm_slim_slave.h",
    ],
    deps = [":%b_btpower"]
)

register_bt_modules(
    name = "radio-i2c-rtc6226-qca",
    path = FMRTC_PATH,
    config_opt = "CONFIG_I2C_RTC6226_QCA",
    srcs = [
        "radio-rtc6226-common.c",
        "radio-rtc6226-i2c.c",
        "radio-rtc6226.h",
    ]
)
