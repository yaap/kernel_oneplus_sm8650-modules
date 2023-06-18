KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

M ?= $(shell pwd)

KBUILD_OPTIONS += NFC_ROOT=$(KERNEL_SRC)/$(M)
KBUILD_OPTIONS += KBUILD_EXTRA_SYMBOLS=$(OUT_DIR)/../sm8650-modules/qcom/opensource/securemsm-kernel/Module.symvers

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean
