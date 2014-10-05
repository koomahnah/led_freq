#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>

#define LED_FREQ_MAJOR	0
#define LED_FREQ_MINOR	0
#define LED_FREQ_DEVICES	1

MODULE_LICENSE("Dual BSD/GPL");

struct led_freq_dev {
	struct cdev cdev;
	int led_frequency;
	int interrupt;
	int led_fail;
};

struct led_susfr_data {
	int freq;
	struct led_freq_dev *this_dev;
};

static struct task_struct *my_thread;
static struct led_susfr_data my_susfr_data;
static struct led_freq_dev my_led_freq_dev;

static dev_t my_dev;
static int led_freq_devices;
static int led_freq_major = LED_FREQ_MAJOR;
static int led_freq_minor = LED_FREQ_MINOR;
static int led_freq_open(struct inode *inode, struct file *filp);
static int led_freq_release(struct inode *inode, struct file *filp);
static ssize_t led_freq_read(struct file *f, char __user *u, size_t s,
			     loff_t *l);
static ssize_t led_freq_write(struct file *f, const char __user *u,
			      size_t s, loff_t *t);
int led_sustain_freq(void *data);

const struct file_operations led_freq_fops = {
	.owner = THIS_MODULE,
	.open = led_freq_open,
	.read = led_freq_read,
	.write = led_freq_write,
	.release = led_freq_release,
};

static void led_freq_setup_cdev(struct led_freq_dev *dev, int index)
{
	int err, devn = MKDEV(led_freq_major, led_freq_minor + index);
	cdev_init(&dev->cdev, &led_freq_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &led_freq_fops;
	err = cdev_add(&dev->cdev, devn, 1);
	if (err)
		pr_devel("Oh no! No device added.\n");
}

static int led_freq_open(struct inode *inode, struct file *filp)
{
	struct led_freq_dev *this_dev =
	    container_of(inode->i_cdev, struct led_freq_dev, cdev);
	unsigned int major = imajor(inode);
	unsigned int minor = iminor(inode);
	filp->private_data = this_dev;
	pr_devel("Whoaa, opened! Major %i, minor %i.\n", major,
	       minor);
	if (filp->f_flags & O_RDWR)
		pr_devel("RDWR flag set.\n");
	if (filp->f_flags & O_TRUNC) 
		pr_devel("TRUNC flag set.\n");
	if (filp->f_flags & O_APPEND)
		pr_devel("APPEND flag set.\n");

	return 0;
}

static int led_freq_release(struct inode *inode, struct file *filp)
{
	pr_devel("Whooo... released.\n");
	return 0;
}

static ssize_t led_freq_read(struct file *f, char __user *u, size_t s,
			     loff_t *f_pos)
{
	char *buf;
	struct led_freq_dev *this_dev = f->private_data;
	int tmp = this_dev->led_frequency;
	int i;
	int digits = 0;
	pr_devel(
	       "Hello_read, size_t: %zx, major: %i, minor: %i, offset: %lld\n",
	       s, imajor(f->f_inode), iminor(f->f_inode), *f_pos);
	if (*f_pos > 0)
		return 0;
	while (tmp > 0) {
		digits++;
		tmp /= 10;
	}
	buf = kmalloc(digits + 1, GFP_KERNEL);
	tmp = this_dev->led_frequency;
	i = digits - 1;
	while (tmp > 0) {
		buf[i] = (char) ((tmp % 10) + 48);
		tmp /= 10;
		i--;
	}
	buf[digits] = '\n';
	if (copy_to_user(u, buf, digits + 1) != 0) {
		pr_devel("Hello_read, copying failure.\n");
		return -EFAULT;
	}
	(*f_pos) += digits + 1;
	pr_devel("Hello_read, f_pos now is %lld, bye!\n", *f_pos);
	kfree(buf);
	return digits + 1;
}

static ssize_t led_freq_write(struct file *f, const char __user *u,
			      size_t s, loff_t *f_pos)
{
	char *buf;
	struct led_freq_dev *this_dev = f->private_data;
	int tmp_freq = 0;

	pr_devel(
	       "Hello_write, size_t: %zx, major: %i, minor: %i, offset given: %lld, internal offset: %lld",
	       s, imajor(f->f_inode), iminor(f->f_inode), *f_pos,
	       f->f_pos);
	if (f->f_flags & O_RDWR)
		pr_devel("RDWR flag set.\n");
	if (f->f_flags & O_TRUNC)
		pr_devel("TRUNC flag set.\n");
	if (f->f_flags & O_APPEND)
		pr_devel("APPEND flag set.\n");
	if (this_dev->led_fail)
		return -1;
	buf = kmalloc(s + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_devel("Hello_write, kmalloc failure.\n");
		return -1;
	}
	pr_devel("Copying, size %zx\n", s);
	if (copy_from_user(buf, u, s) != 0) {
		pr_devel("Hello_write, copying failure.\n");
		return -EFAULT;
	}
	buf[s] = 0;
	kstrtou32(buf, 10, &tmp_freq);
	this_dev->interrupt = 1;
	barrier();
	if (tmp_freq) {
		my_susfr_data.freq = tmp_freq;
		my_susfr_data.this_dev = this_dev;
		my_thread =
		    kthread_create(led_sustain_freq, &my_susfr_data,
				   "led_sustain_freq");
		if (my_thread)
			wake_up_process(my_thread);
	}
	pr_devel("tmp_freq is %i", tmp_freq);
	pr_devel(
	       "Hello_write, given f_pos now is %lld, internal f_pos is %lld, bye!\n",
	       *f_pos, f->f_pos);
	(*f_pos) += s;
	kfree(buf);
	return s;
}

int led_sustain_freq(void *data)
{
	static int state;
	unsigned long j;
	struct led_susfr_data *tmp = data;
	tmp->this_dev->interrupt = 0;
	if (tmp->freq == 0)
		return 0;
	while (tmp->this_dev->interrupt == 0) {
		j = jiffies;
		j += (HZ / tmp->freq);
		while (time_before(jiffies, j))
			schedule();
		gpio_set_value(16, state); 
		pr_devel("State is %i\n", state);
		state = !state;
	}
	return 0;
}


static int led_freq_init(void)
{
	int result;
	led_freq_devices = LED_FREQ_DEVICES;
	if (led_freq_major) {
		my_dev = MKDEV(led_freq_major, led_freq_minor);
		result =
		    register_chrdev_region(my_dev, led_freq_devices,
					   "led_freq");
	} else {
		result =
		    alloc_chrdev_region(&my_dev, led_freq_minor,
					led_freq_devices, "led_freq");
	}
	if (result < 0) {
		pr_devel("Damn it, so wrong! No major number assigned.\n");
		return result;
	}
	led_freq_major = MAJOR(my_dev);
	led_freq_minor = MINOR(my_dev);
	led_freq_setup_cdev(&my_led_freq_dev, 0);
	pr_devel("Hello, world. Major: %i\n", MAJOR(my_dev));
	gpio_free(16);
	my_led_freq_dev.led_fail = gpio_request(16, "testpin");
	if(my_led_freq_dev.led_fail)
		pr_devel("Error, returned %i\n", my_led_freq_dev.led_fail);
	else{
		pr_devel("Success requesting.\n");
		if(gpio_direction_output(16, 1)){
			pr_devel("Fail setting to output.\n");
			my_led_freq_dev.led_fail = 1;
		}
	}
	return 0;
}

static void led_freq_exit(void)
{
	cdev_del(&(my_led_freq_dev.cdev));
	unregister_chrdev_region(my_dev, 1);
	pr_devel("Goodbye, cruel world\n");
	if(!my_led_freq_dev.led_fail)
		gpio_free(16);

}

module_init(led_freq_init);
module_exit(led_freq_exit);
