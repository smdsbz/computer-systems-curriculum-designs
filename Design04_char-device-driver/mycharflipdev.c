#include <linux/init.h>     // (macros) __init, __exit
#include <linux/kernel.h>   // required by kernel-oriented developments
#include <linux/types.h>    // various typedefs, just in case
#include <linux/stddef.h>   // conventional macros like NULL
#include <linux/slab.h>     // kmalloc() and kfree()
#include <linux/module.h>   // required by all module source
#include <linux/device.h>   // class_create/destroy(), device_create/destroy()
#include <linux/cdev.h>     // struct cdev
#include <linux/fs.h>       // struct file_operations,
                            // unregister/register_chrdev_region()
#include <linux/string.h>   // memset()
#include <linux/uaccess.h>  // copy_to_user(), copy_from_user()

/*
 * TODO:
 *  -   Thread-safe with mutex
 */

/******* Module Discriptions *******/

#define DEVICE_NAME     "mycharflip"
#define CLASS_NAME      "mychar"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xiaoguang Zhu (U201610136)");
MODULE_DESCRIPTION("Flips alphabetic characters to their uppercase / "
        "lowercase siblings.");
MODULE_VERSION("0.1");


/******* Module Settings *******/

// number of devices to be created, here we only create one for pleasure
#define MYCHARFLIP_DEV_NUM      3

// uncomment the following line to enable verbose mode
#define MYCHARFLIP_DEBUG


/******* Module Static Data Space *******/

static dev_t devno = MKDEV(0, 0);   // identifier of the first device of this module
static struct class *class = NULL;  // device class struct pointer
static int opening_count = 0;       // keep track of opened sessions


/******* Device Private Data Struct *******/

struct mycharflip_data_t {
    struct cdev    *cdev;
    struct device  *device;
    char           *last_input;     // pointer to last user input data, NULL if no input yet
} *priv_data_arr = NULL;


/******* Device Operation Handlers *******/

static int mycharflip_open(struct inode *, struct file *);
static int mycharflip_release(struct inode *, struct file *);
static ssize_t mycharflip_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t mycharflip_write(struct file *, const char __user *, size_t,
                                loff_t *);

const struct file_operations mycharflip_file_operations = {
    .owner      = THIS_MODULE,
    .open       = mycharflip_open,
    .release    = mycharflip_release,
    .read       = mycharflip_read,
    .write      = mycharflip_write
};


static int mycharflip_open(struct inode *inode, struct file *file) {    // {{{
    int devno = inode->i_cdev->dev;
    // bind file->private_data to private data space
    file->private_data = &priv_data_arr[MINOR(devno)];
    ++opening_count;
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: [%d:%d] Opened device...\n", __func__,
            MAJOR(devno), MINOR(devno));
#endif
    return 0;
}   // }}}

static int mycharflip_release(struct inode *inode, struct file *file) {     // {{{
    struct mycharflip_data_t *priv_data = file->private_data;
    --opening_count;
    // NOTE: Keep data!!!
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: [%d:%d] Closed device...\n", __func__,
            MAJOR(priv_data->cdev->dev), MINOR(priv_data->cdev->dev));
#endif
    return 0;
}   // }}}

static ssize_t mycharflip_read(struct file *file, char __user *user_buf,
                                size_t count, loff_t *offset) {             // {{{
    struct mycharflip_data_t *priv_data;
    long retval;
    char result;
    priv_data = file->private_data;
    if (unlikely(priv_data == NULL)) {
        printk(KERN_ALERT "%s: No data space attached to module!\n", __func__);
        return -EFAULT;
    }
    if (unlikely(priv_data->last_input == NULL)) {
        printk(KERN_ALERT "%s: No data space attached to device %d:%d!\n",
                __func__, MAJOR(priv_data->cdev->dev),
                MINOR(priv_data->cdev->dev));
        return -EFAULT;
    }
    // check range
    if (*offset >= 1) {     // if reading is done
        return 0;
    }
    // TODO: Mutex lock
    if (*(priv_data->last_input) >= 65 && *(priv_data->last_input) <= 90) {     // if is uppercase letter
        result = *(priv_data->last_input) - 'A' + 'a';
    } else if (*(priv_data->last_input) >= 97
                && *(priv_data->last_input) <= 122) {   // if is lowercase letter
        result = *(priv_data->last_input) - 'a' + 'A';
    } else {
        result = *(priv_data->last_input);
    }
    // copy to user buffer
    retval = copy_to_user(user_buf, &result, 1);
    if (unlikely(retval != 0)) {
        printk(KERN_ALERT "%s: copy_to_user() failed!\n", __func__);
        return -EFAULT;
    }
    // modify offset
    *offset += 1;
    // TODO: Mutex unlock
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: Read '%c' from device %d:%d!\n", __func__,
            result, MAJOR(priv_data->cdev->dev), MINOR(priv_data->cdev->dev));
#endif
    return 1;
}   // }}}

static ssize_t mycharflip_write(struct file *file, const char __user *user_buf,
                                size_t count, loff_t *offset) {                     // {{{
    struct mycharflip_data_t *priv_data;
    long retval;
    priv_data = file->private_data;
    if (unlikely(priv_data == NULL)) {
        printk(KERN_ALERT "%s: No data space attached to module!\n", __func__);
        return -EFAULT;
    }
    // check range
    if (*offset >= 1) {     // if buffer, now single char, is full
        return 0;
    }
    // TODO: Mutex lock
    // allocate space if not previous writes
    if (unlikely(priv_data->last_input == NULL)) {
        priv_data->last_input = kmalloc(1, GFP_KERNEL);
        if (unlikely(priv_data->last_input == NULL)) {
            printk(KERN_ALERT "%s: Failed to allocate space for private "
                    "data!\n", __func__);
            return -ENOMEM;
        }
    }
    // copy from user buffer, only first char copied, other ignored for now
    retval = copy_from_user(priv_data->last_input, user_buf, 1);
    if (unlikely(retval != 0)) {
        printk(KERN_ALERT "%s: copy_from_user() failed!\n", __func__);
        return -EFAULT;
    }
    // modify offset
    *offset += 1;
    // TODO: Mutex unlock
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: Wrote '%c' to device %d:%d!\n", __func__,
            *(priv_data->last_input), MAJOR(priv_data->cdev->dev),
            MINOR(priv_data->cdev->dev));
#endif
    return 1;
}   // }}}


/******* Module Context Management Functions *******/

static int __init mycharflip_init(void) {   // {{{
    long retval;    // kernel api return
    int idx;        // loop var
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: Initializing...\n", __func__);
#endif
    // allocate device private data space
    priv_data_arr =
        kmalloc(MYCHARFLIP_DEV_NUM * sizeof(struct mycharflip_data_t),
                GFP_KERNEL);
    if (unlikely(priv_data_arr == NULL)) {
        printk(KERN_ALERT "%s: Failed to allocate space for private data!\n",
                __func__);
        return -ENOMEM;
    }
    memset(priv_data_arr, 0,
            MYCHARFLIP_DEV_NUM * sizeof(struct mycharflip_data_t));
    // register device class
    class = class_create(THIS_MODULE, CLASS_NAME);
    if (unlikely(IS_ERR(class))) {
        printk(KERN_ALERT "%s: Failed to register class, returned %ld\n",
                __func__, (long)class);
        retval = (long)class;
        class = NULL;
        goto __JUMPSIGN_mycharflip_init_failed_on_registering_class;
    }
    // register device regions, get major identifier for this batch
    retval = alloc_chrdev_region(&devno, 0, MYCHARFLIP_DEV_NUM, DEVICE_NAME);
    if (unlikely(retval != 0)) {
        printk(KERN_ALERT "%s: Failed to register device region, returned "
                "%ld.\n", __func__, retval);
        goto __JUMPSIGN_mycharflip_init_failed_on_registering_devno;
    }
    // create device nodes, one by one
    for (idx = 0; idx != MYCHARFLIP_DEV_NUM; ++idx) {
        // initialize clean cdev structure and attach function entries
        priv_data_arr[idx].cdev = cdev_alloc();
        if (unlikely(priv_data_arr[idx].cdev == NULL)) {
            printk(KERN_ALERT "%s: Failed allocate memory for cdev!\n",
                    __func__);
            retval = -ENOMEM;
            goto __JUMPSIGN_mycharflip_init_failed_on_creating_device;
        }
        cdev_init(priv_data_arr[idx].cdev, &mycharflip_file_operations);
        priv_data_arr[idx].cdev->dev = MKDEV(MAJOR(devno), MINOR(devno) + idx);
        // create device
        priv_data_arr[idx].device =
            device_create(class, NULL, priv_data_arr[idx].cdev->dev, NULL,
                            "%s%u", DEVICE_NAME, idx);
        if (unlikely(IS_ERR(priv_data_arr[idx].device))) {
            printk(KERN_ALERT "%s: Failed on device_create()!\n", __func__);
            retval = PTR_ERR(priv_data_arr[idx].device);
            priv_data_arr[idx].device = NULL;
            goto __JUMPSIGN_mycharflip_init_failed_on_creating_device;
        }
        retval = cdev_add(priv_data_arr[idx].cdev,
                            priv_data_arr[idx].cdev->dev, 1);
        if (unlikely(retval < 0)) {
            printk(KERN_ALERT "%s: Failed on cdev_add(), returned %ld\n",
                    __func__, retval);
            goto __JUMPSIGN_mycharflip_init_failed_on_creating_device;
        }
    }
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: Successfully registered devices with major "
            "identifier %d!\n", __func__, MAJOR(devno));
#endif
    return 0;   // 0 means healthy
__JUMPSIGN_mycharflip_init_failed_on_creating_device:
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: In __JUMPSIGN_mycharflip_init_failed_on_creating_de"
            "vice routine\n", __func__);
#endif
    for (; idx != -1; --idx) {
        if (likely(priv_data_arr[idx].device != NULL)) {
            device_destroy(class, priv_data_arr[idx].cdev->dev);    // NOTE: it finds the device struct internally, DON'T kfree()!
            priv_data_arr[idx].device = NULL;
        }
        if (likely(priv_data_arr[idx].cdev != NULL)) {
            cdev_del(priv_data_arr[idx].cdev);      // NOTE: reference count kept by kernel, on 0, it is freed
            priv_data_arr[idx].cdev = NULL;
        }
    }
    unregister_chrdev_region(devno, MYCHARFLIP_DEV_NUM);
__JUMPSIGN_mycharflip_init_failed_on_registering_devno:
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: In __JUMPSIGN_mycharflip_init_failed_on_registering"
            "_devno routine\n", __func__);
#endif
    class_destroy(class);
    class = NULL;
__JUMPSIGN_mycharflip_init_failed_on_registering_class:
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: In __JUMPSIGN_mycharflip_init_failed_on_registering"
            "_class routine\n", __func__);
#endif
    kfree(priv_data_arr);
    priv_data_arr = NULL;
    return (int)retval;
}   // }}}

static void __exit mycharflip_cleanup(void) {   // {{{
    int idx;    // loop var
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: Cleaning up...\n", __func__);
#endif
    if (opening_count != 0) {
        printk(KERN_ALERT "%s: There're still %d processes are using devices "
                "of this module! Unregistering them anyway!\n", __func__,
                opening_count);
    }
    // release memory space
    if (likely(priv_data_arr != NULL)) {
        // free private pointers
        for (idx = 0; idx != MYCHARFLIP_DEV_NUM; ++idx) {
            if (likely(priv_data_arr[idx].last_input != NULL)) {
                kfree(priv_data_arr[idx].last_input);
            }
            if (likely(priv_data_arr[idx].device != NULL)) {
                device_destroy(class, priv_data_arr[idx].cdev->dev);
            }
            if (likely(priv_data_arr[idx].cdev != NULL)) {
                cdev_del(priv_data_arr[idx].cdev);
            }
        }
        // free the entire device data structure
        kfree(priv_data_arr);
    } else {
        printk(KERN_ALERT "%s: No previous allocated data space found!\n",
                __func__);
    }
    // unregister device identifiers
    unregister_chrdev_region(devno, MYCHARFLIP_DEV_NUM);
    // unregister class
    class_destroy(class);
#if defined(MYCHARFLIP_DEBUG)
    printk(KERN_DEBUG "%s: Successfully unregistered devices!\n", __func__);
#endif
    return;
}   // }}}


module_init(mycharflip_init);
module_exit(mycharflip_cleanup);

