/*
 * Allwinner sun7i low res adc input driver
 *
 * Copyright 2014 olegvedi@gmail.com
 *
 * based on:
 *
 * Allwinner sun4i low res adc attached tablet keys driver
 * Copyright (C) 2014 Hans de Goede <hdego...@redhat.com>
 *
 * and
 *
 * mcp3021.c - driver for the Microchip MCP3021 chip
 *
 * Copyright (C) 2008-2009, 2012 Freescale Semiconductor, Inc.
 * Author: Mingkai Hu <Mingkai.hu@freescale.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hwmon.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#define LRADC_BASEADR	0x01C22800
#define LRADC_BASELEN	4*5

#define LRADC_CTRL	0x00
#define LRADC_INTC	0x04
#define LRADC_INTS	0x08
#define LRADC_DATA0	0x0c
#define LRADC_DATA1	0x10

/* LRADC_CTRL bits */
#define FIRST_CONVERT_DLY(x)   ((x) << 24) /* 8 bits */
#define CHAN_SELECT(x)         ((x) << 22) /* 2 bits */
#define CONTINUE_TIME_SEL(x)   ((x) << 16) /* 4 bits */
#define KEY_MODE_SEL(x)        ((x) << 12) /* 2 bits */
#define LEVELA_B_CNT(x)        ((x) << 8)  /* 4 bits */
#define HOLD_EN(x)             ((x) << 6)
#define LEVELB_VOL(x)          ((x) << 4)  /* 2 bits */
#define SAMPLE_RATE(x)         ((x) << 2)  /* 2 bits */
#define ENABLE(x)              ((x) << 0)

/* output format */
//#define DAC_OUTPUT_SCALE	34532 //uV

struct sux7i_lradc_data {
	struct device *hwmon_dev;
	void __iomem *base;
	struct resource *mem;
};

static ssize_t show_in_input(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sux7i_lradc_data *data = dev_get_drvdata(dev);
	int in_input;

	in_input = readl(data->base + LRADC_DATA0);

	return sprintf(buf, "%d\n", in_input);
}

static int sun7i_lradc_open(struct sux7i_lradc_data *lradc)
{
/*        * Set sample time to 16 ms / 62.5 Hz. Wait 2 * 16 ms for key to
        * stabilize on press, wait (1 + 1) * 16 ms for key release        */
//	writel(FIRST_CONVERT_DLY(2) | LEVELA_B_CNT(1) | HOLD_EN(1) |
//               SAMPLE_RATE(2) | ENABLE(1), lradc->base + LRADC_CTRL);
//	writel(CHAN0_KEYUP_IRQ | CHAN0_KEYDOWN_IRQ, lradc->base + LRADC_INTC);
	writel(FIRST_CONVERT_DLY(2) | HOLD_EN(0) |
               SAMPLE_RATE(3) | ENABLE(1), lradc->base + LRADC_CTRL);

	return 0;
}

static void sun7i_lradc_close(struct sux7i_lradc_data *lradc)
{
/* Disable lradc, leave other settings unchanged */
	writel(FIRST_CONVERT_DLY(2) | LEVELA_B_CNT(1) | HOLD_EN(1) |
		    SAMPLE_RATE(2), lradc->base + LRADC_CTRL);
	writel(0, lradc->base + LRADC_INTC);
}

static DEVICE_ATTR(in0_input, S_IRUGO, show_in_input, NULL);

static int sun7i_lradc_probe(struct platform_device *pdev)
{
	int err;
	struct sux7i_lradc_data *data = NULL;
//	struct device *dev = &pdev->dev;

	data = kzalloc(sizeof(struct sux7i_lradc_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

/*        data->mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (!data->mem)
	{
	    err = -ENOENT;
            dev_err(&pdev->dev, "Failed to get platform mmio resource\n");
	    goto exit_free;
        }
*/
//        data->mem = request_mem_region(data->mem->start, resource_size(data->mem), pdev->name);
        data->mem = request_mem_region(LRADC_BASEADR, LRADC_BASELEN, pdev->name);
        if (!data->mem)
	{
	    err = -EBUSY;
	    dev_err(&pdev->dev, "Failed to request mmio memory region\n");
	    goto exit_free;
        }

        data->base = ioremap_nocache(data->mem->start, resource_size(data->mem));
        if (!data->base)
	{
	    err = -EBUSY;
	    dev_err(&pdev->dev, "Failed to ioremap mmio memory\n");
	    goto exit_release_mem;
        }

	platform_set_drvdata(pdev, data);

	err = sysfs_create_file(&pdev->dev.kobj, &dev_attr_in0_input.attr);
	if (err)
		goto exit_unset;

	data->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove;
	}

	sun7i_lradc_open(data);
	return 0;

exit_remove:
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_in0_input.attr);
exit_unset:
	platform_set_drvdata(pdev, NULL);
exit_release_mem:
	release_mem_region(data->mem->start, resource_size(data->mem));
exit_free:
	kfree(data);
	return err;
}

static int __devexit sun7i_lradc_remove(struct platform_device *pdev)
{
	struct sux7i_lradc_data *data = platform_get_drvdata(pdev);

	sun7i_lradc_close(data);
	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_file(&pdev->dev.kobj, &dev_attr_in0_input.attr);
	release_mem_region(data->mem->start, resource_size(data->mem));
        platform_set_drvdata(pdev, NULL);
        kfree(data);

	return 0;
}

static struct platform_driver sun7i_lradc_driver = {
	.probe = sun7i_lradc_probe,
	.remove = __devexit_p(sun7i_lradc_remove),
	.driver = {
	    .name = "sun7i_lradc-input",
	    .owner = THIS_MODULE,
	},
};

//module_platform_driver(sun7i_lradc_driver);
static struct platform_device *sun7i_lradc_device;

static int __init sun7i_lradc_init(void)
{
    int ret;

    ret = platform_driver_register(&sun7i_lradc_driver);

    if (ret) return ret;

    sun7i_lradc_device = platform_device_register_simple(sun7i_lradc_driver.driver.name, -1,NULL, 0);

    if (IS_ERR(sun7i_lradc_device))
    {
	platform_driver_unregister(&sun7i_lradc_driver);
	return PTR_ERR(sun7i_lradc_device);
    }
    return 0;
}

static void __exit sun7i_lradc_exit(void)
{
    platform_device_unregister(sun7i_lradc_device);
    platform_driver_unregister(&sun7i_lradc_driver);
}


module_init(sun7i_lradc_init);
module_exit(sun7i_lradc_exit);

MODULE_AUTHOR("<olegvedi@gmail.com>");
MODULE_DESCRIPTION("SUN7i LRDAC input");
MODULE_LICENSE("GPL");
