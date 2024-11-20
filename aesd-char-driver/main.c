/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>		// kmalloc()
#include <linux/string.h>

#include "aesdchar.h"
#include "aesd-circular-buffer.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("tahasami86"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev,struct aesd_dev,cdev);

    filp->private_data = dev ;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_buffer_entry *read_entry = NULL;;
    struct aesd_dev *dev=NULL;
    size_t read_entry_off = 0;
	ssize_t rc = 0;

    dev = (struct aesd_dev *) filp->private_data;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->driver_lock) != 0)
    {
        return -ERESTARTSYS;
    }
    /**
     * TODO: handle read
     */

    read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->temp_buffer),*f_pos,&read_entry_off);

     if(read_entry == NULL){
        retval = 0;
		*f_pos = 0;
        goto cleanup;
     }

     retval = read_entry->size - read_entry_off ;

     if(count < retval){

        retval = count ;
     }

     rc = copy_to_user(buf,(read_entry->buffptr + read_entry_off),retval);

        if (rc)
        {
            PDEBUG("copy_to_user %ld", rc);
            retval = -EFAULT;
            goto cleanup;
        }

        *f_pos += retval;



cleanup:
mutex_unlock(&dev->driver_lock);
return retval;

}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    ssize_t bytes_written = 0, retval = 0;
    const char *newline_ptr = NULL;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    /**
     * TODO: handle write
     */

    if (mutex_lock_interruptible(&dev->driver_lock))
        return -ERESTARTSYS;

    // Allocate or expand the temporary buffer
    if (!dev->temp_entry.buffptr) {
        dev->temp_entry.buffptr = kmalloc(count, GFP_KERNEL);
        if (!dev->temp_entry.buffptr) {
            retval = -ENOMEM;
            goto cleanup;
        }
        dev->temp_entry.size = 0;
    } else {
        char *new_buffptr = krealloc(dev->temp_entry.buffptr, dev->temp_entry.size + count, GFP_KERNEL);
        if (!new_buffptr) {
            retval = -ENOMEM;
            goto cleanup;
        }
        dev->temp_entry.buffptr = new_buffptr;
    }

    // Copy data from user space to the temporary buffer
    bytes_written = count - copy_from_user((void *)(&dev->temp_entry.buffptr[dev->temp_entry.size]), buf, count);
    dev->temp_entry.size += bytes_written;

    // Check if the newline character is present
    newline_ptr = memchr(dev->temp_entry.buffptr, '\n', dev->temp_entry.size);
    while (newline_ptr) {
        size_t line_length = newline_ptr - dev->temp_entry.buffptr + 1;

        // Create a new entry for the line and add it to the buffer
        struct aesd_buffer_entry new_entry = {
            .buffptr = dev->temp_entry.buffptr,
            .size = line_length,
        };
        aesd_circular_buffer_add_entry(&dev->temp_buffer, &new_entry);

        // Adjust temp_entry for remaining data
        size_t remaining_size = dev->temp_entry.size - line_length;
        if (remaining_size > 0) {
            memmove(dev->temp_entry.buffptr, dev->temp_entry.buffptr + line_length, remaining_size);
            dev->temp_entry.size = remaining_size;
        } else {
            dev->temp_entry.buffptr = NULL;
            dev->temp_entry.size = 0;
        }

        // Look for another newline in the remaining data
        newline_ptr = memchr(dev->temp_entry.buffptr, '\n', dev->temp_entry.size);
    }

    retval = bytes_written;

    cleanup:
    mutex_unlock(&dev->driver_lock);
    return retval;


}



struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    aesd_circular_buffer_init(&aesd_device.temp_buffer);

    mutex_init(&aesd_device.driver_lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    aesd_circular_buffer_free(&aesd_device.temp_buffer);
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
