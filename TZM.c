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

#define  DEV_NAME "tzm"    ///< The device will appear at /dev/charDriver using this value
#define  CLASS_NAME  "charDriver"        ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frederic Dlugi and Maximilian Mang");
MODULE_DESCRIPTION("Ein Linux treiber, der Char-Eingaben verarbeitet");  


static DEFINE_MUTEX(lock_mutex);
static DEFINE_MUTEX(io_mutex);
static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static struct class*  charDriverClass  = NULL; ///< The device-driver class struct pointer
static struct device* charDriverDevice = NULL; ///< The device-driver device struct pointer

u64 last_newline = 0;
int char_count = 0;
// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init dev_init(void){
   printk(KERN_INFO "charDriver: Initializing the charDriver LKM\n");

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEV_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "charDriver failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "charDriver: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   charDriverClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(charDriverClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEV_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(charDriverClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "charDriver: device class registered correctly\n");

   // Register the device driver
   charDriverDevice = device_create(charDriverClass, NULL, MKDEV(majorNumber, 0), NULL, DEV_NAME);
   if (IS_ERR(charDriverDevice)){               // Clean up if there is an error
      class_destroy(charDriverClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEV_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(charDriverDevice);
   }
   printk(KERN_INFO "charDriver: device class created correctly\n"); // Made it! device was initialized
   return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit dev_exit(void){
   device_destroy(charDriverClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(charDriverClass);                          // unregister the device class
   class_destroy(charDriverClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEV_NAME);             // unregister the major number
   printk(KERN_INFO "charDriver: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
    int locked = mutex_trylock(&lock_mutex);
    if (locked) {
        printk(KERN_INFO "Device Could not be opened. Already Opened\n");
        return -EBUSY;
    } else {
		char_count = 0;
		last_newline = 0;
        printk(KERN_INFO "Opened\n");
        return 0;
    }
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
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count==0){            // if true then have success
      printk(KERN_INFO "charDriver: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "charDriver: Failed to send %d characters to the user\n", error_count);
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
	int i;
	for(i = 0; i < len; i++)
	{
		char_count++;
		if(buffer[i] == '\n')
		{
			last_newline = get_jiffies_64();
		}
	}
	
  // appending received string with its length
    size_of_message = strlen(message);                 // store the length of the stored message
    printk(KERN_INFO "charDriver: Received %zu characters from the user\n", len);
    return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
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
