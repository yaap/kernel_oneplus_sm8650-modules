ifeq ($(KGSL_MODULE_ROOT),)
CUR_MKFILE = $(abspath $(lastword $(MAKEFILE_LIST)))
KGSL_MODULE_ROOT = $(dir $(CUR_MKFILE))
endif

KBUILD_OPTIONS += CONFIG_QCOM_KGSL=m
KBUILD_OPTIONS += BOARD_PLATFORM=pineapple
KBUILD_OPTIONS += $(KGSL_SELECT)
KBUILD_OPTIONS += MODNAME=msm_kgsl
KBUILD_OPTIONS += KGSL_PATH=$(KGSL_MODULE_ROOT)

KBUILD_EXTRA_SYMBOLS := \
    $(OUT_DIR)/../sm8650-modules/qcom/opensource/mm-drivers/hw_fence/Module.symvers

all: modules

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	rm -f *.cmd *.d *.mod *.o *.ko *.mod.c *.mod.o Module.symvers modules.order

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)
