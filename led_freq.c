#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/gpio.h>

#define LED_FREQ_MAJOR	0
#define LED_FREQ_MINOR	0
#define LED_FREQ_DEVICES	1
#define LED_FREQ_DATA_SIZE	50

MODULE_LICENSE("Dual BSD/GPL");

static dev_t my_dev;
static int led_freq_devices;
static int led_freq_major = LED_FREQ_MAJOR;
static int led_freq_minor = LED_FREQ_MINOR;
static int led_freq_open(struct inode *inode, struct file *filp);
static int led_freq_release(struct inode *inode, struct file *filp); 
static int led_freq_read(struct file *f, char __user *u, size_t s, loff_t *l);
static ssize_t led_freq_write(struct file *f, const char __user *u, size_t s, loff_t *t);

struct led_freq_dev{
	struct cdev cdev;
	int led_frequency; 
	int interrupt;
};

static struct led_freq_dev my_led_freq_dev;

struct file_operations led_freq_fops = {
	.owner =	THIS_MODULE,
	.open =		led_freq_open,
	.read = 	led_freq_read,
	.write = 	led_freq_write,
	.release = 	led_freq_release,
};
static void led_freq_setup_cdev(struct led_freq_dev *dev, int index){
	int err, devn = MKDEV(led_freq_major, led_freq_minor + index);
	cdev_init(&dev->cdev, &led_freq_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &led_freq_fops;
	err = cdev_add(&dev->cdev, devn, 1);
	if(err)
		printk(KERN_ALERT "Oh no! No device added.\n");
}

static int led_freq_open(struct inode *inode, struct file *filp){
	struct led_freq_dev *this_dev = container_of(inode->i_cdev, struct led_freq_dev, cdev);
	unsigned int major = imajor(inode);
	unsigned int minor = iminor(inode);
	int i;
	filp->private_data = this_dev;
	printk(KERN_ALERT "Whoaa, opened! Major %i, minor %i.\n", major, minor);
	if(filp->f_flags & O_RDWR)
		printk(KERN_ALERT "RDWR flag set.\n");
	if(filp->f_flags & O_TRUNC){
		printk(KERN_ALERT "TRUNC flag set.\n");
	}
	if(filp->f_flags & O_APPEND)
		printk(KERN_ALERT "APPEND flag set.\n");
	gpio_set_value(16, this_dev->led_value); 
	printk(KERN_ALERT "Pin set to %i", this_dev->led_value);

	return 0;
}
static int led_freq_release(struct inode *inode, struct file *filp){
	printk(KERN_ALERT "Whooo... released.\n");
	return 0;
}

static int led_freq_read(struct file *f, char __user *u, size_t s, loff_t *f_pos){
	char *buf;
	struct led_freq_dev *this_dev = f->private_data;
	int tmp = this_dev->frequency;
	int i;
	i= 0;
	printk(KERN_ALERT "Hello_read, size_t: %i, major: %i, minor: %i, offset: %lld\n", s, imajor(f->f_inode), iminor(f->f_inode), *f_pos);
	if(*f_pos>0){
		printk(KERN_ALERT "Hello_read, looking too far, failure.\n");
		return 0;
	}
	while(tmp>0){
		i++;
		tmp/=10;
	}
	buf = kmalloc(i, GFP_KERNEL);
	tmp = frequency;
	i = 0;
	while(tmp>0){
		buf[i] = tmp % 10;
		tmp/=10;
		i++;
	}
	if(copy_to_user(u, buf, s)!=0){
		printk(KERN_ALERT "Hello_read, copying failure.\n");
		return -EFAULT;
	}
	(*f_pos)+=s;
	printk(KERN_ALERT "Hello_read, f_pos now is %lld, bye!\n", *f_pos);
	kfree(buf);
	return s;
}

static ssize_t led_freq_write(struct file *f, const char __user *u, size_t s, loff_t *f_pos){
	loff_t i = *f_pos;
	char *buf;
	struct led_freq_dev *this_dev = f->private_data;

	printk(KERN_ALERT "Hello_write, size_t: %i, major: %i, minor: %i, offset given: %lld, internal offset: %lld", s, imajor(f->f_inode), iminor(f->f_inode), *f_pos, f->f_pos);
	if(f->f_flags & O_RDWR)
		printk(KERN_ALERT "RDWR flag set.\n");
	if(f->f_flags & O_TRUNC)
		printk(KERN_ALERT "TRUNC flag set.\n");
	if(f->f_flags & O_APPEND)
		printk(KERN_ALERT "APPEND flag set.\n");

	buf = kmalloc(s, GFP_KERNEL);
	if(buf==NULL){
		printk(KERN_ALERT "Hello_write, kmalloc failure.\n");
		*f_pos = i;
		return -1;
	}
	printk(KERN_ALERT "Copying, size %i\n", s);
	if(copy_from_user(buf, u, s)!=0){
		printk(KERN_ALERT "Hello_write, copying failure.\n");
		*f_pos = i;
		return -EFAULT;
	}
	(*f_pos)+=s;
	printk(KERN_ALERT "Data end\n");

	printk(KERN_ALERT "Hello_write, given f_pos now is %lld, internal f_pos is %lld, bye!\n", *f_pos, f->f_pos);
	kfree(buf);
	return s;
}

static int led_freq_init(void)
{
	int result;
	int i;
	led_freq_devices = LED_FREQ_DEVICES;
	my_led_freq_dev.led_freq_data_amount = 0;
	if(led_freq_major){
		my_dev = MKDEV(led_freq_major, led_freq_minor);
		result = register_chrdev_region(my_dev, led_freq_devices, "led_freq");
	} else {
		result = alloc_chrdev_region(&my_dev, led_freq_minor, led_freq_devices, "led_freq");
	}
	if(result < 0){
		printk(KERN_ALERT "Damn it, so wrong! No major number assigned.\n");
		return result;
	}
	led_freq_major = MAJOR(my_dev);
	led_freq_minor = MINOR(my_dev);
	led_freq_setup_cdev(&my_led_freq_dev, 0);
	printk(KERN_ALERT "Hello, world. Major: %i\n", MAJOR(my_dev));

	gpio_free(16);
	my_led_freq_dev.led_fail = gpio_request(16, "testpin");
	if(my_led_freq_dev.led_fail)
		printk(KERN_ALERT "Error, returned %i\n", my_led_freq_dev.led_fail);
	else{
		printk(KERN_ALERT "Success requesting.\n");
		my_led_freq_dev.led_value = 1;
		if(gpio_direction_output(16, my_led_freq_dev.led_value)){
			printk(KERN_ALERT "Fail setting to output.\n");
			my_led_freq_dev.led_fail = 1;
		}
	}
	return 0;
}
static void led_freq_exit(void)
{
	cdev_del(&(my_led_freq_dev.cdev));
	unregister_chrdev_region(my_dev, 1);
	printk(KERN_ALERT "Goodbye, cruel world\n");
	if(!my_led_freq_dev.led_fail)
		gpio_free(16);

}
module_init(led_freq_init);
module_exit(led_freq_exit);
