/*
 * Basic Linux Kernel module using GPIO interrupts.
 *
 * Author:
 * 	Stefan Wendler (devnull@kaltpost.de)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h> 

/* Base for update time is 10 sec. */
#define SCAN_DELAY	(10 * HZ)

/* Timeout until to wait for sensor to start transmitting data */
#define MEASURE_START_TIMEOUT	25000 

/* Timeout until to wait for sensor to finish data transmission */
#define MEASURE_XMIT_TIMEOUT 	100000	

/* List for timer function */
static struct timer_list measure_timer;

/* GPIO used to interface with SRF05 ultrasonic range finder */
static int srf05_gpio = 20;

/* Divisor used to calculate cm from raw sensor value */
static int srf05_cmdiv = 450;

/* Devisor used to calculate update freq. (f = 10sec. / updiv) */
static int srf05_updiv = 20;

/* Last distance measured (raw value) */
static unsigned long distance = 0;

/* Sensor status: 0 - OPERATIONAL, 1 - ERROR */
static int status = 0;

/* Module param for gpio */
module_param(srf05_gpio, int, 0);
MODULE_PARM_DESC(srf05_gpio, "Number of GPIO to which data line auf SRF05 is connected (default 20).");

/* Module param for cm divisor */
module_param(srf05_cmdiv, int, 0);
MODULE_PARM_DESC(srf05_cmdiv, "Divisor used to calculate cm from raw sensor value.");

/* Module param for update freq. divisor */
module_param(srf05_updiv, int, 0);
MODULE_PARM_DESC(srf05_updiv, "Divisor used to calculate update freq (devides 10 sec.).");

/*
 * Return current distance (raw) via sysfs
 */
static ssize_t sysfs_distance_raw_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", distance);
}

static struct kobj_attribute distance_raw_attribute = __ATTR(distance_raw, 0444, sysfs_distance_raw_show, NULL);

/*
 * Return current distance (cm) via sysfs
 */
static ssize_t sysfs_distance_cm_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", distance / srf05_cmdiv);
}

static struct kobj_attribute distance_cm_attribute  = __ATTR(distance_cm , 0444, sysfs_distance_cm_show, NULL);

/*
 * Return current sensor status via sysfs
 */
static ssize_t sysfs_status_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", (status == 1 ? "ERROR" : "OPERATIONAL"));
}

static struct kobj_attribute status_attribute  = __ATTR(status , 0444, sysfs_status_show, NULL);

/* List of all attributes exported to sysfs */
static struct attribute *attrs[] = {
	&distance_raw_attribute.attr,
	&distance_cm_attribute.attr,
	&status_attribute.attr,
	NULL,  
};

/* Attributes for sysfs in a group */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

/* Kernel object for sysfs */
static struct kobject *srf05_kobj;

/*
 * Measure distance
 */
static void measure(void) 
{
	unsigned long t = 0;

	gpio_direction_output(srf05_gpio, 1);	

	udelay(20);
	
	gpio_set_value(srf05_gpio, 0);

	gpio_direction_input(srf05_gpio);	

	while(gpio_get_value(srf05_gpio) == 0 && t < MEASURE_START_TIMEOUT) t++;

	if(t >= MEASURE_START_TIMEOUT) {
		status = 1;
		return;
	}
	
	t = 0;

	while(gpio_get_value(srf05_gpio) == 1 && t < MEASURE_XMIT_TIMEOUT) t++;

	if(t >= MEASURE_XMIT_TIMEOUT) {
		status = 1;
		return;
	}

	status 		= 0;
	distance 	= t;
}

/*
 * Timer function called periodically
 */
static void measure_timer_func(unsigned long data)
{
	measure(); 

	// printk(KERN_DEBUG "Current raw distance: %lu\n", distance);

	/* schedule next execution */
	measure_timer.expires = jiffies + SCAN_DELAY / srf05_updiv;
	add_timer(&measure_timer);
}

/*
 * Module init function
 */
static int __init srf05_init(void)
{
	int ret = 0;

	printk(KERN_INFO "srf05: init");
	printk(KERN_INFO "srf05: using gpio #%d for data aquisition\n", srf05_gpio);
	printk(KERN_INFO "srf05: using a divisor of %d for calculating cm from raw\n", srf05_cmdiv);
	printk(KERN_INFO "srf05: using a divisor of %d for calculating update freq.\n", srf05_updiv);

    srf05_kobj = kobject_create_and_add("srf05", kernel_kobj);

    if(!srf05_kobj) {
        return -ENOMEM;
	}

    ret = sysfs_create_group(srf05_kobj, &attr_group);

    if(ret) {
		// remove kobj
        kobject_put(srf05_kobj);
		return ret;
	}

	// register SRF05 gpio
	ret = gpio_request_one(srf05_gpio, GPIOF_IN, "srf05#inout");

	if(ret) {
		printk(KERN_ERR "Unable to request GPIO for SRF05: %d\n", ret);

		// remove kobj
        kobject_put(srf05_kobj);
		return ret;
	}

	/* init timer, add timer function */
	init_timer(&measure_timer);

	measure_timer.function = measure_timer_func;
	measure_timer.expires = jiffies + SCAN_DELAY / srf05_updiv; 
	add_timer(&measure_timer);

	return ret;	
}

/*
 * Module exit function
 */
static void __exit srf05_exit(void)
{
	printk(KERN_INFO "srf05: exit\n");

	// remove kobj
    kobject_put(srf05_kobj);

	// remove timer
	del_timer_sync(&measure_timer);

	// unregister
	gpio_free(srf05_gpio);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefan Wendler");
MODULE_DESCRIPTION("SRF05 utrasonic range finder");

module_init(srf05_init);
module_exit(srf05_exit);
