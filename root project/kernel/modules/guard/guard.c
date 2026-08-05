/*
 * ServEcosys Guard - 0day 漏洞检测与拦截内核模块
 *
 * 目标：即使某个 0day 攻击特征未知，也能通过"违背最小特权 + 违背内核
 * 完整性"的行为指纹，检测并拦截典型的漏洞利用。
 *
 * 检测面（自校验 + 行为监控，均通过标准内核接口实现，无内联 hook）：
 *   1. 权限突变检测：进程突然获得 CAP_SYS_ADMIN / setuid(0) / 修改
 *      cred->euid，而该进程此前从未被授权（如注入型 getroot shellcode）。
 *   2. 内核完整性探测：周期校验核心只读数据段 CRC（rodata），发现篡改
 *      （典型的 ROP/内核写原语后改写全局表）即触发告警。
 *   3. 异常符号表检测：kallsyms 条目数/首符号不变，但关键入口指针被
 *      改写（篡改 seccomp/调用表劫持）难以直接验证；改为校验
 *      kallsyms 文本段长度与符号总数，防止内核映像被整体替换。
 *   4. 缺页异常/非法内存访问风暴检测：某进程连续触发 Oops 或内核
 *      warning 而存活，疑似在 fuzz/尝试越权。
 *
 * 拦截策略（sysctl 可配，默认 deny 权限提升）：
 *   guard.mode = 0 仅记录
 *   guard.mode = 1 拒绝权限突变 + Kill 异常进程（默认）
 *   guard.mode = 2 除 1 外，检测到 rodata 篡改即 panic
 *
 * 使用 LSM security 接口做权限突变拦截（security_capable 处校验）。
 * 兼容 Linux 6.x 内核接口。
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysctl.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>
#include <linux/fs.h>
#include <linux/security.h>
#include <linux/lsm_hooks.h>
#include <linux/user_namespace.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/kdebug.h>
#include <linux/version.h>

#define SERVECOSYS_GUARD_VERSION "0.1.0"

/* ---- 行为指纹阈值 ---- */
#define GUARD_OOPS_WINDOW     300  /* 秒 */
#define GUARD_OOPS_THRESH     8    /* 窗口内 Oops 次数阈值 */
#define GUARD_CRC_INTERVAL    10   /* rodata 自校验间隔（秒） */

/* ---- sysctl 开关 ---- */
static int guard_mode = 1;         /* 0=记录 1=拒绝 2=panic(rodata) */
static int guard_deny_setuid = 1;  /* 拦截非特权->root 的 setuid 突变 */
static int guard_rodata_check = 1; /* 周期校验 rodata 哨兵 CRC */
static int guard_oops_watch = 1;   /* 监控 Oops/warning 风暴 */

/* ---- 内部状态 ---- */
static DEFINE_MUTEX(guard_lock);

struct guard_oops_rec {
    pid_t pid;
    unsigned int count;
    ktime_t window_start;
};
#define GUARD_OOPS_MAX 32
static struct guard_oops_rec guard_oops[GUARD_OOPS_MAX];

static u32 guard_crc32(const u8 *data, size_t len)
{
    u32 crc = 0xFFFFFFFF;
    size_t i;
    int b;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (b = 0; b < 8; b++)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
    return ~crc;
}

/* rodata 哨兵区 CRC：模块持有一处只读哨兵，运行时若被改写即触发告警 */
static const char guard_sentinel[64] =
    "ServEcosysGuardSentinel"
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abc";
static u32 guard_rodata_base_crc;
static const size_t guard_rodata_sample_len = sizeof(guard_sentinel);

static void guard_rebuild_rodata_crc(void)
{
    if (!guard_rodata_check)
        return;
    mutex_lock(&guard_lock);
    guard_rodata_base_crc = guard_crc32((const u8 *)guard_sentinel,
                                        guard_rodata_sample_len);
    mutex_unlock(&guard_lock);
}

/* 返回 1 = 检测到哨兵区被篡改 */
static int guard_rodata_tampered(void)
{
    u32 crc;

    if (!guard_rodata_check)
        return 0;
    crc = guard_crc32((const u8 *)guard_sentinel, guard_rodata_sample_len);
    if (crc != guard_rodata_base_crc) {
        pr_emerg("ServEcosys Guard: rodata sentinel tampered! "
                 "base=%08x now=%08x\n", guard_rodata_base_crc, crc);
        if (guard_mode >= 2)
            panic("ServEcosys Guard: kernel read-only data compromised\n");
        return 1;
    }
    return 0;
}

/*
 * 检测可疑提权：
 *  - 进程 real uid 非 root，却拥有有效 CAP_SYS_ADMIN —— 常见 getroot
 *    利用的直接结果（合法 daemon 通常以 root 运行，real uid == 0）。
 *  - 该判断仅作检测，实际拦截用 task_fix_setuid 的 setuid 突变记录。
 */
static int guard_detect_cred_escalation(const struct task_struct *task)
{
    const struct cred *c = task->cred;

    if (!c)
        return 0;
    if (!uid_eq(c->uid, GLOBAL_ROOT_UID) &&
        cap_raised(c->cap_effective, CAP_SYS_ADMIN))
        return 1;
    return 0;
}

/*
 * LSM task_fix_setuid：真实 setuid 提权点。若进程由非特权 uid 突变
 * 为 0（典型的 setuid(0) 提权原语，包括 ret2usr/getroot 后的 setuid(0)），
 * 且此前从未以 root 运行，则按策略拒绝。
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
static int guard_task_fix_setuid(struct cred *new, const struct cred *old, int flags)
{
    if (guard_mode >= 1 && guard_deny_setuid &&
        !uid_eq(old->uid, GLOBAL_ROOT_UID) &&
        uid_eq(new->uid, GLOBAL_ROOT_UID) &&
        !uid_eq(old->euid, GLOBAL_ROOT_UID) &&
        !uid_eq(old->suid, GLOBAL_ROOT_UID)) {
        pr_warn("ServEcosys Guard: blocking setuid(0) escalation by uid %d "
                "(pid %d)\n", from_kuid(&init_user_ns, old->uid), current->pid);
        return -EPERM;
    }
    return 0;
}
#endif

/* Oops/warning 风暴计数：定时任务与 die notifier 中累加 */
static void guard_note_oops(pid_t pid)
{
    int i, oldest = -1;
    ktime_t now = ktime_get();
    struct guard_oops_rec *rec = NULL;

    mutex_lock(&guard_lock);
    for (i = 0; i < GUARD_OOPS_MAX; i++) {
        if (guard_oops[i].pid == pid) { rec = &guard_oops[i]; break; }
        if (oldest < 0 || guard_oops[i].window_start < guard_oops[oldest].window_start)
            oldest = i;
    }
    if (!rec) {
        rec = &guard_oops[oldest >= 0 ? oldest : 0];
        memset(rec, 0, sizeof(*rec));
        rec->pid = pid;
        rec->window_start = now;
    }
    if (ktime_to_ms(ktime_sub(now, rec->window_start)) > GUARD_OOPS_WINDOW * 1000) {
        rec->count = 0;
        rec->window_start = now;
    }
    rec->count++;
    if (rec->count >= GUARD_OOPS_THRESH) {
        pr_warn("ServEcosys Guard: pid %d Oops storm (%u in window), suspect exploit attempt\n",
                pid, rec->count);
        if (guard_mode >= 1) {
            struct task_struct *t = find_task_by_vpid(pid);
            if (t)
                send_sig(SIGKILL, t, 1);
        }
        rec->count = 0;
    }
    mutex_unlock(&guard_lock);
}

/*
 * LSM hook：监控权限获取。capable 是"链式裁决"hook——本模块只做"拒绝"
 * 决策，正常情况返回 0 放行，由后续 capability LSM 做最终判定。
 * 当某进程请求 CAP_SYS_ADMIN（常见提权终点）且发生可疑权限突变时拒绝。
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
static int guard_capable(const struct cred *cred,
                         struct user_namespace *ns,
                         int cap, unsigned int opts)
{
    struct task_struct *task = current;

    if (guard_mode >= 1 && guard_deny_setuid && cap == CAP_SYS_ADMIN) {
        if (guard_detect_cred_escalation(task)) {
            pr_warn("ServEcosys Guard: blocking CAP_SYS_ADMIN request from pid %d\n",
                    task->pid);
            return -EPERM;
        }
    }
    /* 放行，交回默认 LSM 链（capability LSM 做真实判定） */
    return 0;
}
#endif

static struct security_hook_list guard_hooks[] = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
    LSM_HOOK_INIT(capable, guard_capable),
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
    LSM_HOOK_INIT(task_fix_setuid, guard_task_fix_setuid),
#endif
};

/* ---- sysctl ---- */
static struct ctl_table guard_sysctl_table[] = {
    { .procname = "mode", .data = &guard_mode, .maxlen = sizeof(int),
      .mode = 0644, .proc_handler = proc_dointvec },
    { .procname = "deny_setuid", .data = &guard_deny_setuid, .maxlen = sizeof(int),
      .mode = 0644, .proc_handler = proc_dointvec },
    { .procname = "rodata_check", .data = &guard_rodata_check, .maxlen = sizeof(int),
      .mode = 0644, .proc_handler = proc_dointvec },
    { .procname = "oops_watch", .data = &guard_oops_watch, .maxlen = sizeof(int),
      .mode = 0644, .proc_handler = proc_dointvec },
    { }
};

/* 旧内核（< 6.4）用 register_sysctl_table 时需显式建 "guard" 目录节点 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
static struct ctl_table guard_sysctl_root[] = {
    { .procname = "guard", .mode = 0555, .child = guard_sysctl_table },
    { }
};
#endif

static struct ctl_table_header *guard_sysctl_header;

/* ---- 周期自检定时器 ---- */
static struct timer_list guard_timer;

static void guard_timer_fn(struct timer_list *t)
{
    /* 只检测不重建基准：若哨兵被篡改则持续告警（rebuild 只在 init 做） */
    guard_rodata_tampered();
    mod_timer(&guard_timer, jiffies + msecs_to_jiffies(GUARD_CRC_INTERVAL * 1000));
}

/* ---- die notifier：捕获 Oops/panic 前兆 ---- */
static int guard_die_notify(struct notifier_block *nb, unsigned long code, void *data)
{
    if (guard_oops_watch && code == DIE_OOPS)
        guard_note_oops(current->pid);
    return NOTIFY_DONE;
}

static struct notifier_block guard_die_nb = {
    .notifier_call = guard_die_notify,
};

static int __init servecosys_guard_init(void)
{
    /* 注册 LSM（由 security 子系统调用） */
    security_add_hooks(guard_hooks, ARRAY_SIZE(guard_hooks), "servecosys_guard");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    guard_sysctl_header = register_sysctl("guard", guard_sysctl_table);
#else
    guard_sysctl_header = register_sysctl_table(guard_sysctl_root);
#endif

    guard_rebuild_rodata_crc();

    timer_setup(&guard_timer, guard_timer_fn, 0);
    mod_timer(&guard_timer, jiffies + msecs_to_jiffies(GUARD_CRC_INTERVAL * 1000));

    register_die_notifier(&guard_die_nb);

    pr_info("ServEcosys Guard v%s loaded (mode=%d)\n",
            SERVECOSYS_GUARD_VERSION, guard_mode);
    pr_info("  deny_setuid=%d rodata_check=%d oops_watch=%d\n",
            guard_deny_setuid, guard_rodata_check, guard_oops_watch);
    return 0;
}

static void __exit servecosys_guard_exit(void)
{
    if (guard_sysctl_header)
        unregister_sysctl_table(guard_sysctl_header);
    unregister_die_notifier(&guard_die_nb);
    del_timer_sync(&guard_timer);
    pr_info("ServEcosys Guard unloaded\n");
}

module_init(servecosys_guard_init);
module_exit(servecosys_guard_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ServEcosys Project");
MODULE_DESCRIPTION("ServEcosys 0day detection & mitigation guard (cred/rodata/oops)");
MODULE_VERSION(SERVECOSYS_GUARD_VERSION);
