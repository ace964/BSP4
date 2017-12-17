/**
 * @file   charDriver.c
 * @author Frederic Dlugi and Maximilian Mang
 * @date   16.12.2017
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <asm/uaccess.h>          // Required for the copy to user function
#include <linux/mutex.h>          // Mutex
#include <linux/jiffies.h>
#include <linux/math64.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frederic Dlugi and Maximilian Mang");
MODULE_DESCRIPTION("Ein Linux treiber, der Char-Eingaben verarbeitet");  


static DEFINE_MUTEX(lock_mutex);
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored

unsigned long ret_val_time = 0;
unsigned long time_since_last_newline = 0;
int ret_val_number = 0;
int dev_open = 0;
module_param(ret_val_time, long, S_IRUGO);
MODULE_PARM_DESC(ret_val_time, "Zeit zwischen zwei newlines, wenn keine Messung bekannt ist");
module_param(ret_val_number, int, S_IRUGO);
MODULE_PARM_DESC(ret_val_number, "Laenge zwischen zwei newline, wenn keine Messung bekannt ist");


// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);


/** 
 * 
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** 
 * 
 */
dev_t dev_number;
struct cdev *driver_object;
static int __init dev_init(void)
{
	alloc_chrdev_region(&dev_number, 0, 1, "tzm");
	driver_object = cdev_alloc();
    driver_object->ops = &fops;
    driver_object->owner = "tzm";
	cdev_add(driver_object, dev_number, 1);
    return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit dev_exit(void)
{
    cdev_del(driver_object);
	unregister_chrdev_region(dev_number, 1);
    printk(KERN_INFO "charDriver: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
    if (mutex_lock_interruptible(&lock_mutex)) 
	{
        printk(KERN_INFO "Device Could not be opened. Already Opened\n");
        return -EBUSY;
    }
	if(dev_open)
	{
		return -EBUSY;
	}
	
	dev_open = 1;
	ret_val_number = 0;
	ret_val_time = 0;
    printk(KERN_INFO "Opened\n");
	mutex_unlock(&lock_mutex);
    return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
	if(mutex_lock_interruptible(&lock_mutex))
        return 0; // nothing read

	
	int error_count = 0;
	// copy_to_user has the format ( * to, *from, size) and returns 0 on success
	error_count = copy_to_user(buffer, message, size_of_message);

	if (error_count==0){            // if true then have success
		printk(KERN_INFO "charDriver: Sent %d characters to the user\n", size_of_message);
		mutex_unlock(&access_mutex);
		return (size_of_message=0);  // clear the position to the start and return 0
	}
	else {
		printk(KERN_INFO "charDriver: Failed to send %d characters to the user\n", error_count);
		mutex_unlock(&access_mutex);
		return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
	}
	
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
	
	if(mutex_lock_interruptible(&lock_mutex))
        return 0; // nothing copied
	
	
	int i;
	for(i = 0; i < len; i++)
	{
		ret_val_number++;
		if(buffer[i] == '\n')
		{
			if(ret_val_time != -1)
			{
				time_since_last_newline = get_jiffies_64() - ret_val_time;
			}
			ret_val_time = get_jiffies_64();
		}
	}
	
	sprintf(message,"Time =%d Chars=%d",time_since_last_newline, ret_val_number);
	// appending received string with its length
    size_of_message = strlen(message);                 // store the length of the stored message
    printk(KERN_INFO "charDriver: Received %zu characters from the user\n", len);
	mutex_unlock(&lock_mutex);
    return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
	if (mutex_lock_interruptible(&lock_mutex)) 
	{
        return -EBUSY;
    }
	dev_open = 0;
	mutex_unlock(&lock_mutex);
	printk(KERN_INFO "Device successfully closed\n");
	return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(dev_init);
module_exit(dev_exit);
