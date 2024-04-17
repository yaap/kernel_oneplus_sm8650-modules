KBUILD_OPTIONS+= EVA_ROOT=$(KERNEL_SRC)/$(M)

KBUILD_EXTRA_SYMBOLS := \
    $(OUT_DIR)/../sm8650-modules/qcom/opensource/mmrm-driver/Module.symvers \
    $(OUT_DIR)/../sm8650-modules/qcom/opensource/synx-kernel/Module.symvers \
    $(OUT_DIR)/../sm8650-modules/qcom/opensource/dsp-kernel/Module.symvers

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) M=$(M) -C $(KERNEL_SRC) modules_install

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
