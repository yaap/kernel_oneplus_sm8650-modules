// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>

#include "linux/qti-smmu-proxy.h"

#define SMMU_PROXY_MAX_DEVS 1
static dev_t smmu_proxy_dev_no;
static struct class *smmu_proxy_class;
static struct cdev smmu_proxy_char_dev;

union smmu_proxy_ioctl_arg {
	struct csf_version csf_version;
};

int smmu_proxy_get_csf_version(struct csf_version *csf_version)
{
	csf_version->arch_ver = 2;
	csf_version->max_ver = 0;
	csf_version->min_ver = 0;

	return 0;
}
EXPORT_SYMBOL(smmu_proxy_get_csf_version);

static long smmu_proxy_dev_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	unsigned int dir = _IOC_DIR(cmd);
	union smmu_proxy_ioctl_arg ioctl_arg;

	if (_IOC_SIZE(cmd) > sizeof(ioctl_arg))
		return -EINVAL;

	if (copy_from_user(&ioctl_arg, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	if (!(dir & _IOC_WRITE))
		memset(&ioctl_arg, 0, sizeof(ioctl_arg));

	switch (cmd) {
	case QTI_SMMU_PROXY_GET_VERSION_IOCTL:
	{
		struct csf_version *csf_version =
			&ioctl_arg.csf_version;
		int ret;

		ret = smmu_proxy_get_csf_version(csf_version);
		if(ret)
			return ret;

		break;
	}
	default:
		return -ENOTTY;
	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &ioctl_arg,
				 _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	return 0;
}


static const struct file_operations smmu_proxy_dev_fops = {
	.unlocked_ioctl = smmu_proxy_dev_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
};

static int smmu_proxy_create_dev(struct platform_device *pdev)
{
	int ret;
	struct device *class_dev;

	cdev_init(&smmu_proxy_char_dev, &smmu_proxy_dev_fops);
	ret = cdev_add(&smmu_proxy_char_dev, smmu_proxy_dev_no,
		       SMMU_PROXY_MAX_DEVS);
	if (ret < 0)
		return ret;

	class_dev = device_create(smmu_proxy_class, NULL, smmu_proxy_dev_no, NULL,
				  "qti-smmu-proxy");
	if (IS_ERR(class_dev)) {
		ret = PTR_ERR(class_dev);
		goto err_dev_create;
	}

	return 0;

err_dev_create:
	cdev_del(&smmu_proxy_char_dev);
	return ret;
}

static int smmu_proxy_probe(struct platform_device *pdev)
{
	return smmu_proxy_create_dev(pdev);
}

static const struct of_device_id smmu_proxy_match_table[] = {
	{.compatible = "smmu-proxy-sender"},
	{},
};

static struct platform_driver smmu_proxy_driver = {
	.probe = smmu_proxy_probe,
	.driver = {
		.name = "qti-smmu-proxy",
		.of_match_table = smmu_proxy_match_table,
	},
};

static int __init init_smmu_proxy_driver(void)
{
	int ret;

	ret = alloc_chrdev_region(&smmu_proxy_dev_no, 0, SMMU_PROXY_MAX_DEVS,
				  "qti-smmu-proxy");
	if (ret < 0)
		goto err_chrdev_region;

	smmu_proxy_class = class_create(THIS_MODULE, "qti-smmu-proxy");
	if (IS_ERR(smmu_proxy_class)) {
		ret = PTR_ERR(smmu_proxy_class);
		goto err_class_create;
	}

	ret = platform_driver_register(&smmu_proxy_driver);
	if (ret < 0)
		goto err_platform_drvr_register;

	return 0;

err_platform_drvr_register:
	class_destroy(smmu_proxy_class);
err_class_create:
	unregister_chrdev_region(smmu_proxy_dev_no, SMMU_PROXY_MAX_DEVS);
err_chrdev_region:
	return ret;

}
module_init(init_smmu_proxy_driver);

MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
