load("//external_modules/synx-kernel:synx_modules.bzl", "synx_modules")
load("//external_modules/synx-kernel:synx_module_build.bzl", "define_consolidate_gki_modules")

def define_pineapple():
    define_consolidate_gki_modules(
        target = "pineapple",
        registry = synx_modules,
        modules = [
            "synx",
            "ipclite",
        ],
        config_options = [
            "TARGET_SYNX_ENABLE",
        ],
    )
