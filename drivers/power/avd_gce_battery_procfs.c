/*
 * Battery driver's procfs manager for AVDs on GCE.
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
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include <linux/power/avd_gce_battery.h>

#define PROCFS_MAX_SIZE 32
#define PROCFS_NAME "power_battery_config"
static struct proc_dir_entry* avdgce_procfile;

static void*
avdgce_power_procfile_seq_start(
	struct seq_file *s, loff_t *pos)
{
	if (*pos == 0) {
		if (avdgce_battery_capacity < 0 || avdgce_battery_capacity > 100) {
			avdgce_battery_capacity = AVDGCE_BATTERY_CAPACITY_DEFAULT;
		}
		return &avdgce_battery_capacity;
	}
	else {
		*pos = 0;
		return NULL;
	}
}

static void*
avdgce_power_procfile_seq_next(
	struct seq_file *s, void *v, loff_t *pos)
{
	unsigned long *tmp_v = (unsigned long *)v;
	(*tmp_v)++;
	(*pos)++;
	return NULL;
}

static void
avdgce_power_procfile_seq_stop(
	struct seq_file *s, void *v)
{
	/* no op */
}

static int
avdgce_power_procfile_seq_show(
	struct seq_file *s, void *v)
{
	loff_t *spos = (loff_t *) v;

	pr_info("%s: (/proc/%s) called\n", __func__, PROCFS_NAME);
	seq_printf(s, "%lld", *spos);
	return 0;
}

static ssize_t avdgce_procfile_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *data)
{
	char kern_buffer[PROCFS_MAX_SIZE];
	int capacity = -1;
	int num_read;

	if (count > PROCFS_MAX_SIZE) {
		count = PROCFS_MAX_SIZE;
	}

	if (copy_from_user(kern_buffer, buffer, count)) {
		return -EFAULT;
	}

	num_read = sscanf(kern_buffer, "%d", &capacity);
	if (num_read == 1 && capacity >= 0 && capacity <= 100) {
		avdgce_battery_capacity = capacity;
	} else {
		return -EFAULT;
	}
	return count;
}

static struct seq_operations avdgce_power_procfile_sops = {
	.start = avdgce_power_procfile_seq_start,
	.next = avdgce_power_procfile_seq_next,
	.stop = avdgce_power_procfile_seq_stop,
	.show = avdgce_power_procfile_seq_show
};

static int avdgce_procfile_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &avdgce_power_procfile_sops);
};

static const struct file_operations avdgce_power_procfile_fops = {
	.open = avdgce_procfile_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.write = avdgce_procfile_write,
};

int avdgce_power_procfs_init(struct module *owner)
{
	// create the /proc/power_battery_config file for dynamic configuration.
	avdgce_procfile = proc_create(
		PROCFS_NAME, 0, NULL, &avdgce_power_procfile_fops);
	if (!avdgce_procfile) {
		remove_proc_entry(PROCFS_NAME, NULL);
		pr_err("%s: could not initialize /proc/%s\n", __func__,
			PROCFS_NAME);
		return -ENOMEM;
	}

	pr_info("/proc/%s created\n", PROCFS_NAME);
	return 0;
}

void avdgce_power_procfs_exit(void)
{
	remove_proc_entry(PROCFS_NAME, NULL);
	pr_err("/proc/%s removed\n", PROCFS_NAME);
}

