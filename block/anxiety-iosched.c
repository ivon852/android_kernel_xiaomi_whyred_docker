/*
 * Copyright (c) 2019, Tyler Nijmeh <tylernij@gmail.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

/* Default tunable values */
#define	DEFAULT_MAX_WRITES_STARVED	(4) /* Max times reads can starve a write */

struct anxiety_data {
	struct list_head queue[2];
	uint16_t writes_starved;

	/* Tunables */
	uint8_t max_writes_starved;
};

static void anxiety_merged_requests(struct request_queue *q, struct request *rq, struct request *next)
{
	rq_fifo_clear(next);
}

static inline struct request *anxiety_choose_request(struct anxiety_data *adata)
{
	/* Prioritize reads unless writes are exceedingly starved */
	bool starved = adata->writes_starved > adata->max_writes_starved;

	/* Handle a read request */
	if (!starved && !list_empty(&adata->queue[READ])) {
		adata->writes_starved++;
		return rq_entry_fifo(adata->queue[READ].next);
	}

	/* Handle a write request */
	if (!list_empty(&adata->queue[WRITE])) {
		adata->writes_starved = 0;
		return rq_entry_fifo(adata->queue[WRITE].next);
	}

	/* If there are no requests, then there is nothing to starve */
	adata->writes_starved = 0;
	return NULL;
}

static int anxiety_dispatch(struct request_queue *q, int force)
{
	struct request *rq = anxiety_choose_request(q->elevator->elevator_data);

	if (!rq)
		return 0;

	rq_fifo_clear(rq);
	elv_dispatch_add_tail(rq->q, rq);

	return 1;
}

static void anxiety_add_request(struct request_queue *q, struct request *rq)
{
	const uint8_t dir = rq_is_sync(rq);
	struct anxiety_data *adata = q->elevator->elevator_data;

	list_add_tail(&rq->queuelist, &adata->queue[dir]);
}

static int anxiety_init_queue(struct request_queue *q, struct elevator_type *elv)
{
	struct anxiety_data *adata;
	struct elevator_queue *eq = elevator_alloc(q, elv);

	if (!eq)
		return -ENOMEM;

	/* Allocate the data */
	adata = kmalloc_node(sizeof(*adata), GFP_KERNEL, q->node);
	if (!adata) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}

	/* Set the elevator data */
	eq->elevator_data = adata;

	/* Initialize */
	INIT_LIST_HEAD(&adata->queue[READ]);
	INIT_LIST_HEAD(&adata->queue[WRITE]);
	adata->writes_starved = 0;
	adata->max_writes_starved = DEFAULT_MAX_WRITES_STARVED;

	/* Set elevator to Anxiety */
	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

	return 0;
}

/* Sysfs access */
static ssize_t anxiety_max_writes_starved_show(struct elevator_queue *e, char *page)
{
	struct anxiety_data *adata = e->elevator_data;

	return snprintf(page, PAGE_SIZE, "%u\n", adata->max_writes_starved);
}

static ssize_t anxiety_max_writes_starved_store(struct elevator_queue *e, const char *page, size_t count)
{
	struct anxiety_data *adata = e->elevator_data;
	int ret;

	ret = kstrtou8(page, 0, &adata->max_writes_starved);
	if (ret < 0)
		return ret;

	return count;
}

static struct elv_fs_entry anxiety_attrs[] = {
	__ATTR(max_writes_starved, 0644, anxiety_max_writes_starved_show, anxiety_max_writes_starved_store),
	__ATTR_NULL
};

static struct elevator_type elevator_anxiety = {
	.ops = {
		.elevator_merge_req_fn	= anxiety_merged_requests,
		.elevator_dispatch_fn	= anxiety_dispatch,
		.elevator_add_req_fn	= anxiety_add_request,
		.elevator_former_req_fn	= elv_rb_former_request,
		.elevator_latter_req_fn	= elv_rb_latter_request,
		.elevator_init_fn	= anxiety_init_queue,
	},
	.elevator_name = "anxiety",
	.elevator_attrs = anxiety_attrs,
	.elevator_owner = THIS_MODULE,
};

static int __init anxiety_init(void)
{
	return elv_register(&elevator_anxiety);
}

static void __exit anxiety_exit(void)
{
	elv_unregister(&elevator_anxiety);
}

module_init(anxiety_init);
module_exit(anxiety_exit);

MODULE_AUTHOR("Tyler Nijmeh");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Anxiety IO scheduler");
