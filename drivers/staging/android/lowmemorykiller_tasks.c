/*
 *  lowmemorykiller_tasks
 *
 *  Copyright (C) 2017 Sony Mobile Communications Inc.
 *
 *  Author: Peter Enderborg <peter.enderborg@sonymobile.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

/* this files contains help functions for handling tasks within the
 * lowmemorykiller. It track tasks that are in it's score range,
 * and it track tasks that signaled to be killed
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/slab.h>
#include <linux/oom_score_notifier.h>

#include "lowmemorykiller.h"
#include "lowmemorykiller_tasks.h"

static struct rb_root watch_tree = RB_ROOT;
struct list_head lmk_death_pending;
struct kmem_cache *lmk_dp_cache;
struct kmem_cache *lmk_task_cache;

/* We need a well defined order for our tree, score is the major order
 * and we use pid to get a unique order.
 * return -1 on smaller, 0 on equal and 1 on bigger
 */

enum {
	LMK_OFR_LESS = -1,
	LMK_OFR_EQUAL = 0,
	LMK_OFR_GREATER = 1
};

/* to protect lmk task storage data structures */
DEFINE_SPINLOCK(lmk_task_lock);
LIST_HEAD(lmk_death_pending);

int death_pending_len;

static inline int lmk_task_orderfunc(int lkey, int lpid, int rkey, int rpid)
{
	if (lkey > rkey)
		return LMK_OFR_GREATER;
	if (lkey < rkey)
		return LMK_OFR_LESS;
	if (lpid > rpid)
		return LMK_OFR_GREATER;
	if (lpid < rpid)
		return LMK_OFR_LESS;
	return LMK_OFR_EQUAL;
}

static inline int __lmk_task_insert(struct rb_root *root,
				    struct task_struct *tsk)
{
	struct rb_node **new = &root->rb_node, *parent = NULL;
	struct lmk_rb_watch *t;

	t = kmem_cache_alloc(lmk_task_cache, GFP_ATOMIC);
	t->key = tsk->signal->oom_score_adj;
	t->tsk = tsk;

	/* Figure out where to put new node */
	while (*new) {
		struct lmk_rb_watch *this = rb_entry(*new,
						     struct lmk_rb_watch,
						     rb_node);
		int result;

		result = lmk_task_orderfunc(t->key, t->tsk->pid,
					    this->key, this->tsk->pid);
		if (result == LMK_OFR_EQUAL) {
			lowmem_print(1, "Dupe key %d pid %d - key %d pid %d\n",
				     t->key, t->tsk->pid,
				     this->key, this->tsk->pid);
			WARN_ON(1);
			return 0;
		}
		parent = *new;
		if (result > 0)
			new = &((*new)->rb_left);
		else
			new = &((*new)->rb_right);
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&t->rb_node, parent, new);
	rb_insert_color(&t->rb_node, root);

	return 1;
}

static struct lmk_rb_watch *__lmk_task_search(struct rb_root *root,
					      struct task_struct *tsk,
					      int score)
{
	struct rb_node *node = root->rb_node;

	while (node) {
		struct lmk_rb_watch *data = rb_entry(node,
						     struct lmk_rb_watch,
						     rb_node);
		int result;

		result = lmk_task_orderfunc(data->key, data->tsk->pid,
					    score, tsk->pid);

		if (result < 0)
			node = node->rb_left;
		else if (result > 0)
			node = node->rb_right;
		else if (data->tsk == tsk)
			return data;
	}
	return NULL;
}

int __lmk_task_remove(struct task_struct *tsk,
		      int score)
{
	struct lmk_rb_watch *lrw;

	lrw = __lmk_task_search(&watch_tree, tsk, score);
	if (lrw) {
		rb_erase(&lrw->rb_node, &watch_tree);
		kmem_cache_free(lmk_task_cache, lrw);
		return 1;
	}

	return 0;
}

static void lmk_task_watch(struct task_struct *tsk, int old_oom_score_adj)
{
	if (thread_group_leader(tsk) &&
	    (tsk->signal->oom_score_adj >= LMK_SCORE_THRESHOLD ||
	     old_oom_score_adj >= LMK_SCORE_THRESHOLD) &&
	    !(tsk->flags & PF_KTHREAD)) {
		spin_lock(&lmk_task_lock);
		__lmk_task_remove(tsk, old_oom_score_adj);
		if (tsk->signal->oom_score_adj >= LMK_SCORE_THRESHOLD)
			if (!test_tsk_thread_flag(tsk, TIF_MEMDIE))
				__lmk_task_insert(&watch_tree, tsk);
		spin_unlock(&lmk_task_lock);
	}
}

static void lmk_task_free(struct task_struct *tsk)
{
	if (thread_group_leader(tsk) &&
	    !(tsk->flags & PF_KTHREAD)) {
		struct lmk_death_pending_entry *dp_iterator;
		int clear = 1;

		spin_lock(&lmk_task_lock);
		if (__lmk_task_remove(tsk, tsk->signal->oom_score_adj))
			clear = 0;

		/* check our kill queue */
		list_for_each_entry(dp_iterator,
				    &lmk_death_pending, lmk_dp_list) {
			if (dp_iterator->tsk == tsk) {
				list_del(&dp_iterator->lmk_dp_list);
				kmem_cache_free(lmk_dp_cache, dp_iterator);
				death_pending_len--;
				clear = 0;
				break;
			}
		}
		spin_unlock(&lmk_task_lock);
		if (clear) {
			lowmem_print(2, "Pid not in list %d %d\n",
				     tsk->pid, tsk->signal->oom_score_adj);
		}
	}
}

static int lmk_oom_score_notifier(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct oom_score_notifier_struct *osns = data;

	switch (action) {
	case OSN_NEW:
		lmk_task_watch(osns->tsk, LMK_SCORE_THRESHOLD - 1);
		break;
	case OSN_FREE:
		lmk_task_free(osns->tsk);
		break;
	case OSN_UPDATE:
		lmk_task_watch(osns->tsk, osns->old_score);
		break;
	}
	return 0;
}

int __lmk_death_pending_add(struct lmk_death_pending_entry *lwp)
{
	list_add(&lwp->lmk_dp_list, &lmk_death_pending);
	death_pending_len++;
	return 0;
}

struct lmk_rb_watch *__lmk_first(void)
{
	return rb_entry(rb_first(&watch_tree), struct lmk_rb_watch, rb_node);
}

struct notifier_block lmk_oom_score_nb = {
	.notifier_call = lmk_oom_score_notifier,
};
