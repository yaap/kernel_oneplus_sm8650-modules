load("//msm-kernel:target_variants.bzl", "get_all_variants")
load("//build/kernel/kleaf:kernel.bzl", "ddk_module")

load(":bt_modules.bzl", "bt_modules")

def _get_module_srcs(module):
    """
    Gets all the module sources, formats them with the path for that module
    and then groups them together
    It also includes all the headers within the `include` directory
    `native.glob()` returns a new list with every file need for the current package
    """
    return native.glob(
        ["{}/{}".format(module.path, src) for src in module.srcs] + ["include/*.h"]
    )

def _get_module_deps(module, formatter):
    """
    Formats the dependent targets with the necessary prefix
    Args:
        module: kernel module
        formatter: function that will replace the format string within `deps`
    Example:
        kernel build = "pineapple_gki"
        dep = "%b_btpower"
        The formatted string will look as follow
        formatted_dep = formatter(dep) = "pineapple_gki_btpower"
    """
    return [formatter(dep) for dep in module.deps]

def define_target_variant_modules(target, variant, modules):
    """
    Generates the ddk_module for each of our kernel modules
    Args:
        target: either `pineapple` or `kalama`
        variant: either `gki` or `consolidate`
        modules: bt_modules dict defined in `bt_modules.bzl`
    """
    kernel_build = "{}_{}".format(target, variant)
    modules = [modules.get(module_name) for module_name in modules]
    formatter = lambda s : s.replace("%b", kernel_build)

    for module in modules:
        rule_name = "{}_{}".format(kernel_build, module.name)
        module_srcs = _get_module_srcs(module)

        ddk_module(
            name = rule_name,
            kernel_build = "//msm-kernel:{}".format(kernel_build),
            srcs = module_srcs,
            out = "{}.ko".format(module.name),
            deps = ["//msm-kernel:all_headers"] + _get_module_deps(module, formatter),
            includes = ["include"],
            visibility = ["//visibility:public"],
        )

def define_bt_modules(targets):
    for target in targets:
        for (t, v) in get_all_variants():
            if t == target:
                define_target_variant_modules(t, v, bt_modules)
