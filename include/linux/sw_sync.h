/*
 * include/linux/sw_sync.h
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_SW_SYNC_H
#define _LINUX_SW_SYNC_H

#ifdef CONFIG_SW_SYNC

#include <linux/sync_file.h>

struct sync_timeline;

/**
 * sw_sync_timeline_create() - creates a sync object
 * @name:	sync_timeline name
 *
 * Creates a new sync_timeline. Returns the sync_timeline object or NULL in
 * case of error.
 */
struct sync_timeline *sw_sync_timeline_create(const char *name);

/**
 * sw_sync_timeline_inc() - signal a status change on a sync_timeline
 * @obj:	sync_timeline to signal
 * @inc:	num to increment on timeline->value
 *
 * A sync implementation should call this any time one of it's fences
 * has signaled or has an error condition.
 */
void sw_sync_timeline_inc(struct sync_timeline *obj, unsigned int inc);

/**
 * sw_sync_timeline_put() - decrement timeline reference count
 * @obj:    sync_timeline to decrement reference count on
 *
 * A sync implementation should call this any time its contextual use of
 * a timeline has ended. If the caller is the last reference to the
 * timeline, @obj may be destroyed.
 */
void sw_sync_timeline_put(struct sync_timeline *obj);

/**
 * sw_sync_fence_create() - creates a sync fence
 * @parent:	fence's parent sync_timeline
 * @inc:	value of the fence
 *
 * Creates a new fence as a child of @parent. Returns the fence object
 * or NULL in case of error.
 */
struct fence *sw_sync_fence_create(struct sync_timeline *obj,
				   unsigned int value);

#endif /* CONFIG_SW_SYNC */

#endif /* _LINUX_SW_SYNC_H */
