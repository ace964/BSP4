/**
 * @file   tzm.c
 * @author Frederic Dlugi and Maximilian Mang
 * @date   16.12.2017
 */

#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/mutex.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frederic Dlugi and Maximilian Mang");
MODULE_DESCRIPTION("Ein Linux treiber, der Char-Eingaben verarbeitet");  


static DEFINE_MUTEX(lock_mutex);
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored

unsigned long ret_val_time = 0;
unsigned long timeStampOfLastNL = 0;
int ret_val_number = 0;
module_param(ret_val_time, long, S_IRUGO);
MODULE_PARM_DESC(ret_val_time, "Zeit zwischen zwei newlines, wenn keine Messung bekannt ist");
module_param(ret_val_number, int, S_IRUGO);
MODULE_PARM_DESC(ret_val_number, "Laenge zwischen zwei newline, wenn keine Messung bekannt ist");

//flag to ensure only one opening
static bool isOpen = false;

// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);


/** 
 * Map the method pointer to file_operations struct
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** 
 * Init method for driver 
 * allocates a dev_number in the kernel
 * allocates a driverobject and sets ops and owner.
 * Adds driver to kernel.
 */
dev_t dev_number;
struct cdev *driver_object;
static int __init dev_init(void)
{
	alloc_chrdev_region(&dev_number, 0, 1, "tzm");
	driver_object = cdev_alloc();
    driver_object->ops = &fops;
    driver_object->owner = THIS_MODULE;
	cdev_add(driver_object, dev_number, 1);
    return 0;
}

/** 
 * deletes driver object and unregisters dev number.
 */
static void __exit dev_exit(void)
{
    cdev_del(driver_object);
	unregister_chrdev_region(dev_number, 1);
}

/** 
 * Opens the driver device.
 * @return
 * 0 if successfull and -EBUSY if not successfull.
 */
static int dev_open(struct inode *inodep, struct file *filep){
    if (mutex_lock_interruptible(&lock_mutex)) 
	{
        return -EBUSY;
    }
    if(isOpen)
	{
        return -EBUSY;
    }
    
    isOpen = true;
	ret_val_number = 0;
	ret_val_time = 0;
	mutex_unlock(&lock_mutex);
    return 0;
}

/** 
 * Reads the current status of the driver.
 * @return
 * the length of the copied message
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
	int error_count = 0;
    if(mutex_lock_interruptible(&lock_mutex))
        return 0; // nothing read
	
	// copy_to_user has the format ( * to, *from, size) and returns 0 on success
	error_count = copy_to_user(buffer, message, size_of_message);

	if (error_count == 0){            // if true then have success
		mutex_unlock(&lock_mutex);
		return (size_of_message);  // return size_of_message
	}
	else {
		mutex_unlock(&lock_mutex);
		return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
	}
	
}

/**
 * Writes a string to the device.
 * @return
 * the length of the copied string.
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
	int i; // Initialized at top because of 
	if(mutex_lock_interruptible(&lock_mutex))
        return 0; // nothing copied
	
	
	
	for(i = 0; i < len; i++)
	{
		ret_val_number++;
		if(buffer[i] == '\n')
		{
			if(timeStampOfLastNL != -1)
			{
				ret_val_time = get_jiffies_64() - timeStampOfLastNL;
			}
			timeStampOfLastNL = get_jiffies_64();
		}
	}
	
	sprintf(message,"Time =%lu Chars=%d \n",ret_val_time, ret_val_number);
    size_of_message = strlen(message);                 // store the length of the stored message
	mutex_unlock(&lock_mutex);
    return len;
}

/** 
 * Releases the driver object.
 * @return
 * 0 if successfull else -EBUSY
 */
static int dev_release(struct inode *inodep, struct file *filep){
	if (mutex_lock_interruptible(&lock_mutex)) 
	{
        return -EBUSY;
    }
	isOpen = false;
	mutex_unlock(&lock_mutex);
	printk(KERN_INFO "Device successfully closed\n");
	return 0;
}

/** 
 * sets module init and exit functions
 */
module_init(dev_init);
module_exit(dev_exit);
