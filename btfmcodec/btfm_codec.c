// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/kdev_t.h>
#include <linux/refcount.h>
#include <linux/idr.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/module.h>
#include "btfm_codec.h"
//#include "btfm_codec_hw_interface.h"

#define dev_to_btfmcodec(_dev) container_of(_dev, struct btfmcodec_data, dev)

static DEFINE_IDR(dev_minor);
static struct class *dev_class;
static dev_t dev_major;
struct btfmcodec_data *btfmcodec;
struct device_driver driver = {.name = "btfmcodec-driver", .owner = THIS_MODULE};
struct btfmcodec_char_device *btfmcodec_dev;

char *coverttostring(enum btfmcodec_states state) {
	switch (state) {
	case IDLE:
		return "IDLE";
		break;
	case BT_Connected:
		return "BT_CONNECTED";
		break;
	case BT_Connecting:
		return "BT_CONNECTING";
		break;
	case BTADV_AUDIO_Connected:
		return "BTADV_AUDIO_CONNECTED";
		break;
	case BTADV_AUDIO_Connecting:
		return "BTADV_AUDIO_CONNECTING";
		break;
	default:
		return "INVALID_STATE";
		break;
	}
}

static const struct file_operations btfmcodec_fops = {
	.owner = THIS_MODULE,
/*	.open = glink_pkt_open,
	.release = glink_pkt_release,
	.read = glink_pkt_read,
	.write = glink_pkt_write,
	.poll = glink_pkt_poll,
	.unlocked_ioctl = glink_pkt_ioctl,
	.compat_ioctl = glink_pkt_ioctl,
*/};

static ssize_t btfmcodec_attributes_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t n)
{
	struct btfmcodec_data *btfmcodec = dev_to_btfmcodec(dev);
	struct btfmcodec_char_device *btfmcodec_dev = btfmcodec->btfmcodec_dev;
	long tmp;

	mutex_lock(&btfmcodec_dev->lock);
	if (kstrtol(buf, 0, &tmp)) {
		mutex_unlock(&btfmcodec_dev->lock);
/*		BTFMCODEC_ERR("unable to convert string to int for /dev/%s\n",
			      btfmcodec->dev->name);
*/		return -EINVAL;
	}
	mutex_unlock(&btfmcodec_dev->lock);

	return n;
}

static ssize_t btfmcodec_attributes_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
//	struct btfmcodec_char_device *btfmcodec_dev = dev_to_btfmcodec(dev);

	return 0;
}

struct btfmcodec_data* btfm_get_btfmcodec(void)
{
	return btfmcodec;
}
EXPORT_SYMBOL(btfm_get_btfmcodec);

static DEVICE_ATTR_RW(btfmcodec_attributes);

static int __init btfmcodec_init(void)
{
	struct btfmcodec_state_machine *states;
	struct btfmcodec_char_device *btfmcodec_dev;
	struct device *dev;
	int ret;

	BTFMCODEC_INFO("starting up the module");
	btfmcodec = kzalloc(sizeof(struct btfmcodec_data), GFP_KERNEL);
	if (!btfmcodec) {
		BTFMCODEC_ERR("failed to allocate memory");
		return -ENOMEM;
	}

	states = &btfmcodec->states;
	states->current_state = IDLE;
	states->next_state = IDLE;

	BTFMCODEC_INFO("creating device node");
	/* create device node for communication between userspace and kernel */
	btfmcodec_dev = kzalloc(sizeof(struct btfmcodec_char_device), GFP_KERNEL);
	if (!btfmcodec_dev) {
		BTFMCODEC_ERR("failed to allocate memory");
		ret = -ENOMEM;
		goto info_cleanup;
	}

	BTFMCODEC_INFO("trying to get major number\n");
	ret = alloc_chrdev_region(&dev_major, 0, 0, "btfmcodec");
	if (ret < 0) {
		BTFMCODEC_ERR("failed to allocate character device region");
		goto dev_cleanup;
	}

	BTFMCODEC_INFO("creating btfm codec class");
	dev_class = class_create(THIS_MODULE, "btfmcodec");
	if (IS_ERR(dev_class)) {
		ret = PTR_ERR(dev_class);
		BTFMCODEC_ERR("class_create failed ret:%d\n", ret);
		goto deinit_chrdev;
	}

	btfmcodec_dev->reuse_minor = idr_alloc(&dev_minor, btfmcodec, 1, 0, GFP_KERNEL);
	if (ret < 0) {
		BTFMCODEC_ERR("failed to allocated minor number");
		goto deinit_class;
	}

	dev = &btfmcodec->dev;
	dev->driver = &driver;

	// ToDo Rethink of having btfmcodec alone instead of btfmcodec
	btfmcodec->btfmcodec_dev = btfmcodec_dev;
	refcount_set(&btfmcodec_dev->active_clients, 1);
	mutex_init(&btfmcodec_dev->lock);
	strlcpy(btfmcodec_dev->dev_name, "btfmcodec_dev", DEVICE_NAME_MAX_LEN);
	device_initialize(dev);
	dev->class = dev_class;
	dev->devt = MKDEV(MAJOR(dev_major), btfmcodec_dev->reuse_minor);
	dev_set_drvdata(dev, btfmcodec);

	cdev_init(&btfmcodec_dev->cdev, &btfmcodec_fops);
	btfmcodec_dev->cdev.owner = THIS_MODULE;
	dev_set_name(dev, btfmcodec_dev->dev_name, btfmcodec_dev->reuse_minor);
	ret = cdev_add(&btfmcodec_dev->cdev, dev->devt, 1);
	if (ret) {
		BTFMCODEC_ERR("cdev_add failed with error no %d", ret);
		goto idr_cleanup;
	}

	// ToDo to handler HIDL abrupt kill
	dev->release = NULL;
	ret = device_add(dev);
	if (ret) {
		BTFMCODEC_ERR("Failed to add device error no %d", ret);
		goto free_device;
	}

	BTFMCODEC_ERR("Creating a sysfs entry with name: %s", btfmcodec_dev->dev_name);
	ret = device_create_file(dev, &dev_attr_btfmcodec_attributes);
	if (ret) {
		BTFMCODEC_ERR("Failed to create a devicd node: %s", btfmcodec_dev->dev_name);
		goto free_device;
	}

	BTFMCODEC_INFO("created a node at /dev/%s with %u:%u\n",
		btfmcodec_dev->dev_name, dev_major, btfmcodec_dev->reuse_minor);

	return ret;

free_device:
	put_device(dev);
idr_cleanup:
	idr_remove(&dev_minor, btfmcodec_dev->reuse_minor);
deinit_class:
	class_destroy(dev_class);
deinit_chrdev:
	unregister_chrdev_region(MAJOR(dev_major), 0);
dev_cleanup:
	kfree(btfmcodec_dev);
info_cleanup:
	kfree(btfmcodec);

	return ret;
}

static void __exit btfmcodec_deinit(void)
{
	struct btfmcodec_char_device *btfmcodec_dev;
	struct device *dev;
	BTFMCODEC_INFO("cleaning up btfm codec driver", __func__);
	if (!btfmcodec) {
		BTFMCODEC_ERR("skiping driver cleanup", __func__);
		goto info_cleanup;
	}

	dev = &btfmcodec->dev;

	device_remove_file(dev, &dev_attr_btfmcodec_attributes);
	put_device(dev);

	if (!btfmcodec->btfmcodec_dev) {
		BTFMCODEC_ERR("skiping device node cleanup", __func__);
		goto info_cleanup;
	}

	btfmcodec_dev = btfmcodec->btfmcodec_dev;
	idr_remove(&dev_minor, btfmcodec_dev->reuse_minor);
	class_destroy(dev_class);
	unregister_chrdev_region(MAJOR(dev_major), 0);
	kfree(btfmcodec_dev);
info_cleanup:
	kfree(btfmcodec);
	BTFMCODEC_INFO("btfm codec driver cleanup completed", __func__);
	return;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM Bluetooth FM CODEC driver");

module_init(btfmcodec_init);
module_exit(btfmcodec_deinit);
