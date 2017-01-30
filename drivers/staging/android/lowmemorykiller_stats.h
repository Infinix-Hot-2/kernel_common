/*
 *  lowmemorykiller_stats interface
 *
 *  Copyright (C) 2017 Sony Mobile Communications Inc.
 *
 *  Author: Peter Enderborg <peter.enderborg@sonymobile.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

enum  lmk_kill_stats {
	LMK_SCAN = 1,
	LMK_KILL = 2,
	LMK_WASTE = 3,
	LMK_TIMEOUT = 4,
	LMK_COUNT = 5
};

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_STATS
void lmk_inc_stats(int key);
int __init init_procfs_lmk(void);
#else
static inline void lmk_inc_stats(int key) { return; };
static inline int __init init_procfs_lmk(void) { return 0; };
#endif
