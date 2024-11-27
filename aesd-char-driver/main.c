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

#include "aesd_ioctl.h"
#include "aesdchar.h"
#include "aesd-circular-buffer.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("tahasami86"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    PDEBUG("open");
    /**
     * TODO: handle open
     */
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
    struct aesd_dev *dev=NULL;
    size_t read_entry_off;
    ssize_t retval = 0;
    struct aesd_buffer_entry *read_entry;
	ssize_t rc = 0;

    read_entry = NULL ;
    read_entry_off = 0;

    dev = (struct aesd_dev *) filp->private_data;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->driver_lock) != 0)
    {
        retval = -ERESTARTSYS;
        goto cleanup;
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

     if(count > retval){

         count = retval ;
     }

    rc =copy_to_user(buf,(read_entry->buffptr + read_entry_off),count);

        if (rc)
        {
            PDEBUG("copy_to_user %ld", rc);
            retval = -EFAULT;
            goto cleanup;
        }

    // Update return value and file position for next read
    PDEBUG("Successfully read %ld bytes!", rc);
        retval = count;
        *f_pos = *f_pos + count;

    cleanup:
        mutex_unlock((&dev->driver_lock));
        PDEBUG("Read is returning value %ld", retval);
        return retval;
}



static const char *find_newline(const char *buffer,  size_t buffer_size)
{

         size_t i;

         for (i =0 ; i < buffer_size; i++)
         {

            if(buffer[i] == '\n')
            {
                return &buffer[i];

            }

         }
            return NULL;

}



/* 
 * Allocate memory (kmalloc) for each write command and save command in 
 * allocated memory each write command is \n character terminated and any none 
 * \n terminated command will remain and be appended to by future writes only
 * keep track of most recent 10 commands, overwrites should free memory before
 * overwritting command. 
 */


ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = (struct aesd_dev*)filp->private_data;
    size_t remaining_size ;
    //struct aesd_buffer_entry new_entry;
    ssize_t bytes_written = 0, retval = 0;
    const char *eol_ptr=NULL;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    // If the userspace buffer is NULL we can't do anything useful
    if (buf == NULL) {
        retval = -EFAULT;
        goto cleanup;
    }

    if (mutex_lock_interruptible(&dev->driver_lock))
        retval = -ERESTARTSYS;
    
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

    bytes_written = count - copy_from_user((void *)(&dev->temp_entry.buffptr[dev->temp_entry.size]), buf, count);
    dev->temp_entry.size += bytes_written;

    eol_ptr = find_newline(dev->temp_entry.buffptr,count);
    while (eol_ptr) {
        size_t line_length = eol_ptr - dev->temp_entry.buffptr + 1;

        // Create a new entry for the line and add it to the buffer
        struct aesd_buffer_entry new_entry = {
            .buffptr = dev->temp_entry.buffptr,
            .size = line_length,
        };
        aesd_circular_buffer_add_entry(&dev->temp_buffer, &new_entry);

        // Adjust temp_entry for remaining data
        remaining_size = dev->temp_entry.size - line_length;
        if (remaining_size > 0) {
            char *temp_buff = kmalloc(remaining_size, GFP_KERNEL); // Allocate temporary buffer
            if (!temp_buff)
                retval = -ENOMEM;
    
            memmove(temp_buff, dev->temp_entry.buffptr + line_length, remaining_size);
            dev->temp_entry.buffptr = temp_buff; 
            dev->temp_entry.size = remaining_size;
        } else {
            dev->temp_entry.buffptr = NULL;
            dev->temp_entry.size = 0;
        }

        // Look for another newline in the remaining data
        eol_ptr = find_newline(dev->temp_entry.buffptr,dev->temp_entry.size);
    }

    retval = bytes_written;

    cleanup:
        mutex_unlock(&dev->driver_lock);
        return retval;
}



loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entryptr;
    loff_t newpos, errors = 0;
    loff_t fileSize = 0;
    int i;

    // lock device to prevent write from altering filp or writing before llseek
    // finishes.
    if (mutex_lock_interruptible(&dev->driver_lock)) {
        return -ERESTARTSYS;
    }

    AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &dev->temp_buffer, i)
    {
        fileSize += entryptr->size;
    }

    // Determine new position offset value to update file pointer too
    switch(whence) {
        case SEEK_SET:
            // Beginning of device
            newpos = off;
            break;
        case SEEK_CUR:
            // Specific location on device
            newpos = filp->f_pos + off;
            break;
        case SEEK_END:
            // End of device
            newpos = fileSize + off;
            break;
        default:
            // Invalid, shouldn't ever hit this
            errors++;
            newpos = -EINVAL;
    }

    // Check validity of new position, can't be negative or larger than the size
    // of the buffer
    if (newpos < 0) {
        errors++;
        newpos = -EINVAL;
    }
    // Set new file position if there aren't any errors
    if (errors == 0) {
        filp->f_pos = newpos;
    }

    // Unlock mutex now that writes to the device should be valid
    mutex_unlock(&dev->driver_lock);

    return newpos;
}



long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    struct aesd_seekto seek_data;
    struct aesd_buffer_entry *entry;
    long retval=0;
    loff_t loc_off = 0;
     uint32_t command_count = 0;
    int index;
    
    PDEBUG("aesd_ioctl called with cmd: %u\n", cmd);


     if (cmd != AESDCHAR_IOCSEEKTO)
    {
        return -ENOTTY;
    }
    
            if(copy_from_user(&seek_data,(const void __user *)arg, sizeof(seek_data)) != 0)
            {
            PDEBUG("Error copying seekto_data from user space\n");
            retval = -EFAULT;
            }

            if((seek_data.write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) || (seek_data.write_cmd < 0))
            {
                PDEBUG("SEEK_DATA.wrote_cmd is invalid : %u \n",seek_data.write_cmd);
                retval = -EINVAL;
            }

            if(mutex_lock_interruptible(&dev->driver_lock))
            {
                retval = -ERESTARTSYS;
            }  
            
            AESD_CIRCULAR_BUFFER_FOREACH(entry,&dev->temp_buffer,index)
            {

                 if (entry->buffptr != NULL)
                {
                        command_count++;
                }

            }

                if(seek_data.write_cmd > command_count){
                    mutex_unlock(&dev->driver_lock);
                    return -EINVAL;
                }

            command_count = 0;
            AESD_CIRCULAR_BUFFER_FOREACH(entry,&dev->temp_buffer,index)
            {
                if(entry->buffptr != NULL)
                {
                    if(command_count == seek_data.write_cmd)
                    {

                        if (seek_data.write_cmd_offset >= entry->size)
                            {
                                mutex_unlock(&dev->driver_lock);
                                return -EINVAL;
                            }
                        loc_off += seek_data.write_cmd_offset;
                        break;
                    }

                    loc_off += entry->size;
                    command_count++;

                }

            }    
                
            filp->f_pos = loc_off ;
            
            mutex_unlock(&dev->driver_lock);
    
    return 0;


}


struct file_operations aesd_fops = {
    .owner          =     THIS_MODULE,
    .read           =     aesd_read,
    .write          =     aesd_write,
    .open           =     aesd_open,
    .release        =     aesd_release,
    .llseek         =     aesd_llseek,
    .unlocked_ioctl =     aesd_ioctl
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
