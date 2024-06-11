#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "gdev"
#define GPIO_PIN 18

static int major;
static int gpio_state = 0;

static int device_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    char msg[2];
    int msg_len;

    gpio_state = gpio_get_value(GPIO_PIN);
    msg[0] = gpio_state ? '1' : '0';
    msg[1] = '\n';
    msg_len = 2;

    if (*offset >= msg_len)
        return 0;

    if (copy_to_user(buffer, msg, msg_len))
        return -EFAULT;

    *offset += msg_len;

    return msg_len;
}

static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    char msg[1];

    if (copy_from_user(msg, buffer, 1))
        return -EFAULT;

    if (msg[0] == '1')
        gpio_set_value(GPIO_PIN, 1);
    else if (msg[0] == '0')
        gpio_set_value(GPIO_PIN, 0);
    else
        return -EINVAL;

    return length;
}

static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int __init gpio_module_init(void)
{
    int result;

    if (!gpio_is_valid(GPIO_PIN)) {
        printk(KERN_INFO "Invalid GPIO %d\n", GPIO_PIN);
        return -ENODEV;
    }

    result = gpio_request(GPIO_PIN, "sysfs");
    if (result) {
        printk(KERN_INFO "gpio request failed\n");
        return result;
    }

    gpio_direction_output(GPIO_PIN, gpio_state);
    gpio_export(GPIO_PIN, false);

    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        gpio_unexport(GPIO_PIN);
        gpio_free(GPIO_PIN);
        printk(KERN_ALERT "Registering char device failed with %d\n", major);
        return major;
    }

    printk(KERN_INFO "GPIO module loaded with device major number %d\n", major);
    return 0;
}

static void __exit gpio_module_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    gpio_unexport(GPIO_PIN);
    gpio_free(GPIO_PIN);
    printk(KERN_INFO "GPIO module unloaded\n");
}

module_init(gpio_module_init);
module_exit(gpio_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("F Z");
MODULE_DESCRIPTION("A simple GPIO Linux char driver for Raspberry Pi");
MODULE_VERSION("0.1");
