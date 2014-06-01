#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#define HELLO_MAJOR	0
#define HELLO_MINOR	0
#define HELLO_DEVICES	2

MODULE_LICENSE("Dual BSD/GPL");

static dev_t my_dev;
static int hello_devices;
static int hello_major = HELLO_MAJOR;
static int hello_minor = HELLO_MINOR;
static int hello_open(struct inode *inode, struct file *filp);
static int hello_release(struct inode *inode, struct file *filp); 
static int hello_read(struct file *f, char __user *u, size_t s, loff_t *l);
static ssize_t hello_write(struct file *f, const char __user *u, size_t s, loff_t *t);
static loff_t hello_llseek(struct file *f, loff_t l, int i);

struct hello_dev{
	struct cdev cdev;
};

static struct hello_dev my_hello_dev;

struct file_operations hello_fops = {
	.owner =	THIS_MODULE,
	.open =		hello_open,
	.read = 	hello_read,
	.write = 	hello_write,
	.release = 	hello_release,
	.llseek =	hello_llseek,
};
static void hello_setup_cdev(struct hello_dev *dev, int index){
	int err, devn = MKDEV(hello_major, hello_minor + index);
	cdev_init(&dev->cdev, &hello_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &hello_fops;
	err = cdev_add(&dev->cdev, devn, 1);
	if(err)
		printk(KERN_ALERT "Oh no! No device added.\n");
}

static int hello_open(struct inode *inode, struct file *filp){
	struct hello_dev *this_dev = container_of(inode->i_cdev, struct hello_dev, cdev);
	filp->private_data = this_dev;
	printk(KERN_ALERT "Whoaa, opened!\n");
	return 0;
}
static int hello_release(struct inode *inode, struct file *filp){
	printk(KERN_ALERT "Whooo... released.\n");
	return 0;
}

static int hello_read(struct file *f, char __user *u, size_t s, loff_t *l){
	printk(KERN_ALERT "Hello_read, size_t: %i, major: %i, minor: %i", s, MAJOR(f->f_inode->i_rdev), MINOR(f->f_inode->i_rdev));
	return 0;
}

static ssize_t hello_write(struct file *f, const char __user *u, size_t s, loff_t *t){
	char *buf = kmalloc(s, GFP_KERNEL); 
	copy_from_user(buf, u, s);
	printk(KERN_ALERT "Hello_write. %s", buf);
	kfree(buf);
	return s;
}


static loff_t hello_llseek(struct file *f, loff_t l, int i){
	return 0;
}
static int hello_init(void)
{
	int result;
	hello_devices = HELLO_DEVICES;
	if(hello_major){
		my_dev = MKDEV(hello_major, hello_minor);
		result = register_chrdev_region(my_dev, hello_devices, "hello");
	} else {
		result = alloc_chrdev_region(&my_dev, hello_minor, hello_devices, "hello");
	}
	if(result < 0){
		printk(KERN_ALERT "Damn it, so wrong! No major number assigned.\n");
		return result;
	}
	hello_major = MAJOR(my_dev);
	hello_minor = MINOR(my_dev);
	hello_setup_cdev(&my_hello_dev, 0);
	hello_setup_cdev(&my_hello_dev, 1);
	printk(KERN_ALERT "Hello, world. Major: %i\n", MAJOR(my_dev));
	return 0;
}
static void hello_exit(void)
{
	cdev_del(&(my_hello_dev.cdev));
	unregister_chrdev_region(my_dev, 2);
	printk(KERN_ALERT "Goodbye, cruel world\n");
}
module_init(hello_init);
module_exit(hello_exit);
