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
#include <asm/uaccess.h>

#include <linux/power/avd_gce_battery.h>

#define PROCFS_MAX_SIZE 32
#define PROCFS_NAME "power_battery_config"
static struct proc_dir_entry* avdgce_procfile;

static int
avdgce_procfile_read(
	char *buffer,
	char **buffer_location,
	off_t offset,
	int buffer_length,
	int *eof,
	void *data)
{
	int ret = 0;
	pr_info("%s: (/proc/%s) called\n", __func__, PROCFS_NAME);
	if (offset == 0) {
		if (avdgce_battery_capacity >= 0 && avdgce_battery_capacity <= 100) {
			ret = snprintf(buffer, 0, "%d", avdgce_battery_capacity);
                        if (buffer_length >= ret) {
				ret = sprintf(buffer, "%d", avdgce_battery_capacity);
			} else {
				ret = 0;
			}
		} else {
			ret = snprintf(buffer, 0, "%d", AVDGCE_BATTERY_CAPACITY_DEFAULT);
                        if (buffer_length >= ret) {
				ret = sprintf(buffer, "%d", AVDGCE_BATTERY_CAPACITY_DEFAULT);
			} else {
				ret = 0;
			}
		}
	}
	return ret;
}

static int
avdgce_procfile_write(
	struct file *file,
	const char *buffer,
	unsigned long count,
	void *data)
{
	char kern_buffer[PROCFS_MAX_SIZE];
	if (count > PROCFS_MAX_SIZE ) {
		count = PROCFS_MAX_SIZE;
	}

	if ( copy_from_user(kern_buffer, buffer, count) ) {
		return -EFAULT;
	}

	int capacity = -1;
	int num_read = sscanf(kern_buffer, "%d", &capacity);
	if (num_read == 1 && capacity >= 0 && capacity <= 100) {
		avdgce_battery_capacity = capacity;
	} else {
		return -EFAULT;
	}
	return count;
}

int avdgce_power_procfs_init(struct module *owner)
{
	// create the /proc/power_battery_config file for dynamic configuration.
	avdgce_procfile = create_proc_entry(PROCFS_NAME, 0644, NULL);
	if (avdgce_procfile == NULL) {
		remove_proc_entry(PROCFS_NAME, NULL);
		pr_err("%s: could not initialize /proc/%s\n", __func__,
			PROCFS_NAME);
		return -ENOMEM;
	}

	avdgce_procfile->read_proc = avdgce_procfile_read;
	avdgce_procfile->write_proc = avdgce_procfile_write;
	avdgce_procfile->mode = S_IFREG | S_IRUGO;
	avdgce_procfile->uid = 0;
	avdgce_procfile->gid = 0;
	avdgce_procfile->size = 37;
	pr_info("/proc/%s created\n", PROCFS_NAME);
	return 0;
}

void avdgce_power_procfs_exit(void)
{
	remove_proc_entry(PROCFS_NAME, NULL);
	pr_err("/proc/%s removed\n", PROCFS_NAME);
}

