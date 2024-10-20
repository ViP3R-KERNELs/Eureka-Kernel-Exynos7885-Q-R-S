/* See include/linux/lglock.h for description */
#include <linux/module.h>
#include <linux/lglock.h>
#include <linux/cpu.h>
#include <linux/string.h>

#ifndef CONFIG_PREEMPT_RT_FULL
# define lg_lock_ptr		arch_spinlock_t
# define lg_do_lock(l)		arch_spin_lock(l)
# define lg_do_unlock(l)	arch_spin_unlock(l)
#else
# define lg_lock_ptr		struct rt_mutex
# define lg_do_lock(l)		__rt_spin_lock__no_mg(l)
# define lg_do_unlock(l)	__rt_spin_unlock(l)
#endif
/*
 * Note there is no uninit, so lglocks cannot be defined in
 * modules (but it's fine to use them from there)
 * Could be added though, just undo lg_lock_init
 */

void lg_lock_init(struct lglock *lg, char *name)
{
#ifdef CONFIG_PREEMPT_RT_FULL
	int i;

	for_each_possible_cpu(i) {
		struct rt_mutex *lock = per_cpu_ptr(lg->lock, i);

		rt_mutex_init(lock);
	}
#endif
	LOCKDEP_INIT_MAP(&lg->lock_dep_map, name, &lg->lock_key, 0);
}
EXPORT_SYMBOL(lg_lock_init);

void lg_local_lock(struct lglock *lg)
{
	lg_lock_ptr *lock;

	migrate_disable();
	lock_acquire_shared(&lg->lock_dep_map, 0, 0, NULL, _RET_IP_);
	lock = this_cpu_ptr(lg->lock);
	lg_do_lock(lock);
}
EXPORT_SYMBOL(lg_local_lock);

void lg_local_unlock(struct lglock *lg)
{
	lg_lock_ptr *lock;

	lock_release(&lg->lock_dep_map, 1, _RET_IP_);
	lock = this_cpu_ptr(lg->lock);
	lg_do_unlock(lock);
	migrate_enable();
}
EXPORT_SYMBOL(lg_local_unlock);

void lg_local_lock_cpu(struct lglock *lg, int cpu)
{
	lg_lock_ptr *lock;

	preempt_disable_nort();
	lock_acquire_shared(&lg->lock_dep_map, 0, 0, NULL, _RET_IP_);
	lock = per_cpu_ptr(lg->lock, cpu);
	lg_do_lock(lock);
}
EXPORT_SYMBOL(lg_local_lock_cpu);

void lg_local_unlock_cpu(struct lglock *lg, int cpu)
{
	lg_lock_ptr *lock;

	lock_release(&lg->lock_dep_map, 1, _RET_IP_);
	lock = per_cpu_ptr(lg->lock, cpu);
	lg_do_unlock(lock);
	preempt_enable_nort();
}
EXPORT_SYMBOL(lg_local_unlock_cpu);

void lg_double_lock(struct lglock *lg, int cpu1, int cpu2)
{
	BUG_ON(cpu1 == cpu2);

	/* lock in cpu order, just like lg_global_lock */
	if (cpu2 < cpu1)
		swap(cpu1, cpu2);

	preempt_disable_nort();
	lock_acquire_shared(&lg->lock_dep_map, 0, 0, NULL, _RET_IP_);
	lg_do_lock(per_cpu_ptr(lg->lock, cpu1));
	lg_do_lock(per_cpu_ptr(lg->lock, cpu2));
}

void lg_double_unlock(struct lglock *lg, int cpu1, int cpu2)
{
	lock_release(&lg->lock_dep_map, 1, _RET_IP_);
	lg_do_unlock(per_cpu_ptr(lg->lock, cpu1));
	lg_do_unlock(per_cpu_ptr(lg->lock, cpu2));
	preempt_enable_nort();
}

void lg_global_lock(struct lglock *lg)
{
	int i;

	preempt_disable_nort();
	lock_acquire_exclusive(&lg->lock_dep_map, 0, 0, NULL, _RET_IP_);
	for_each_possible_cpu(i) {
		lg_lock_ptr *lock;
		lock = per_cpu_ptr(lg->lock, i);
		lg_do_lock(lock);
	}
}
EXPORT_SYMBOL(lg_global_lock);

void lg_global_unlock(struct lglock *lg)
{
	int i;

	lock_release(&lg->lock_dep_map, 1, _RET_IP_);
	for_each_possible_cpu(i) {
		lg_lock_ptr *lock;
		lock = per_cpu_ptr(lg->lock, i);
		lg_do_unlock(lock);
	}
	preempt_enable_nort();
}
EXPORT_SYMBOL(lg_global_unlock);
