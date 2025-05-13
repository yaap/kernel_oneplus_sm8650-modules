load(":bt_kernel.bzl", "define_bt_modules")

def define_pineapple():
    define_bt_modules(
        target = "pineapple",
        modules = [
            "btpower",
            "bt_fm_slim",
            "radio-i2c-rtc6226-qca",
            "bt_fm_swr",
            "btfmcodec",
        ],
        config_options = [
            "CONFIG_MSM_BT_POWER",
            "CONFIG_BTFM_SLIM",
            "CONFIG_I2C_RTC6226_QCA",
            "CONFIG_BTFM_SWR",
            "CONFIG_BTFM_CODEC",
            "CONFIG_BT_HW_SECURE_DISABLE",
        ]
    )

def define_blair():
    define_bt_modules(
        target = "blair",
        modules = [
            "btpower",
            "bt_fm_slim",
        ],
        config_options = [
            "CONFIG_MSM_BT_POWER",
            "CONFIG_BTFM_SLIM",
            "CONFIG_BT_HW_SECURE_DISABLE",
        ]
    )

def define_pitti():
    define_bt_modules(
	target = "pitti",
	modules = [
	    "btpower",
	    "bt_fm_slim",
	],
	config_options = [
	    "CONFIG_MSM_BT_POWER",
	    "CONFIG_BTFM_SLIM",
	    "CONFIG_BT_HW_SECURE_DISABLE",
	]
   )

def define_niobe():
    define_bt_modules(
	target = "niobe",
	modules = [
	    "btpower",
	    "bt_fm_slim",
	],
	config_options = [
	    "CONFIG_MSM_BT_POWER",
	    "CONFIG_BTFM_SLIM",
	    "CONFIG_BT_HW_SECURE_DISABLE",
	]
   )

def define_anorak61():
    define_bt_modules(
	target = "anorak61",
	modules = [
	    "btpower",
	    "bt_fm_slim",
	],
	config_options = [
	    "CONFIG_MSM_BT_POWER",
	    "CONFIG_BTFM_SLIM",
	    "CONFIG_BT_HW_SECURE_DISABLE",
	]
   )

def define_neo61():
    define_bt_modules(
	target = "neo-la",
	modules = [
	    "btpower",
	    "bt_fm_slim",
	],
	config_options = [
	    "CONFIG_MSM_BT_POWER",
	    "CONFIG_BTFM_SLIM",
	]
   )

def define_volcano():
    define_bt_modules(
	target = "volcano",
	modules = [
	    "btpower",
	    "btfmcodec",
	    "bt_fm_slim",
	    "radio-i2c-rtc6226-qca",
	    "bt_fm_swr",
	],
	config_options = [
	    "CONFIG_MSM_BT_POWER",
	    "CONFIG_BTFM_CODEC",
	    "CONFIG_BTFM_SLIM",
	    "CONFIG_I2C_RTC6226_QCA",
	    "CONFIG_BTFM_SWR",
	    "CONFIG_BT_HW_SECURE_DISABLE",
	]
   )

def define_seraph():
    define_bt_modules(
	target = "seraph",
	modules = [
	    "btpower",
	    "btfmcodec",
	    "bt_fm_slim",
	    "bt_fm_swr",
	],
	config_options = [
	    "CONFIG_MSM_BT_POWER",
	    "CONFIG_BTFM_CODEC",
	    "CONFIG_BTFM_SLIM",
	    "CONFIG_BTFM_SWR",
	    "CONFIG_BT_HW_SECURE_DISABLE",
	]
   )
