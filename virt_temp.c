#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "virt_temp"
#define CLASS_NAME "virt"
#define BUF_SIZE 1024
#define IOCTL_SET_SAMPLING_RATE _IOW('t', 1, int)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Virtual Temperature Sensor Driver");
MODULE_VERSION("1.0");

static int sampling_rate = 1; // 1 second by default
static char buffer[BUF_SIZE] = "25.5\n"; // initial virtual temp reading
static size_t data_len = 6; // includes newline

static dev_t dev_num;
static struct class *virt_class;
static struct device *virt_device;
static struct cdev virt_cdev;
static DEFINE_SPINLOCK(buffer_lock);

// ---------- File Operations ----------

static int virt_open(struct inode *inode, struct file *file) {
    pr_info("virt_temp: device opened\n");
    return 0;
}

static int virt_release(struct inode *inode, struct file *file) {
    pr_info("virt_temp: device closed\n");
    return 0;
}

static ssize_t virt_read(struct file *file, char __user *user_buf, size_t len, loff_t *offset) {
    ssize_t ret;
    unsigned long flags;

    spin_lock_irqsave(&buffer_lock, flags);

    if (*offset >= data_len) {
        spin_unlock_irqrestore(&buffer_lock, flags);
        return 0; // EOF
    }

    if (*offset + len > data_len)
        len = data_len - *offset;

    if (copy_to_user(user_buf, buffer + *offset, len)) {
        spin_unlock_irqrestore(&buffer_lock, flags);
        return -EFAULT;
    }

    *offset += len;
    ret = len;

    spin_unlock_irqrestore(&buffer_lock, flags);
    return ret;
}

static ssize_t virt_write(struct file *file, const char __user *user_buf, size_t len, loff_t *offset) {
    ssize_t ret;
    unsigned long flags;

    if (len >= BUF_SIZE)
        return -EINVAL;

    spin_lock_irqsave(&buffer_lock, flags);

    memset(buffer, 0, BUF_SIZE);
    if (copy_from_user(buffer, user_buf, len)) {
        spin_unlock_irqrestore(&buffer_lock, flags);
        return -EFAULT;
    }

    buffer[len] = '\0';  // Null-terminate
    data_len = len;
    ret = len;

    spin_unlock_irqrestore(&buffer_lock, flags);
    return ret;
}

static long virt_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case IOCTL_SET_SAMPLING_RATE:
            if (copy_from_user(&sampling_rate, (int __user *)arg, sizeof(int)))
                return -EFAULT;
            pr_info("virt_temp: Sampling rate set to %d sec\n", sampling_rate);
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

// ---------- File Ops Registration ----------

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = virt_open,
    .release = virt_release,
    .read = virt_read,
    .write = virt_write,
    .unlocked_ioctl = virt_ioctl,
};

// ---------- Init and Exit ----------

static int __init virt_init(void) {
    int ret;

    // Allocate device number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("virt_temp: Failed to allocate major number\n");
        return ret;
    }

    // Create device class
    virt_class = class_create(CLASS_NAME);
    if (IS_ERR(virt_class)) {
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(virt_class);
    }

    // Create device node
    virt_device = device_create(virt_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(virt_device)) {
        class_destroy(virt_class);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(virt_device);
    }

    // Init and add char device
    cdev_init(&virt_cdev, &fops);
    ret = cdev_add(&virt_cdev, dev_num, 1);
    if (ret < 0) {
        device_destroy(virt_class, dev_num);
        class_destroy(virt_class);
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    pr_info("virt_temp: Module loaded. Device: /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit virt_exit(void) {
    cdev_del(&virt_cdev);
    device_destroy(virt_class, dev_num);
    class_destroy(virt_class);
    unregister_chrdev_region(dev_num, 1);
    pr_info("virt_temp: Module unloaded\n");
}

module_init(virt_init);
module_exit(virt_exit);
