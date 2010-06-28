/*
 * Copyright (C) 2010 Motorola, Inc.
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

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>

struct bq24617_data {
	struct work_struct work;
	int stat1_irq;
	int stat2_irq;
	struct wake_lock wake_lock;
	struct power_supply ac;
	int ac_online;
};

static int bq24617_stat2_value;

static char *bq24617_supply_list[] = {
	"battery",
};

static enum power_supply_property bq24617_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

int is_ac_charge_complete(void)
{
	return bq24617_stat2_value;
}

static int power_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct bq24617_data *bq_data =
		container_of(psy, struct bq24617_data, ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	val->intval = bq_data->ac_online;
	return 0;
}

static irqreturn_t bq24617_isr(int irq, void *data)
{
	struct bq24617_data *bq_data = data;

	wake_lock(&bq_data->wake_lock);
	schedule_work(&bq_data->work);

	return IRQ_HANDLED;
}

static void bq24617_work(struct work_struct *work)
{
	struct bq24617_data *bq_data =
		container_of(work, struct bq24617_data, work);
	int stat1;

	/* STAT1 indicates charging, STAT2 indicates charge complete */
	stat1 = gpio_get_value(irq_to_gpio(bq_data->stat1_irq));
	bq24617_stat2_value = gpio_get_value(irq_to_gpio(bq_data->stat2_irq));

	if (!stat1 || bq24617_stat2_value)
		bq_data->ac_online = 1;
	else
		bq_data->ac_online = 0;

	pr_info("%s: ac_online=%d\n", __func__, bq_data->ac_online);

	power_supply_changed(&bq_data->ac);
	wake_unlock(&bq_data->wake_lock);
}

static int bq24617_probe(struct platform_device *pdev)
{
	struct bq24617_data *bq_data;
	int retval;
	unsigned int flags;

	bq_data = kzalloc(sizeof(*bq_data), GFP_KERNEL);
	if (bq_data == NULL)
		return -ENOMEM;

	INIT_WORK(&bq_data->work, bq24617_work);
	wake_lock_init(&bq_data->wake_lock, WAKE_LOCK_SUSPEND, "bq24617");
	platform_set_drvdata(pdev, bq_data);

	bq_data->stat1_irq = platform_get_irq_byname(pdev, "stat1");
	bq_data->stat2_irq = platform_get_irq_byname(pdev, "stat2");
	if ((bq_data->stat1_irq < 0) || (bq_data->stat2_irq < 0)) {
		dev_err(&pdev->dev, "Resources not set properly\n");
		retval = -ENODEV;
		goto free_mem;
	}

	flags = IRQF_DISABLED | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	retval = request_irq(bq_data->stat1_irq, bq24617_isr, flags,
			     "bq24617_stat1", bq_data);
	if (retval) {
		dev_err(&pdev->dev, "Failed requesting STAT1 IRQ\n");
		goto free_mem;
	}

	retval = request_irq(bq_data->stat2_irq, bq24617_isr, flags,
			     "bq24617_stat2", bq_data);
	if (retval) {
		dev_err(&pdev->dev, "Failed requesting STAT2 IRQ\n");
		goto free_stat1;
	}

	enable_irq_wake(bq_data->stat1_irq);
	enable_irq_wake(bq_data->stat2_irq);

	bq_data->ac.name = "ac";
	bq_data->ac.type = POWER_SUPPLY_TYPE_MAINS;
	bq_data->ac.supplied_to = bq24617_supply_list;
	bq_data->ac.num_supplicants = ARRAY_SIZE(bq24617_supply_list);
	bq_data->ac.properties = bq24617_power_props;
	bq_data->ac.num_properties = ARRAY_SIZE(bq24617_power_props);
	bq_data->ac.get_property = power_get_property;

	retval = power_supply_register(&pdev->dev, &bq_data->ac);
	if (retval) {
		dev_err(&pdev->dev, "Failed registering power supply\n");
		goto free_stat2;
	}

	bq24617_work(&bq_data->work);

	return 0;

free_stat2:
	free_irq(bq_data->stat2_irq, bq_data);
free_stat1:
	free_irq(bq_data->stat1_irq, bq_data);
free_mem:
	wake_lock_destroy(&bq_data->wake_lock);
	kfree(bq_data);

	return retval;
}

static int bq24617_remove(struct platform_device *pdev)
{
	struct bq24617_data *bq_data = platform_get_drvdata(pdev);

	cancel_work_sync(&bq_data->work);
	power_supply_unregister(&bq_data->ac);

	free_irq(bq_data->stat1_irq, bq_data);
	free_irq(bq_data->stat2_irq, bq_data);

	wake_lock_destroy(&bq_data->wake_lock);
	kfree(bq_data);

	return 0;
}

static struct platform_driver bq24617_pdrv = {
	.driver = {
		.name = "bq24617",
	},
	.probe = bq24617_probe,
	.remove = bq24617_remove,
};

static int __init bq24617_init(void)
{
	return platform_driver_register(&bq24617_pdrv);
}

static void __exit bq24617_exit(void)
{
	platform_driver_unregister(&bq24617_pdrv);
}

module_init(bq24617_init);
module_exit(bq24617_exit);

MODULE_ALIAS("platform:bq24617_charger");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola");
MODULE_DESCRIPTION("bq24617 charger driver");
