#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/uaccess.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/kdev_t.h> // MKDEV
#include <linux/types.h> // dev_t

#define DEVICE_NAME "gpdev"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("F Z");
MODULE_DESCRIPTION("A simple GPIO Linux char driver for Raspberry Pi using gpiod");
MODULE_VERSION("0.1");

/* Declate the probe and remove functions */
static int dt_probe(struct platform_device *pdev);
static int dt_remove(struct platform_device *pdev);

static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset);
static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset);
static int device_release(struct inode *inode, struct file *file);
static int device_open(struct inode *inode, struct file *file);

static struct of_device_id my_driver_ids[] = {
	{
		.compatible = "brightlight,gpdev",
	}, { /* sentinel */ }
};

static dev_t DEV_T;
static struct class *device_class = NULL;
MODULE_DEVICE_TABLE(of, my_driver_ids);

static struct platform_driver my_driver = {
	.probe = dt_probe,
	.remove = dt_remove,
	.driver = {
		.name = "m_gpdev_driver",
		.of_match_table = my_driver_ids,
	},
};

/* GPIO variable */
static struct gpio_desc *my_led = NULL;
static struct proc_dir_entry *proc_file;

static int minor;
static int major;

/**
 * @brief File operations for the GPIO char device.
 *
 * This structure defines the file operations for the GPIO char device.
 * It specifies the functions to be called when the device is opened,
 * read from, written to, and closed.
 */
static struct file_operations dev_fops = {
    .read = device_read,    /**< Function to read from the device */
    .write = device_write,  /**< Function to write to the device */
    .open = device_open,    /**< Function to open the device */
    .release = device_release /**< Function to release (close) the device */
};

/**
 * @brief Opens the GPIO device.
 *
 * This function is called when the device is opened.
 * It currently does nothing but return 0.
 *
 * @param inode Pointer to the inode structure.
 * @param file Pointer to the file structure.
 * @return Always returns 0.
 */
static int device_open(struct inode *inode, struct file *file)
{
    return 0;
}

/**
 * @brief Releases (closes) the GPIO device.
 *
 * This function is called when the device is closed.
 * It currently does nothing but return 0.
 *
 * @param inode Pointer to the inode structure.
 * @param file Pointer to the file structure.
 * @return Always returns 0.
 */
static int device_release(struct inode *inode, struct file *file)
{
    return 0;
}

/**
 * @brief Reads the state of the GPIO pin.
 *
 * This function is called when the device is read from.
 * It reads the state of the GPIO pin and returns it to the user.
 *
 * @param filp Pointer to the file structure.
 * @param buffer Pointer to the user buffer where the data will be copied.
 * @param length Length of the buffer.
 * @param offset Offset in the file (unused).
 * @return Number of bytes read on success, or negative error code on failure.
 */
static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t *offset)
{
    char msg[2];
    int msg_len;
	int gpio_state = 0;
	if (my_led)
	{
		gpio_state = gpiod_get_value(my_led);
	}
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

/**
 * @brief Writes to the GPIO device.
 *
 * This function is called when the device is written to.
 * It sets the state of the GPIO pin based on the user input.
 *
 * @param filp Pointer to the file structure.
 * @param buffer Pointer to the user buffer containing the data.
 * @param length Length of the buffer.
 * @param offset Offset in the file (unused).
 * @return Number of bytes written on success, or negative error code on failure.
 */
static ssize_t device_write(struct file *filp, const char *buffer, size_t length, loff_t *offset)
{
    char msg[1];

    if (copy_from_user(msg, buffer, 1))
        return -EFAULT;

    if (msg[0] == '1')
        gpiod_set_value(my_led, 1);
    else if (msg[0] == '0')
        gpiod_set_value(my_led, 0);
    else
        return -EINVAL;

    return length;
}

static struct proc_ops fops = {
	.proc_write = device_write,
};

/**
 * @brief This function is called on loading the driver 
 */
static int dt_probe(struct platform_device *pdev) {
	struct device *dev = &pdev->dev;
	const char *label;
	int my_value, ret;

	printk("dt_gpio - now in the probe function!\n");

	/* Check for device properties */
	if(!device_property_present(dev, "label")) {
		printk("dt_gpio - Error! Device property 'label' not found!\n");
		return -1;
	}
	if(!device_property_present(dev, "my_value")) {
		printk("dt_gpio - Error! Device property 'my_value' not found!\n");
		return -1;
	}
	if(!device_property_present(dev, "green-led-gpio")) {
		printk("dt_gpio - Error! Device property 'green-led-gpio' not found!\n");
		return -1;
	}

	/* Read device properties */
	ret = device_property_read_string(dev, "label", &label);
	if(ret) {
		printk("dt_gpio - Error! Could not read 'label'\n");
		return -1;
	}
	printk("dt_gpio - label: %s\n", label);
	ret = device_property_read_u32(dev, "my_value", &my_value);
	if(ret) {
		printk("dt_gpio - Error! Could not read 'my_value'\n");
		return -1;
	}
	printk("dt_gpio - my_value: %d\n", my_value);

	/* Init GPIO */
	my_led = gpiod_get(dev, "green-led", GPIOD_OUT_LOW);
	if(IS_ERR(my_led)) {
		printk("dt_gpio - Error! Could not setup the GPIO\n");
		return -1 * IS_ERR(my_led);
	}

	/* Creating procfs file */
	proc_file = proc_create("gled", 0666, NULL, &fops);
	if(proc_file == NULL) {
		printk("procfs_test - Error creating /proc/gled\n");
		gpiod_put(my_led);
		return -ENOMEM;
	}
	return 0;
}

/**
 * @brief This function is called on unloading the driver 
 */
static int dt_remove(struct platform_device *pdev) {
	printk("dt_gpio - Now I am in the remove function\n");
	gpiod_put(my_led);
	proc_remove(proc_file);
	return 0;
}

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void) {
    void *ptr_error;
	printk("dt_gpio - Loading the driver...\n");
	if(platform_driver_register(&my_driver)) {
		printk("dt_gpio - Error! Could not load driver\n");
		return -1;
	}
	
	major = register_chrdev(0, DEVICE_NAME, &dev_fops);
    if (major < 0) {
        printk(KERN_ALERT "Registering char device failed with %d\n", major);
        return major;
    }

    DEV_T = MKDEV(major, minor);

    /* class_create */
    device_class = class_create(DEVICE_NAME);
    if (IS_ERR(device_class))
    {
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Class creation failed\n");
    return PTR_ERR(device_class);
    }

    /* device_create */
    ptr_error = device_create(device_class, NULL, DEV_T, NULL, DEVICE_NAME);
    if (IS_ERR(ptr_error))
    {
    class_destroy(device_class);
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Device creation failed\n");
    return PTR_ERR(ptr_error);
    }

    printk(KERN_INFO "GPIO module loaded with device major number %d\n", major);
    return 0;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void) {
	printk("dt_gpio - Unload driver");
	platform_driver_unregister(&my_driver);
device_destroy(device_class, DEV_T);
    class_destroy(device_class);
	unregister_chrdev(major, DEVICE_NAME);
}

module_init(my_init);
module_exit(my_exit);
