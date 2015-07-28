/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <odp/helper/linux.h>
#include <odp_internal.h>
#include <odp/thread.h>
#include <odp/init.h>
#include <odp/system_info.h>
#include <odp_debug_internal.h>

#include <rte_lcore.h>

static void *odp_run_start_routine(void *arg)
{
	odp_start_args_t *start_args = arg;

	/* ODP thread local init */
	if (odp_init_local()) {
		ODP_ERR("Local init failed\n");
		return NULL;
	}

	return start_args->start_routine(start_args->arg);
}


int odph_linux_pthread_create(odph_linux_pthread_t *thread_tbl,
			      const odp_cpumask_t *mask_in,
			      void *(*start_routine) (void *), void *arg)
{
	int i, num;
	int cpu;
	odp_cpumask_t mask;
	int ret;

	odp_cpumask_copy(&mask, mask_in);
	num = odp_cpumask_count(&mask);

	memset(thread_tbl, 0, num * sizeof(odph_linux_pthread_t));
	if (num < 1 || num > odp_cpu_count()) {
		ODP_ERR("Bad num\n");
		return 0;
	}

	cpu = odp_cpumask_first(&mask);
	for (i = 0; i < num; i++) {
		/* do not lock up current core */
		if ((unsigned)cpu == rte_get_master_lcore()) {
			cpu = odp_cpumask_next(&mask, cpu);
			continue;
		}

		thread_tbl[i].cpu = cpu;

		/* pthread affinity is not set here because, DPDK
		 * creates, initialises and sets the affinity for pthread
		 * part of rte_eal_init()
		 */

		thread_tbl[i].start_args = malloc(sizeof(odp_start_args_t));
		if (thread_tbl[i].start_args == NULL)
			ODP_ABORT("Malloc failed");

		thread_tbl[i].start_args->start_routine = start_routine;
		thread_tbl[i].start_args->arg           = arg;

		ret = rte_eal_remote_launch(
				(int(*)(void *))odp_run_start_routine,
				thread_tbl[i].start_args, cpu);
		if (ret != 0) {
			ODP_ERR("Failed to start thread on cpu #%d\n", cpu);
			free(thread_tbl[i].start_args);
			break;
		}

		cpu = odp_cpumask_next(&mask, cpu);
	}

	if (i != num)
		ODP_DBG("Run %d thread instead of %d\n", i, num);

	return i;
}


void odph_linux_pthread_join(odph_linux_pthread_t *thread_tbl, int num)
{
	uint32_t lcore_id;

	(void) thread_tbl;
	(void) num;

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		int ret = rte_eal_wait_lcore(lcore_id);
		free(thread_tbl[lcore_id].start_args);
		if (ret < 0)
			return;
	}
}

int odph_linux_process_fork_n(odph_linux_process_t *proc_tbl ODP_UNUSED,
			      const odp_cpumask_t *mask_in ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	ODP_ABORT("");
	return 0;
}

int odph_linux_process_wait_n(odph_linux_process_t *proc_tbl ODP_UNUSED,
			      int num ODP_UNUSED)
{
	ODP_UNIMPLEMENTED();
	ODP_ABORT("");
	return 0;
}
