// SPDX-License-Identifier: GPL-2.0
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched/stat.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/irqnr.h>
#include <linux/sched/cputime.h>
#include <linux/tick.h>

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

extern int FALCON_CPUS[40];
extern int NR_FALCON_CPUS;
extern int CPUSTAT_INTERVAL;
extern int FALCON_LOAD_THRESHOLD;
extern int FALCON_LOAD_DIFF;
extern int FALCON_BALANCE_INTERVAL;

//grosplit
extern int GROSPLIT_CPUS[40];
extern int NR_GROSPLIT_CPUS;
//end

#ifdef arch_idle_time

static u64 get_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle;

	idle = kcs->cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static u64 get_iowait_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 iowait;

	iowait = kcs->cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}

#else

static u64 get_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle, idle_usecs = -1ULL;

	if (cpu_online(cpu))
		idle_usecs = get_cpu_idle_time_us(cpu, NULL);

	if (idle_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcs->cpustat[CPUTIME_IDLE];
	else
		idle = idle_usecs * NSEC_PER_USEC;

	return idle;
}

static u64 get_iowait_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 iowait, iowait_usecs = -1ULL;

	if (cpu_online(cpu))
		iowait_usecs = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcs->cpustat[CPUTIME_IOWAIT];
	else
		iowait = iowait_usecs * NSEC_PER_USEC;

	return iowait;
}

#endif

static void show_irq_gap(struct seq_file *p, unsigned int gap)
{
	static const char zeros[] = " 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0";

	while (gap > 0) {
		unsigned int inc;

		inc = min_t(unsigned int, gap, ARRAY_SIZE(zeros) / 2);
		seq_write(p, zeros, 2 * inc);
		gap -= inc;
	}
}

static void show_all_irqs(struct seq_file *p)
{
	unsigned int i, next = 0;

	for_each_active_irq(i) {
		show_irq_gap(p, i - next);
		seq_put_decimal_ull(p, " ", kstat_irqs_usr(i));
		next = i + 1;
	}
	show_irq_gap(p, nr_irqs - next);
}

static int show_stat(struct seq_file *p, void *v)
{
	int i, j;
	u64 user, nice, system, idle, iowait, irq, softirq, steal;
	u64 guest, guest_nice;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};
	struct timespec64 boottime;

	user = nice = system = idle = iowait =
		irq = softirq = steal = 0;
	guest = guest_nice = 0;
	getboottime64(&boottime);

	for_each_possible_cpu(i) {
		struct kernel_cpustat *kcs = &kcpustat_cpu(i);

		user += kcs->cpustat[CPUTIME_USER];
		nice += kcs->cpustat[CPUTIME_NICE];
		system += kcs->cpustat[CPUTIME_SYSTEM];
		idle += get_idle_time(kcs, i);
		iowait += get_iowait_time(kcs, i);
		irq += kcs->cpustat[CPUTIME_IRQ];
		softirq += kcs->cpustat[CPUTIME_SOFTIRQ];
		steal += kcs->cpustat[CPUTIME_STEAL];
		guest += kcs->cpustat[CPUTIME_GUEST];
		guest_nice += kcs->cpustat[CPUTIME_GUEST_NICE];
		sum += kstat_cpu_irqs_sum(i);
		sum += arch_irq_stat_cpu(i);

		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);

			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();

	seq_put_decimal_ull(p, "cpu  ", nsec_to_clock_t(user));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(nice));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(system));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(idle));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(iowait));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(irq));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(softirq));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(steal));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(guest));
	seq_put_decimal_ull(p, " ", nsec_to_clock_t(guest_nice));
	seq_putc(p, '\n');

	for_each_online_cpu(i) {
		struct kernel_cpustat *kcs = &kcpustat_cpu(i);

		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user = kcs->cpustat[CPUTIME_USER];
		nice = kcs->cpustat[CPUTIME_NICE];
		system = kcs->cpustat[CPUTIME_SYSTEM];
		idle = get_idle_time(kcs, i);
		iowait = get_iowait_time(kcs, i);
		irq = kcs->cpustat[CPUTIME_IRQ];
		softirq = kcs->cpustat[CPUTIME_SOFTIRQ];
		steal = kcs->cpustat[CPUTIME_STEAL];
		guest = kcs->cpustat[CPUTIME_GUEST];
		guest_nice = kcs->cpustat[CPUTIME_GUEST_NICE];
		seq_printf(p, "cpu%d", i);
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(user));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(nice));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(system));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(idle));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(iowait));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(irq));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(softirq));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(steal));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(guest));
		seq_put_decimal_ull(p, " ", nsec_to_clock_t(guest_nice));
		seq_putc(p, '\n');
	}
	seq_put_decimal_ull(p, "intr ", (unsigned long long)sum);

	show_all_irqs(p);

	seq_printf(p,
		"\nctxt %llu\n"
		"btime %llu\n"
		"processes %lu\n"
		"procs_running %lu\n"
		"procs_blocked %lu\n",
		nr_context_switches(),
		(unsigned long long)boottime.tv_sec,
		total_forks,
		nr_running(),
		nr_iowait());

	seq_put_decimal_ull(p, "softirq ", (unsigned long long)sum_softirq);

	for (i = 0; i < NR_SOFTIRQS; i++)
		seq_put_decimal_ull(p, " ", per_softirq_sums[i]);
	seq_putc(p, '\n');

	return 0;
}

static int stat_open(struct inode *inode, struct file *file)
{
	unsigned int size = 1024 + 128 * num_online_cpus();

	/* minimum size to display an interrupt count : 2 bytes */
	size += 2 * nr_irqs;
	return single_open_size(file, show_stat, NULL, size);
}

static const struct file_operations proc_stat_operations = {
	.open		= stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

//grosplit
static int grosplit_cpus_show(struct seq_file *f, void *v)
{
        u64 grosplit_cpu_map = 0;
        int i;

        for (i = 0; i < NR_GROSPLIT_CPUS; i++) {
                grosplit_cpu_map |= (1ull << GROSPLIT_CPUS[i]);
        }
        seq_printf(f, "%llx\n", grosplit_cpu_map);
        return 0;
}

static int grosplit_cpus_open(struct inode *inode, struct file *file)
{
        return single_open(file, grosplit_cpus_show, NULL);
}

static ssize_t grosplit_cpus_write(struct file *file, const char __user *ubuf,
                                 size_t size, loff_t *pos)
{
        char buf[101];
        int len, i;
        u64 grosplit_cpu_map;

        if (*pos > 0 || size > 100)
                return -EFAULT;
        if (copy_from_user(buf, ubuf, size))
                return -EFAULT;

        if (sscanf(buf, "%llx", &grosplit_cpu_map) != 1)
                return -EFAULT;

        len = strlen(buf);
        *pos = len;

        NR_GROSPLIT_CPUS = 0;
        for_each_online_cpu(i) {
                // check the bit
                if (grosplit_cpu_map & 1) {
                        GROSPLIT_CPUS[NR_GROSPLIT_CPUS] = i;
                        NR_GROSPLIT_CPUS++;
                }
                grosplit_cpu_map >>= 1;
        }

        return len;
}

static const struct file_operations grosplit_cpus_ops = {
        .open              = grosplit_cpus_open,
        .read              = seq_read,
        .write             = grosplit_cpus_write,
        .llseek             = seq_lseek,
        .release           = single_release,
};
//end

/* FALCON_CPUS */

static int falcon_cpus_show(struct seq_file *f, void *v)
{
	u64 falcon_cpu_map = 0;
	int i;

	for (i = 0; i < NR_FALCON_CPUS; i++) {
		falcon_cpu_map |= (1ull << FALCON_CPUS[i]);
	}
	seq_printf(f, "%llx\n", falcon_cpu_map);
	return 0;
}

static int falcon_cpus_open(struct inode *inode, struct file *file)
{
	return single_open(file, falcon_cpus_show, NULL);
}

static ssize_t falcon_cpus_write(struct file *file, const char __user *ubuf,
				 size_t size, loff_t *pos)
{
	char buf[101];
	int len, i;
	u64 falcon_cpu_map;

	// copy user's input to kernel buffer
	if (*pos > 0 || size > 100)
		return -EFAULT;
	if (copy_from_user(buf, ubuf, size))
		return -EFAULT;

	// read the cpu map
	if (sscanf(buf, "%llx", &falcon_cpu_map) != 1)
		return -EFAULT;

	len = strlen(buf);
	*pos = len;

	// fill the FALCON_CPUS array
	NR_FALCON_CPUS = 0;
	for_each_online_cpu(i) {
		// check the bit
		if (falcon_cpu_map & 1) {
			FALCON_CPUS[NR_FALCON_CPUS] = i;
			NR_FALCON_CPUS++;
		}
		falcon_cpu_map >>= 1;
	}

	return len;
}

static const struct file_operations falcon_cpus_ops = {
	.open		= falcon_cpus_open,
	.read		= seq_read,
	.write		= falcon_cpus_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* CPUSTAT_INTERVAL */

static int cpustat_show(struct seq_file *f, void *v)
{
	seq_printf(f, "%d\n", CPUSTAT_INTERVAL);
	return 0;
}

static int cpustat_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpustat_show, NULL);
}

static ssize_t cpustat_write(struct file *file, const char __user *ubuf,
			     size_t size, loff_t *pos)
{
	char buf[101];
	int len;

	if (*pos > 0 || size > 100)
		return -EFAULT;
	if (copy_from_user(buf, ubuf, size))
		return -EFAULT;
	if (sscanf(buf, "%d", &CPUSTAT_INTERVAL) != 1)
		return -EFAULT;

	len = strlen(buf);
	*pos = len;
	return len;
}

static const struct file_operations cpustat_ops = {
	.open		= cpustat_open,
	.read		= seq_read,
	.write		= cpustat_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* CPU LOADS */

static int loads_show(struct seq_file *f, void *v)
{
	int cpu;
	char c;

	for_each_online_cpu(cpu) {
		c = kcpustat_cpu(cpu).load >= FALCON_LOAD_THRESHOLD ? '|' : '.';
		seq_putc(f, c);
	}
	seq_putc(f, '\n');

	return 0;
}

static int loads_open(struct inode *inode, struct file *file)
{
	return single_open(file, loads_show, NULL);
}

static const struct file_operations loads_ops = {
	.open		= loads_open,
	.read		= seq_read,
	// .write		= loads_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* THRESHOLD */

static int threshold_show(struct seq_file *f, void *v)
{
	seq_printf(f, "%d\n", FALCON_LOAD_THRESHOLD);
	return 0;
}

static int threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, threshold_show, NULL);
}

static ssize_t threshold_write(struct file *file, const char __user *ubuf,
			       size_t size, loff_t *pos)
{
	char buf[101];
	int len, thres;


	if (*pos > 0 || size > 100)
		return -EFAULT;
	if (copy_from_user(buf, ubuf, size))
		return -EFAULT;
	if (sscanf(buf, "%d", &thres) != 1)
		return -EFAULT;
	if (thres < 0 || thres > 100)
		return -EFAULT;
	
	FALCON_LOAD_THRESHOLD = thres;

	len = strlen(buf);
	*pos = len;
	return len;
}

static const struct file_operations threshold_ops = {
	.open		= threshold_open,
	.read		= seq_read,
	.write		= threshold_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* DIFF */

static int diff_show(struct seq_file *f, void *v)
{
	seq_printf(f, "%d\n", FALCON_LOAD_DIFF);
	return 0;
}

static int diff_open(struct inode *inode, struct file *file)
{
	return single_open(file, diff_show, NULL);
}

static ssize_t diff_write(struct file *file, const char __user *ubuf,
			       size_t size, loff_t *pos)
{
	char buf[101];
	int len, diff;

	if (*pos > 0 || size > 100)
		return -EFAULT;
	if (copy_from_user(buf, ubuf, size))
		return -EFAULT;
	if (sscanf(buf, "%d", &diff) != 1)
		return -EFAULT;
	if (diff < 0 || diff > 100)
		return -EFAULT;
	
	FALCON_LOAD_DIFF = diff;

	len = strlen(buf);
	*pos = len;
	return len;
}

static const struct file_operations diff_ops = {
	.open		= diff_open,
	.read		= seq_read,
	.write		= diff_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* PERCENT */

static int percent_show(struct seq_file *f, void *v)
{
	int percent = 0;
	if (FALCON_BALANCE_INTERVAL > 0)
		percent = 100 / FALCON_BALANCE_INTERVAL;
	seq_printf(f, "%d\n", percent);
	return 0;
}

static int percent_open(struct inode *inode, struct file *file)
{
	return single_open(file, percent_show, NULL);
}

static ssize_t percent_write(struct file *file, const char __user *ubuf,
			     size_t size, loff_t *pos)
{
	char buf[101];
	int len, percent;

	if (*pos > 0 || size > 100)
		return -EFAULT;
	if (copy_from_user(buf, ubuf, size))
		return -EFAULT;
	if (sscanf(buf, "%d", &percent) != 1)
		return -EFAULT;

	if (percent <= 0) {
		FALCON_BALANCE_INTERVAL = 0;
	} else {
		if (percent > 100)
			percent = 100;
		percent = 100;
		FALCON_BALANCE_INTERVAL = 100 / percent;
	}

	len = strlen(buf);
	*pos = len;
	return len;
}

static const struct file_operations percent_ops = {
	.open		= percent_open,
	.read		= seq_read,
	.write		= percent_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_stat_init(void)
{
	proc_create("stat", 0, NULL, &proc_stat_operations);
	proc_create("falcon_cpus", 0666, NULL, &falcon_cpus_ops);
	proc_create("cpustat_interval", 0666, NULL, &cpustat_ops);
	proc_create("loads", 0444, NULL, &loads_ops);
	proc_create("threshold", 0666, NULL, &threshold_ops);
	proc_create("diff", 0666, NULL, &diff_ops);
	proc_create("balance_percent", 0666, NULL, &percent_ops);
//grosplit
	proc_create("grosplit_cpus", 0666, NULL, &grosplit_cpus_ops);
//end
	return 0;
}
fs_initcall(proc_stat_init);
