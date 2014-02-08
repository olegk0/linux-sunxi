/*
 * My CubieBoard battery driver
 *
 * Copyright 2014 olegvedi@gmail.com
 *
 * based on
 *
 * drivers/power/virtual_battery.c
 * Virtual battery driver
 * Copyright (C) 2008 Pylone, Inc.
 * Author: Masashi YOKOTA <yokota@pylone.jp>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <plat/sys_config.h>

static struct platform_device *bat_pdev;

static enum power_supply_property mycb_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property mycb_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static int get_ac_status(void)
{
    u32 pio_hdle = 0;
    int ret;

    pio_hdle = gpio_request_ex("power_para",NULL);
    if(pio_hdle)
    {
	ret = gpio_read_one_pin_value(pio_hdle,"ac_status_pin");
        gpio_release(pio_hdle, 1);
	if(ret) ret = 0;
	else ret = 1;
	return(ret);
    }

    return(0);
}

static int mycb_ac_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	int ret = 0;

	dev_dbg(&bat_pdev->dev, "%s: psp=%d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = get_ac_status();
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

#define LRADC_MVOLT_ON_STEP 285
#define LRADC_PERC_ON_STEP 10
#define LRADC_MAX_HEALTH_STEP 44
#define LRADC_PERC_MIN_STEP 34
#define LRADC_MIN_HEALTH_STEP 30
#define LRADC_MIN_ONLINE_STEP 25

extern int get_raw_lradc0(void);
static int get_batt_data(void)
{
    int i,rdt;

    rdt = 0;
    for(i=0;i<10;i++)
	rdt += get_raw_lradc0();
    rdt += (i/2);
    return(rdt /i);
}

static int mycb_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0,tmp, res, ac;

	res = get_batt_data();
	ac = get_ac_status();
	dev_dbg(&bat_pdev->dev, "%s: psp=%d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	    if(ac){
		if(res >= LRADC_MAX_HEALTH_STEP)
		    val->intval = POWER_SUPPLY_STATUS_FULL;
		else
		    val->intval = POWER_SUPPLY_STATUS_CHARGING;
	    }
	    else
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
	    break;
	case POWER_SUPPLY_PROP_HEALTH:
	    if(res < LRADC_MIN_ONLINE_STEP)
		val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	    else if(res < LRADC_MIN_HEALTH_STEP)
		val->intval = POWER_SUPPLY_HEALTH_DEAD;
	    else if(res > LRADC_MAX_HEALTH_STEP)
		val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	    else
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
	    break;
	case POWER_SUPPLY_PROP_PRESENT:
	    if(res < LRADC_MIN_ONLINE_STEP)
		val->intval = 0;
	    else
		val->intval = 1;
	    break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	    val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
	    break;
	case POWER_SUPPLY_PROP_CAPACITY:
	    tmp = (res-LRADC_PERC_MIN_STEP) * LRADC_PERC_ON_STEP;
	    if(tmp > 100) tmp = 100;
	    else if(tmp < 0) tmp = 0;
	    val->intval = tmp;
	    break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	    if(res > LRADC_MIN_ONLINE_STEP)
	        val->intval = res * LRADC_MVOLT_ON_STEP;
	    else
	        val->intval = 0;
	    break;
	default: ret = -EINVAL;
		break;
	}

	return ret;
}

static struct power_supply power_supply_ac = {
	.properties = mycb_ac_props,
	.num_properties = ARRAY_SIZE(mycb_ac_props),
	.get_property = mycb_ac_get_property,
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_MAINS,
};

static struct power_supply power_supply_bat = {
	.properties = mycb_battery_props,
	.num_properties = ARRAY_SIZE(mycb_battery_props),
	.get_property = mycb_battery_get_property,
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
};

static int __init mycb_battery_init(void)
{
	int ret;

	bat_pdev = platform_device_register_simple(KBUILD_BASENAME, 0, NULL, 0);
	if (IS_ERR(bat_pdev))
		return PTR_ERR(bat_pdev);

	ret = power_supply_register(&bat_pdev->dev, &power_supply_ac);
	if (ret)
		goto err_battery_failed;

	ret = power_supply_register(&bat_pdev->dev, &power_supply_bat);
	if (ret)
		goto err_ac_failed;

	printk(KERN_INFO KBUILD_BASENAME": registered \n");
	return 0;

 err_battery_failed:
	power_supply_unregister(&power_supply_ac);
 err_ac_failed:
	return ret;
}

static void __exit mycb_battery_exit(void)
{
	power_supply_unregister(&power_supply_ac);
	power_supply_unregister(&power_supply_bat);
	platform_device_unregister(bat_pdev);
	printk(KERN_INFO KBUILD_BASENAME": unregistered \n");
}

module_init(mycb_battery_init);
module_exit(mycb_battery_exit);

MODULE_AUTHOR("<olegvedi@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My CubieBoard battery driver");
MODULE_ALIAS("platform:"KBUILD_BASENAME);
