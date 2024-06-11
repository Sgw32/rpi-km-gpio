#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "gdev"
#define GPIO_PIN 18

static int major;
static struct gpio_desc *gpio;

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

    int gpio_state = gpiod_get_value(gpio);
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
        gpiod_set_value(gpio, 1);
    else if (msg[0] == '0')
        gpiod_set_value(gpio, 0);
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

    gpio = gpiod_get(NULL, "gpio_device", 0);
    if (IS_ERR(gpio)) {
        printk(KERN_INFO "Failed to get GPIO descriptor\n");
        return PTR_ERR(gpio);
    }

    result = gpiod_direction_output(gpio, 0);
    if (result) {
        printk(KERN_INFO "Failed to set GPIO direction\n");
        gpiod_put(gpio);
        return result;
    }

    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        gpiod_put(gpio);
        printk(KERN_ALERT "Registering char device failed with %d\n", major);
        return major;
    }
    printk(KERN_INFO "GPIO module loaded with device major number %d\n", major);
    return 0;
}

static void __exit gpio_module_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    gpiod_put(gpio);
    printk(KERN_INFO "GPIO module unloaded\n");
}

module_init(gpio_module_init);
module_exit(gpio_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("F Z");
MODULE_DESCRIPTION("A simple GPIO Linux char driver for Raspberry Pi using gpiod");
MODULE_VERSION("0.1");
