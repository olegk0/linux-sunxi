/*
 * Backlight driver for SUNXI PWM
 *
 * Copyright 2014 olegvedi@gmail.com
 *
 * based on:
 * Backlight driver for Wolfson Microelectronics WM831x PMICs
 * Copyright 2009 Wolfson Microelectonics plc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <video/sunxi_disp_ioctl.h>

#define MIN_BRT 50
#define PWM_CH 0

extern __s32 pwm_set_para(__u32 channel, __pwm_info_t *pwm_info);
extern __s32 pwm_get_para(__u32 channel, __pwm_info_t *pwm_info);

struct sunxipwm_backlight_data {
	__pwm_info_t pwm_info;
};

static int sunxipwm_backlight_set(struct backlight_device *bl, int brightness)
{
	struct sunxipwm_backlight_data *data = bl_get_data(bl);

	if(brightness > data->pwm_info.period_ns)
	    data->pwm_info.duty_ns = data->pwm_info.period_ns;
	else{
	    if(brightness < MIN_BRT)
		data->pwm_info.duty_ns = MIN_BRT;
	    else
		data->pwm_info.duty_ns = brightness;
	}

	if(brightness == 0)
	    data->pwm_info.enable = 0;
	else
	    data->pwm_info.enable = 1;

	pwm_set_para(PWM_CH, &data->pwm_info);

	return 0;
}

static int sunxipwm_backlight_update_status(struct backlight_device *bl)
{
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.state & BL_CORE_SUSPENDED)
		brightness = 0;

	return sunxipwm_backlight_set(bl, brightness);
}

static int sunxipwm_backlight_get_brightness(struct backlight_device *bl)
{
//	struct sunxipwm_backlight_data *data = bl_get_data(bl);

//	pwm_get_para(PWM_CH, &data->pwm_info);
//	return data->pwm_info.duty_ns;
	return bl->props.brightness;
}

static const struct backlight_ops sunxipwm_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status	= sunxipwm_backlight_update_status,
	.get_brightness	= sunxipwm_backlight_get_brightness,
};

static int sunxipwm_backlight_probe(struct platform_device *pdev)
{
	struct sunxipwm_backlight_data *data;
	struct backlight_device *bl;
	struct backlight_properties props;
	__pwm_info_t *pwm_info;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	pwm_info = &data->pwm_info;

	pwm_get_para(PWM_CH, pwm_info);

	printk("sunxipwm:PWM state - enable:%d,active_state:%d,period_ns:%d,duty_ns:%d\n"
	    ,pwm_info->enable,pwm_info->active_state,pwm_info->period_ns,pwm_info->duty_ns);

	pwm_info->enable = 1;
	pwm_info->active_state = 1;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = pwm_info->period_ns;

	bl = backlight_device_register(pdev->name, &pdev->dev, data,
				       &sunxipwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}

	bl->props.brightness = pwm_info->duty_ns;

	platform_set_drvdata(pdev, bl);

	backlight_update_status(bl);

	return 0;
}

static int sunxipwm_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	backlight_device_unregister(bl);
	return 0;
}

static struct platform_driver sunxipwm_bl_driver = {
	.driver		= {
		.name	= "sunxipwm-bl",
		.owner	= THIS_MODULE,
	},
	.probe		= sunxipwm_backlight_probe,
	.remove		= sunxipwm_backlight_remove,
};

//module_platform_driver(sunxipwm_bl_driver);
static struct platform_device *sunxipwm_device;

static int __init sunxipwm_init(void)
{
    int ret;

    ret = platform_driver_register(&sunxipwm_bl_driver);

    if (ret) return ret;

    sunxipwm_device = platform_device_register_simple(sunxipwm_bl_driver.driver.name, -1,NULL, 0);

    if (IS_ERR(sunxipwm_device))
    {
	platform_driver_unregister(&sunxipwm_bl_driver);
	return PTR_ERR(sunxipwm_device);
    }
    return 0;
}

static void __exit sunxipwm_exit(void)
{
    platform_device_unregister(sunxipwm_device);
    platform_driver_unregister(&sunxipwm_bl_driver);
}

module_init(sunxipwm_init);
module_exit(sunxipwm_exit);

MODULE_DESCRIPTION("Backlight Driver for SUNXI PWM");
MODULE_AUTHOR("olegvedi@gmail.com");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sunxipwm-bl");
