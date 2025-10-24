// Minimal serdev client driver for UART3 on Raspberry Pi (BCM2711)
// - Binds to a serdev child under &uart3 via DT compatible
// - Opens the serial port, sets baudrate, logs received bytes
// - Optional echo-back (disable if using TX<->RX loopback to avoid storms)

#include <linux/module.h>
#include <linux/serdev.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/kfifo.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>

struct uart3_echo_priv {
    struct serdev_device *serdev;
    bool echo_back;
    u32 baud;
    /* Byte FIFO and polling to process data every N ms */
    struct kfifo fifo;
    spinlock_t fifo_lock;
    struct delayed_work poll_work;
    u32 period_ms; /* default 1000ms, DT: poll-period-ms */
    /* Userspace char device interface */
    struct miscdevice miscdev;
    wait_queue_head_t read_wq;
};

static void uart3_echo_poll(struct work_struct *work)
{
    struct uart3_echo_priv *priv =
        container_of(to_delayed_work(work), struct uart3_echo_priv, poll_work);
    struct device *dev = &priv->serdev->dev;
    unsigned int total;
    unsigned int preview_len;
    unsigned char preview[32];

    /* Peek into FIFO for logging without consuming user data */
    spin_lock(&priv->fifo_lock);
    total = kfifo_len(&priv->fifo);
    preview_len = min_t(unsigned int, total, sizeof(preview));
    if (preview_len)
        preview_len = kfifo_out_peek(&priv->fifo, preview, preview_len);
    spin_unlock(&priv->fifo_lock);

    if (total) {
        char hex[3 * 32 + 1];
        unsigned int i, p = 0;
        for (i = 0; i < preview_len && p + 3 < sizeof(hex); i++)
            p += scnprintf(hex + p, sizeof(hex) - p, "%02x ", preview[i]);
        dev_info(dev, "poll %u ms: fifo %u bytes, first %u: %s\n",
                 priv->period_ms, total, preview_len, hex);
    } else {
        dev_dbg(dev, "poll %u ms: no data\n", priv->period_ms);
    }

    /* Re-arm periodic work */
    schedule_delayed_work(&priv->poll_work, msecs_to_jiffies(priv->period_ms));
}

static size_t uart3_echo_receive(struct serdev_device *serdev,
                                 const u8 *buf, size_t count)
{
    struct uart3_echo_priv *priv = serdev_device_get_drvdata(serdev);

    dev_info(&serdev->dev, "rx %zu bytes\n", count);

    if (priv && count) {
        unsigned int in = kfifo_in_spinlocked(&priv->fifo, buf, count, &priv->fifo_lock);
        if (in < count)
            dev_warn(&serdev->dev, "fifo overflow: dropped %zu bytes\n", count - in);
        /* wake up any blocking readers */
        if (in)
            wake_up_interruptible(&priv->read_wq);
    }

    if (priv && priv->echo_back && count) {
        int n = serdev_device_write_buf(serdev, buf, count);
        dev_info(&serdev->dev, "echoed %d bytes\n", n);
    }
    return count; // consumed all bytes
}

static void uart3_echo_write_wakeup(struct serdev_device *serdev)
{
    // Nothing to do; required by serdev ops signature
}

static const struct serdev_device_ops uart3_echo_ops = {
    .receive_buf = uart3_echo_receive,
    .write_wakeup = uart3_echo_write_wakeup,
};

/*
 * Character device: /dev/uart3_echo
 * - read(): drains from FIFO to userspace
 * - poll(): signals readable when FIFO has data
 * - write(): sends bytes to underlying serdev (optional)
 */

static ssize_t uart3_echo_chr_read(struct file *filp, char __user *ubuf,
                                   size_t len, loff_t *ppos)
{
    struct uart3_echo_priv *priv = container_of(filp->private_data, struct uart3_echo_priv, miscdev);
    size_t want;
    unsigned int copied;
    unsigned char *kbuf;
    int ret;

    if (len == 0)
        return 0;

    /* Blocking behavior: wait for data if FIFO empty */
    if (kfifo_is_empty(&priv->fifo)) {
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        ret = wait_event_interruptible(priv->read_wq, !kfifo_is_empty(&priv->fifo));
        if (ret)
            return ret;
    }

    /* Limit a single read chunk to reasonable size */
    want = min_t(size_t, len, 4096);
    kbuf = kmalloc(want, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    spin_lock(&priv->fifo_lock);
    copied = kfifo_out(&priv->fifo, kbuf, want);
    spin_unlock(&priv->fifo_lock);

    if (!copied) {
        kfree(kbuf);
        return 0;
    }
    if (copy_to_user(ubuf, kbuf, copied)) {
        kfree(kbuf);
        return -EFAULT;
    }
    kfree(kbuf);
    return copied;
}

static __poll_t uart3_echo_chr_poll(struct file *filp, poll_table *wait)
{
    struct uart3_echo_priv *priv = container_of(filp->private_data, struct uart3_echo_priv, miscdev);
    __poll_t mask = 0;

    poll_wait(filp, &priv->read_wq, wait);
    if (!kfifo_is_empty(&priv->fifo))
        mask |= POLLIN | POLLRDNORM;
    return mask;
}

static ssize_t uart3_echo_chr_write(struct file *filp, const char __user *ubuf,
                                    size_t len, loff_t *ppos)
{
    struct uart3_echo_priv *priv = container_of(filp->private_data, struct uart3_echo_priv, miscdev);
    unsigned char *kbuf;
    int n;

    if (len == 0)
        return 0;
    if (len > 4096)
        len = 4096;

    kbuf = memdup_user(ubuf, len);
    if (IS_ERR(kbuf))
        return PTR_ERR(kbuf);

    n = serdev_device_write_buf(priv->serdev, kbuf, len);
    kfree(kbuf);
    if (n < 0)
        return n;
    return n;
}

static int uart3_echo_chr_open(struct inode *inode, struct file *filp)
{
    /* nothing to do; miscdevice->this_device is already set */
    return 0;
}

static const struct file_operations uart3_echo_fops = {
    .owner = THIS_MODULE,
    .read = uart3_echo_chr_read,
    .write = uart3_echo_chr_write,
    .poll = uart3_echo_chr_poll,
    .open = uart3_echo_chr_open,
    .llseek = noop_llseek,
};

static int uart3_echo_probe(struct serdev_device *serdev)
{
    struct device *dev = &serdev->dev;
    struct uart3_echo_priv *priv;
    int ret;
    u32 baud = 115200;
    bool echo_back = false;
    u32 period_ms = 1000;

    device_property_read_u32(dev, "current-speed", &baud);
    echo_back = device_property_read_bool(dev, "echo");
    device_property_read_u32(dev, "poll-period-ms", &period_ms);

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->serdev = serdev;
    priv->echo_back = echo_back;
    priv->baud = baud;
    priv->period_ms = period_ms;
    spin_lock_init(&priv->fifo_lock);
    init_waitqueue_head(&priv->read_wq);
    ret = kfifo_alloc(&priv->fifo, 4096, GFP_KERNEL);
    if (ret) {
        dev_err(dev, "failed to alloc fifo: %d\n", ret);
        return ret;
    }
    serdev_device_set_drvdata(serdev, priv);

    serdev_device_set_client_ops(serdev, &uart3_echo_ops);

    ret = serdev_device_open(serdev);
    if (ret) {
        dev_err(dev, "failed to open serdev: %d\n", ret);
        kfifo_free(&priv->fifo);
        return ret;
    }

    serdev_device_set_flow_control(serdev, false);
    ret = serdev_device_set_baudrate(serdev, baud);
    if (!ret)
        dev_info(dev, "configured baudrate %u\n", baud);

    // Send a greeting to help testing (will be received back if loopback wired)
    {
        const char hello[] = "[kernel] uart3-echo online\r\n";
        serdev_device_write_buf(serdev, hello, sizeof(hello) - 1);
    }

    INIT_DELAYED_WORK(&priv->poll_work, uart3_echo_poll);
    schedule_delayed_work(&priv->poll_work, msecs_to_jiffies(priv->period_ms));

    /* Register misc chardev for userspace access */
    priv->miscdev.minor = MISC_DYNAMIC_MINOR;
    priv->miscdev.name = "uart3_echo";
    priv->miscdev.fops = &uart3_echo_fops;
    priv->miscdev.parent = dev;
    ret = misc_register(&priv->miscdev);
    if (ret) {
        dev_err(dev, "failed to register miscdev: %d\n", ret);
        cancel_delayed_work_sync(&priv->poll_work);
        serdev_device_close(serdev);
        kfifo_free(&priv->fifo);
        return ret;
    }

    dev_info(dev, "echo_back=%d, poll-period-ms=%u\n", priv->echo_back, priv->period_ms);
    return 0;
}

static void uart3_echo_remove(struct serdev_device *serdev)
{
    struct uart3_echo_priv *priv = serdev_device_get_drvdata(serdev);
    if (priv) {
        cancel_delayed_work_sync(&priv->poll_work);
        misc_deregister(&priv->miscdev);
        kfifo_free(&priv->fifo);
    }
    serdev_device_close(serdev);
}

static const struct of_device_id uart3_echo_of_match[] = {
    { .compatible = "codex,uart3-echo" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, uart3_echo_of_match);

static struct serdev_device_driver uart3_echo_driver = {
    .probe = uart3_echo_probe,
    .remove = uart3_echo_remove,
    .driver = {
        .name = "uart3_serdev_echo",
        .of_match_table = uart3_echo_of_match,
    },
};

module_serdev_device_driver(uart3_echo_driver);

MODULE_AUTHOR("Codex CLI");
MODULE_DESCRIPTION("Minimal serdev client for UART3 echo/log");
MODULE_LICENSE("GPL v2");
