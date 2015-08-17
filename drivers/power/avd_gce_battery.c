/*
 * Battery driver for Android Virtual Devices on Google Compute Engine (GCE).
 *
 * Copyright (C) 2015 Google, Inc.
 * Author: Keun Soo Yim <yim@google.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/vermagic.h>

#include <linux/power/avd_gce_battery.h>
#include <linux/power/avd_gce_battery_procfs.h>

#define AVDGCE_MODEL_NAME	"AVD battery"
#define AVDGCE_MANUFACTURER	"Google, Inc."
#define AVDGCE_AC_SUPPLY_NAME	"avd-ac"
#define AVDGCE_BATTERY_NAME	"avd-battery"

static int battery_status			= POWER_SUPPLY_STATUS_CHARGING;
static const char *battery_status_str		= "charging";

static int battery_health			= POWER_SUPPLY_HEALTH_GOOD;
static const char *battery_health_str		= "good";

static int battery_present			= 1;
static const char *battery_present_str		= "true";

static int battery_technology			= POWER_SUPPLY_TECHNOLOGY_LION;
static const char *battery_technology_str	= "LION";

int avdgce_battery_capacity			= AVDGCE_BATTERY_CAPACITY_DEFAULT;
static int battery_voltage			= 3300;

static bool avdgce_module_initialized;

static int avdgce_power_get_ac_property(struct power_supply *psy,
				      enum power_supply_property psp,
				      union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;  /* online */
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int avdgce_power_get_battery_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = AVDGCE_MODEL_NAME;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = AVDGCE_MANUFACTURER;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = battery_status;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = battery_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = battery_present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = battery_technology;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = avdgce_battery_capacity;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = 100;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		val->intval = 6840;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = 360;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = 26;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery_voltage;
		break;
	default:
		pr_info("%s: some properties deliberately report errors.\n",
			__func__);
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property avdgce_power_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property avdgce_power_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static char *avdgce_power_ac_supplied_to[] = {
	AVDGCE_BATTERY_NAME,
};

static struct power_supply avdgce_ac_supply = {
	.name = AVDGCE_AC_SUPPLY_NAME,
	.type = POWER_SUPPLY_TYPE_MAINS,
	.supplied_to = avdgce_power_ac_supplied_to,
	.num_supplicants = ARRAY_SIZE(avdgce_power_ac_supplied_to),
	.properties = avdgce_power_ac_props,
	.num_properties = ARRAY_SIZE(avdgce_power_ac_props),
	.get_property = avdgce_power_get_ac_property,
};

static struct power_supply avdgce_battery_supply = {
	.name = AVDGCE_BATTERY_NAME,
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = avdgce_power_battery_props,
	.num_properties = ARRAY_SIZE(avdgce_power_battery_props),
	.get_property = avdgce_power_get_battery_property,
};

static int __init avdgce_power_init(void)
{
	int ret = avdgce_power_procfs_init(THIS_MODULE);
	if (ret) {
		return ret;
	}

	ret = power_supply_register(NULL, &avdgce_ac_supply);
	if (ret) {
		pr_err("%s: registeration of %s failed\n", __func__,
			avdgce_ac_supply.name);
		return ret;
	}
	ret = power_supply_register(NULL, &avdgce_battery_supply);
	if (ret) {
		pr_err("%s: registeration of %s failed\n", __func__,
			avdgce_battery_supply.name);
		power_supply_unregister(&avdgce_ac_supply);
		return ret;
	}
	avdgce_module_initialized = true;
	return 0;
}

static void __exit avdgce_power_exit(void)
{
	power_supply_unregister(&avdgce_battery_supply);
	power_supply_unregister(&avdgce_ac_supply);
	avdgce_power_procfs_exit();
	avdgce_module_initialized = false;
}

module_init(avdgce_power_init);
module_exit(avdgce_power_exit);

static int param_get_battery_status(
	char *buffer, const struct kernel_param* kp)
{
	strcpy(buffer, battery_status_str);
	return strlen(buffer);
}

static int param_get_battery_health(
	char *buffer, const struct kernel_param* kp)
{
	strcpy(buffer, battery_health_str);
	return strlen(buffer);
}

static int param_get_battery_present(
	char *buffer, const struct kernel_param* kp)
{
	strcpy(buffer, battery_present_str);
	return strlen(buffer);
}

static int param_get_battery_technology(
	char *buffer, const struct kernel_param* kp)
{
	strcpy(buffer, battery_technology_str);
	return strlen(buffer);
}

#define param_get_battery_capacity param_get_int
#define param_get_battery_voltage param_get_int

static struct kernel_param_ops param_ops_battery_status = {
	.get = param_get_battery_status,
};

static struct kernel_param_ops param_ops_battery_present = {
	.get = param_get_battery_present,
};

static struct kernel_param_ops param_ops_battery_technology = {
	.get = param_get_battery_technology,
};

static struct kernel_param_ops param_ops_battery_health = {
	.get = param_get_battery_health,
};

static struct kernel_param_ops param_ops_battery_capacity = {
	.get = param_get_battery_capacity,
};

static struct kernel_param_ops param_ops_battery_voltage = {
	.get = param_get_battery_voltage,
};

#define param_check_battery_status(name, p) __param_check(name, p, void);
#define param_check_battery_present(name, p) __param_check(name, p, void);
#define param_check_battery_technology(name, p) __param_check(name, p, void);
#define param_check_battery_health(name, p) __param_check(name, p, void);
#define param_check_battery_capacity(name, p) __param_check(name, p, void);
#define param_check_battery_voltage(name, p) __param_check(name, p, void);


module_param(battery_status, battery_status, 0644);
MODULE_PARM_DESC(battery_status,
	"battery status <charging|discharging|not-charging|full>");

module_param(battery_present, battery_present, 0644);
MODULE_PARM_DESC(battery_present,
	"battery presence state <good|overheat|dead|overvoltage|failure>");

module_param(battery_technology, battery_technology, 0644);
MODULE_PARM_DESC(battery_technology,
	"battery technology <NiMH|LION|LIPO|LiFe|NiCd|LiMn>");

module_param(battery_health, battery_health, 0644);
MODULE_PARM_DESC(battery_health,
	"battery health state <good|overheat|dead|overvoltage|failure>");

module_param(avdgce_battery_capacity, battery_capacity, 0644);
MODULE_PARM_DESC(battery_capacity, "battery capacity (percentage)");

module_param(battery_voltage, battery_voltage, 0644);
MODULE_PARM_DESC(battery_voltage, "battery voltage (millivolts)");

MODULE_AUTHOR("Keun Soo Yim yim@google.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery driver for AVD on GCE");
