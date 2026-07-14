// SPDX-License-Identifier: GPL-2.0-only
/*
 * eSIM/Physical UIM2 Switcher Driver
 * Copyright (C) 2026 The YAAP Project
 *
 * This driver provides manual control of UIM2 via sysfs.
 * UIM2 is shared between the eSIM and the second physical SIM slot.
 * A hardware mux GPIO controls which path is active.
 *
 * - Write 0: Enable eSIM (default)
 * - Write 1: Enable physical SIM2
 *
 * Depends on oplus_network_oem_qmi module for UIM QMI power control.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>

#define DRIVER_NAME "uim2-switch"
#define ESIM_UIM_SLOT_ID 2

/*
 * GPIO polarity:
 *   LOW  (gpio 0) = eSIM path active
 *   HIGH (gpio 1) = physical SIM2 path active
 */
#define GPIO_VAL_ESIM   0
#define GPIO_VAL_PSIM2  1

struct uim2_switch_data {
	int mux_gpio;
	bool psim2_active;
	struct mutex lock;
	struct device *dev;
};

extern int uim_qmi_power_up_req(u8 slot_id);
extern int uim_qmi_power_down_req(u8 slot_id);

static int do_switch(struct uim2_switch_data *data, bool to_psim2)
{
	int ret;
	int gpio_val = to_psim2 ? GPIO_VAL_PSIM2 : GPIO_VAL_ESIM;
	const char *target = to_psim2 ? "PSIM2" : "eSIM";

	if (data->psim2_active == to_psim2) {
		dev_dbg(data->dev, "Already on %s\n", target);
		return 0;
	}

	dev_info(data->dev, "Switching to %s\n", target);

	ret = uim_qmi_power_down_req(ESIM_UIM_SLOT_ID);
	if (ret < 0)
		dev_warn(data->dev, "QMI power down failed: %d\n", ret);

	msleep(200);

	gpio_direction_output(data->mux_gpio, gpio_val);
	msleep(50);

	ret = uim_qmi_power_up_req(ESIM_UIM_SLOT_ID);
	if (ret < 0)
		dev_warn(data->dev, "QMI power up failed: %d\n", ret);

	msleep(300);

	data->psim2_active = to_psim2;
	dev_info(data->dev, "%s enabled\n", target);

	return 0;
}

static ssize_t uim2_state_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct uim2_switch_data *data = dev_get_drvdata(dev);
	int val;

	mutex_lock(&data->lock);
	val = data->psim2_active ? 1 : 0;
	mutex_unlock(&data->lock);

	return sysfs_emit(buf, "%d\n", val);
}

static ssize_t uim2_state_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct uim2_switch_data *data = dev_get_drvdata(dev);
	int val, ret;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val != 0 && val != 1)
		return -EINVAL;

	mutex_lock(&data->lock);
	ret = do_switch(data, val == 1);
	mutex_unlock(&data->lock);

	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR_RW(uim2_state);

static struct attribute *uim2_switch_attrs[] = {
	&dev_attr_uim2_state.attr,
	NULL,
};

static const struct attribute_group uim2_switch_attr_group = {
	.attrs = uim2_switch_attrs,
};

static int esim_switch_probe(struct platform_device *pdev)
{
	struct uim2_switch_data *data;
	struct device *dev = &pdev->dev;
	int ret, gpio_val;

	dev_info(dev, "probing\n");

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = dev;
	mutex_init(&data->lock);

	platform_set_drvdata(pdev, data);

	data->mux_gpio = of_get_named_gpio(dev->of_node,
					   "esim-control-gpios", 0);
	if (data->mux_gpio == -EPROBE_DEFER)
		return -EPROBE_DEFER;

	if (!gpio_is_valid(data->mux_gpio)) {
		dev_err(dev, "esim-control-gpios not valid\n");
		return -EINVAL;
	}

	ret = devm_gpio_request(dev, data->mux_gpio, "uim2-mux");
	if (ret) {
		dev_err(dev, "Failed to request GPIO: %d\n", ret);
		return ret;
	}

	gpio_val = gpio_get_value(data->mux_gpio);
	if (gpio_val < 0) {
		dev_warn(dev, "Cannot read GPIO, defaulting to eSIM\n");
		gpio_val = GPIO_VAL_ESIM;
	}

	/* We want to default to eSIM at boot */
	if (gpio_val == GPIO_VAL_PSIM2) {
		dev_info(dev, "GPIO indicates PSIM2, forcing eSIM at boot\n");
		gpio_direction_output(data->mux_gpio, GPIO_VAL_ESIM);
		data->psim2_active = false;
	} else {
		data->psim2_active = false;
	}

	ret = sysfs_create_group(&dev->kobj, &uim2_switch_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create sysfs group: %d\n", ret);
		return ret;
	}

	dev_info(dev, "initialized, control via /sys/devices/platform/%s/uim2_state\n",
		 dev_name(dev));

	return 0;
}

static int esim_switch_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct uim2_switch_data *data = platform_get_drvdata(pdev);

	sysfs_remove_group(&dev->kobj, &uim2_switch_attr_group);

	mutex_lock(&data->lock);
	if (data->psim2_active) {
		dev_info(dev, "Restoring to eSIM on removal\n");
		do_switch(data, false);
	}
	mutex_unlock(&data->lock);

	return 0;
}

static const struct of_device_id uim2_switch_dt_match[] = {
	{ .compatible = "oplus,uim2-switch" },
	{ }
};
MODULE_DEVICE_TABLE(of, uim2_switch_dt_match);

static struct platform_driver uim2_switch_driver = {
	.probe = esim_switch_probe,
	.remove = esim_switch_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = uim2_switch_dt_match,
	},
};

module_platform_driver(uim2_switch_driver);

MODULE_DESCRIPTION("Oplus eSIM/Physical UIM2 Switcher");
MODULE_AUTHOR("The YAAP Project");
MODULE_LICENSE("GPL v2");
