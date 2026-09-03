#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "include/fingerprint_event.h"
#include <linux/init.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/kconfig.h>
#if (IS_ENABLED(CONFIG_OPLUS_FEATURE_BSP_DRV_VND_INJECT_TEST) || IS_ENABLED(CONFIG_FP_INJECT_ENABLE))
#include "include/fp_fault_inject.h"
#endif  // CONFIG_OPLUS_FEATURE_BSP_DRV_VND_INJECT_TEST || CONFIG_FP_INJECT_ENABLE

#include <linux/spinlock.h>

#define FP_EVENT_QUEUE_DEPTH 16

static struct fingerprint_message_t g_fingerprint_msg = {0};
static struct fingerprint_message_t fp_event_queue[FP_EVENT_QUEUE_DEPTH];
static unsigned int fp_q_head;
static unsigned int fp_q_tail;
static unsigned int fp_q_count;
static DEFINE_SPINLOCK(fp_event_lock);
int g_fp_driver_event_type = FP_DRIVER_INTERRUPT;
DECLARE_WAIT_QUEUE_HEAD(fp_wait_queue);

static bool fp_event_has_data(void)
{
	/* Polled by wait_event(); queue itself is always touched under
	 * fp_event_lock, wake_up() pairs with the dequeue lock. */
	return fp_q_count > 0;
}

void reset_fingerprint_msg(void)
{
	unsigned long flags;

	spin_lock_irqsave(&fp_event_lock, flags);
	fp_q_head = 0;
	fp_q_tail = 0;
	fp_q_count = 0;
	memset(&g_fingerprint_msg, 0, sizeof(g_fingerprint_msg));
	memset(fp_event_queue, 0, sizeof(fp_event_queue));
	spin_unlock_irqrestore(&fp_event_lock, flags);
}

int wait_fp_event(void *data, unsigned int size,
                           struct fingerprint_message_t **msg) {
    int ret = 0;
    struct fingerprint_message_t rev_msg = {0};
    unsigned long flags;

    if (size == sizeof(rev_msg)) {
        memcpy(&rev_msg, data, size);
    }

    /* Dequeue oldest first; loop handles a second reader stealing the
     * event between wake_up() and our spin_lock(). */
    for (;;) {
        ret = wait_event_interruptible(fp_wait_queue, fp_event_has_data());
        if (ret)
            break;

        spin_lock_irqsave(&fp_event_lock, flags);
        if (!fp_q_count) {
            spin_unlock_irqrestore(&fp_event_lock, flags);
            continue;
        }
        g_fingerprint_msg = fp_event_queue[fp_q_head];
        fp_q_head = (fp_q_head + 1) % FP_EVENT_QUEUE_DEPTH;
        fp_q_count--;
        spin_unlock_irqrestore(&fp_event_lock, flags);
        break;
    }

    if (ret) {
        pr_info("fp driver wait event fail, %d", ret);
    }
    if (msg != NULL)
        *msg = ret ? NULL : &g_fingerprint_msg;
    return ret;
}

static ssize_t fp_event_node_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    pr_info("fp_event_node_read enter");
    if (file == NULL || count != sizeof(g_fp_driver_event_type)) {
        return -1;
    }
    pr_info("fp_event_node_read,  %d", g_fp_driver_event_type);
    if (copy_to_user(buf, &g_fp_driver_event_type, count)) {
        return -EFAULT;
    }
    pr_info("fp_event_node_read,  %d", g_fp_driver_event_type);
    return count;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static const struct proc_ops fp_event_func = {
    .proc_write = NULL,
    .proc_read = fp_event_node_read,
};
#else
static struct file_operations fp_event_func = {
    .write = NULL,
    .read = fp_event_node_read,
};
#endif

int fp_evt_register_proc_fs(void)
{
    int ret = 0;
    char *tee_node = "fp_kernel_event";
    struct proc_dir_entry *event_node_dir = NULL;

    event_node_dir = proc_create(tee_node, 0666, NULL, &fp_event_func);
    if (event_node_dir == NULL) {
        ret = -1;
        goto exit;
    }

    return 0;
exit :
    return ret;
}

void set_fp_driver_evt_type(int type)
{
    pr_info("set_fp_driver_evt_type, %d", type);
    g_fp_driver_event_type = type; // FP_DRIVER_INTERRUPT
}

int get_fp_driver_evt_type(void)
{
    return g_fp_driver_event_type;
}

int send_fingerprint_msg(int module, int event, void *data,
                             unsigned int size) {
    struct fingerprint_message_t tmp_msg = {0};
    int need_report = 0;
    unsigned long flags;

    if (get_fp_driver_evt_type() != FP_DRIVER_INTERRUPT) {
        pr_debug("%s, NETLINK is enable\n", __func__);
        return 0;
    }
    tmp_msg.in_size = 0;
    tmp_msg.out_size = 0;
    switch (module) {
    case E_FP_TP:
        tmp_msg.module = E_FP_TP;
        tmp_msg.event = event == 1 ? E_FP_EVENT_TP_TOUCHDOWN : E_FP_EVENT_TP_TOUCHUP;
        tmp_msg.out_size = size <= MAX_MESSAGE_SIZE ? size : MAX_MESSAGE_SIZE;
        if (data && tmp_msg.out_size)
            memcpy(tmp_msg.out_buf, data, tmp_msg.out_size);
        need_report = 1;
        break;
    case E_FP_LCD:
        tmp_msg.module = E_FP_LCD;
        tmp_msg.event =
            event == 1 ? E_FP_EVENT_UI_READY : E_FP_EVENT_UI_DISAPPEAR;
        need_report = 1;

        //pr_info("kernel module:%d event:%d - %d", tmp_msg.module, event, tmp_msg.event);
        break;
    case E_FP_HAL:
        tmp_msg.module = E_FP_HAL;
        tmp_msg.event = E_FP_EVENT_STOP_INTERRUPT;
        need_report = 1;
        break;
    case E_TP_AIFILM:
        tmp_msg.module = E_TP_AIFILM;
        tmp_msg.event = event;
        tmp_msg.out_size = size <= MAX_MESSAGE_SIZE ? size : MAX_MESSAGE_SIZE;
        if (data && tmp_msg.out_size)
            memcpy(tmp_msg.out_buf, data, tmp_msg.out_size);
        need_report = 1;
        break;
    case E_FP_TP_GRIP:
        tmp_msg.module = E_FP_TP_GRIP;
        tmp_msg.event = event == 1 ? E_FP_EVENT_MISTOUCH_CLASP : E_FP_EVENT_MISTOUCH_UNCLASP;
        tmp_msg.out_size = size <= MAX_MESSAGE_SIZE ? size : MAX_MESSAGE_SIZE;
        if (data && tmp_msg.out_size)
            memcpy(tmp_msg.out_buf, data, tmp_msg.out_size);
        need_report = 1;
        break;
    default:
        tmp_msg.module = module;
        tmp_msg.event = event;
        need_report = 1;
        pr_info("unknow module, ignored");
        break;
    }
#if (IS_ENABLED(CONFIG_OPLUS_FEATURE_BSP_DRV_VND_INJECT_TEST) || IS_ENABLED(CONFIG_FP_INJECT_ENABLE))
    fault_inject_fp_msg_hook(&tmp_msg, &need_report);
#endif  // CONFIG_OPLUS_FEATURE_BSP_DRV_VND_INJECT_TEST || CONFIG_FP_INJECT_ENABLE
    pr_debug("%s, event_change:%d - %d, out_size:%d\n", __func__, event, tmp_msg.event, tmp_msg.out_size);
    pr_debug("%s, module:%d, event:%d\n", __func__, tmp_msg.module, tmp_msg.event);
    if (!need_report)
        return 0;

    spin_lock_irqsave(&fp_event_lock, flags);
    if (fp_q_count >= FP_EVENT_QUEUE_DEPTH) {
        /* Queue full: drop oldest so a burst of UI_READY/DOWN can never
         * stall the unlock waiting on a freed slot. */
        fp_q_head = (fp_q_head + 1) % FP_EVENT_QUEUE_DEPTH;
        fp_q_count--;
    }
    fp_event_queue[fp_q_tail] = tmp_msg;
    fp_q_tail = (fp_q_tail + 1) % FP_EVENT_QUEUE_DEPTH;
    fp_q_count++;
    spin_unlock_irqrestore(&fp_event_lock, flags);

    wake_up_interruptible(&fp_wait_queue);
    return 0;
}
