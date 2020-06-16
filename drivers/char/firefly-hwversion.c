/*
 * Copyright (C) 2016, Zhongshan T-chip Intelligent Technology Co.,ltd.
 * Copyright 2006 ZL.Guan
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

#define HWVERSION_OF_RANGE_GT  "firefly,hwversion_range_gt"
#define HWVERSION_OF_RANGE_LT  "firefly,hwversion_range_lt"

static char hwversion[16]="0";
static u32 firefly_hwversion = 0;

static ssize_t show_hwversion_node( struct device *dev, struct device_attribute *attr, char *buf){
    return sprintf(buf, "%s", hwversion);
}
static DEVICE_ATTR(hwversion, 0444, show_hwversion_node, NULL);

static char *version2str(char *out, char *sversion){
    int ret;
    u32 version;
    /*
     * firefly_hwversion transfer, like:
     *  sversion        out
        0x00010000  ->  1.0
        0x00020001  ->  2.1
        0x00210523  ->  21.523
    */

    ret = kstrtou32(sversion+2, 10, &version);
    if(0 == ret) {
        sprintf(out, "%d.%d", version/10000,version%10000);
        return out;
    } else {
        return NULL;
    }

}

static int __init firefly_hwversion_setup (char *str){
    int ret;

    ret = kstrtou32(str, 16, &firefly_hwversion);
    if (ret) {
        printk("%s: convert to u32 error: %s\n", __FUNCTION__, str);
    }
    version2str(hwversion,str);
	return 1;
}
__setup("hwversion=", firefly_hwversion_setup);


bool firefly_hwversion_in_range(struct device_node *device){
    u32 compare[2]; // compare[0]: 1-eq, 0-neq
    int length, cond_count=0;
	struct property *prop;

	const char *compatible;

	of_property_read_string(device, "compatible", &compatible);
    //printk("%s: check %s range\n", __FUNCTION__, compatible);

    /* firefly,hwversion_range_gt
     * compare[0] == 0: >
     * compare[0] == 1: >=
     */
	prop = of_find_property(device, HWVERSION_OF_RANGE_GT, &length);
	if (prop) {
        if(2 == (length / sizeof(u32))) {
            of_property_read_u32_array(device, HWVERSION_OF_RANGE_GT, compare, 2);
            // when </<= compare[1], not in range 
            if(firefly_hwversion < compare[1] || (compare[0] == 0 && firefly_hwversion == compare[1])) {
				printk("%s %s: hwversion should >%s 0x%08x!\n", __FUNCTION__, compatible, 
                        compare[0] == 0 ? "" : "=", compare[1]);
                return false;
			} else {
                cond_count++;
				printk("%s %s: hwversion >%s 0x%08x\n", __FUNCTION__, compatible, 
                        compare[0] == 0 ? "" : "=", compare[1]);
            }
        } else {
            printk("%s %s: parse '%s' error!\n", __FUNCTION__, compatible, HWVERSION_OF_RANGE_GT);
            return false;
        }
    }

    /* firefly,hwversion_range_lt
     * compare[0] == 0: <
     * compare[0] == 1: <=
     */
	prop = of_find_property(device, HWVERSION_OF_RANGE_LT, &length);
	if (prop) {
        if(2 == (length / sizeof(u32))) {
            of_property_read_u32_array(device, HWVERSION_OF_RANGE_LT, compare, 2);
            // when >/>= compare[1], not in range 
            if(firefly_hwversion > compare[1] || (compare[0] == 0 && firefly_hwversion == compare[1])) {
				printk("%s %s: hwversion should <%s 0x%08x!\n", __FUNCTION__, compatible, 
                        compare[0] == 0 ? "" : "=", compare[1]);
                return false;
			} else {
                cond_count++;
				printk("%s %s: hwversion <%s 0x%08x\n", __FUNCTION__, compatible, 
                        compare[0] == 0 ? "" : "=", compare[1]);
            }
        } else {
            printk("%s %s: parse '%s' error!\n", __FUNCTION__, compatible, HWVERSION_OF_RANGE_GT);
            return false;
        }

    }

    printk("%s %s: hwversion in range(%d conditions)\n", __FUNCTION__, compatible, cond_count);
    return true;
}

struct miscdevice mdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "firefly_hwversion",
    .mode = 0444,
};

static int firefly_hwversion_probe( struct platform_device *pdev){
    int ret;

	printk("firefly_hwversion: V%s\n", hwversion);
    if (misc_register(&mdev)) {
          dev_err(&pdev->dev, "create firefly hwversion misc fail.\r \n");
          return -EINVAL;
    }
    ret = device_create_file(mdev.this_device, &dev_attr_hwversion);
    if (ret) {
          dev_err(&pdev->dev, "read hwversion message fail.\r\n");
          ret = -EINVAL;
          goto error0;
    }
  
   return 0;

error0:
   misc_deregister(&mdev);
   return ret;
}

static int firefly_hwversion_remove( struct platform_device *pdev){
   device_remove_file(mdev.this_device, &dev_attr_hwversion);
   misc_deregister(&mdev);
   return 0;
}

static const struct of_device_id firefly_hwversion_match[] = {
    {.compatible = "firefly,hwversion"},
    {},
};

static struct platform_driver firefly_hwversion_driver = {
    .probe = firefly_hwversion_probe,
    .remove = firefly_hwversion_remove,
    .driver = {
        .name   = "hwversion",
        .owner  = THIS_MODULE,
        .of_match_table = firefly_hwversion_match,
    },
};

static int firefly_hwversion_init(void){
    return platform_driver_register(&firefly_hwversion_driver);

}subsys_initcall(firefly_hwversion_init);

static void firefly_hwversion_exit(void){
    platform_driver_unregister(&firefly_hwversion_driver);

}module_exit(firefly_hwversion_exit);

MODULE_AUTHOR("mojx <service@t-firefly.com>");
MODULE_DESCRIPTION("Firefly hwversion information");
MODULE_ALIAS("platform:firefly-hwversion");
MODULE_LICENSE("GPL");

