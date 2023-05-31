#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/unistd.h>
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h> 

#define BUF_LEN 128

#include "message_slot.h"

static struct msg_slot* msg_slots[257]; 

struct msg_slot_channel {
    int id;
    char* message;
    struct msg_slot_channel* next;
};

struct msg_slot {
    struct msg_slot_channel* active_channel;
    size_t num_of_channels;
    struct msg_slot_channel* head;
    struct msg_slot_channel* last;
};

//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
    int minor = iminor(inode);
    printk("Invoking open");
    if (!msg_slots[minor]) {
        msg_slots[minor] = kmalloc(sizeof(struct msg_slot),GFP_KERNEL);
        msg_slots[minor]->num_of_channels = 0;
        msg_slots[minor]->head = NULL;
        msg_slots[minor]->active_channel = NULL;
        msg_slots[minor]->last = NULL;
    }
    return SUCCESS;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
    int minor = iminor(file->f_inode);
    int flag = 0;
    struct msg_slot_channel* tmp2;
    struct msg_slot_channel* tmp = kmalloc(sizeof(struct msg_slot_channel), GFP_KERNEL);
    if (!tmp) {
        // Handle memory allocation failure
        printk("Memory allocation failed");
        return -ENOMEM;
    }

    printk("Invoking ioctl");
    // check if the ioctl's command is valid 
    if( MSG_SLOT_CHANNEL != ioctl_command_id || ioctl_param == 0) {
      // invalid argument error
      return -EINVAL;
    }
    if (!msg_slots[minor]->num_of_channels) {
        tmp->id = ioctl_param;
        tmp->message = kmalloc(sizeof(char) * BUF_LEN, GFP_KERNEL);
        if (!tmp->message) {
            // Handle memory allocation failure
            printk("Memory allocation failed");
            kfree(tmp);
            return -ENOMEM;
        }
        tmp->next = NULL;
        msg_slots[minor]->head = tmp;
        msg_slots[minor]->last = tmp;
        msg_slots[minor]->num_of_channels++;
        msg_slots[minor]->active_channel = tmp;
    } else {
        tmp2 = msg_slots[minor]->head;
        // check if there already exists a channel with this ID
        while (tmp2 != NULL) {
            if (tmp2->id == ioctl_param) {
                flag = 1;
                printk("Channel exists and was found!");
                msg_slots[minor]->active_channel = tmp2;
                break;    
            }
            tmp2 = tmp2->next;
        }
        if (!flag) { // in-case that there is no channel with the given ID, create one.
            msg_slots[minor]->last->next = kmalloc(sizeof(struct msg_slot_channel), GFP_KERNEL);
            if (!msg_slots[minor]->last->next) {
                // Handle memory allocation failure
                printk("Memory allocation failed");
                kfree(tmp);
                return -ENOMEM;
            }
            tmp->id = ioctl_param;
            tmp->message = kmalloc(sizeof(char) * BUF_LEN, GFP_KERNEL);
            if (!tmp->message) {
                // Handle memory allocation failure
                printk("Memory allocation failed");
                kfree(msg_slots[minor]->last->next);
                kfree(tmp);
                return -ENOMEM;
            }
            tmp->next = NULL;
            msg_slots[minor]->last->next = tmp;
            msg_slots[minor]->last = tmp;
            msg_slots[minor]->num_of_channels++;
            msg_slots[minor]->active_channel = tmp;
        }
    }
    return SUCCESS;
}
//---------------------------------------------------------------
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
    size_t i;
    int minor = iminor(file->f_inode);
    size_t bytes_read = 0;
    printk("Invoking read");
    if (buffer == NULL) {
        printk("Invalid user-space buffer");
        return -EINVAL;
    }
    if (!msg_slots[minor]->active_channel) {
        printk("no channel has been set on the fd");
        return -EINVAL;
    }
    // printk("%zu", strlen(msg_slots[minor]->active_channel->message));
    if (strlen(msg_slots[minor]->active_channel->message) == 0) {
        printk("The message is empty");
        return -EWOULDBLOCK;
    } 

    if (strlen(msg_slots[minor]->active_channel->message) > length) {
        printk("The provided buffer length is too small to hold the message");
        return -ENOSPC;
    }
    for (i = 0; i < BUF_LEN && i < strlen(msg_slots[minor]->active_channel->message); i++)
    {
        put_user(msg_slots[minor]->active_channel->message[i], &buffer[i]);
        bytes_read++;
    }
    return bytes_read;

}

//---------------------------------------------------------------
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
    int minor = iminor(file->f_inode);
    size_t i;
    size_t bytes_written = 0;
    printk("Invoking write");
    if (buffer == NULL) {
        printk("Invalid user-space buffer");
        return -EINVAL;
    }
    if (!msg_slots[minor]->active_channel) {
        printk("no channel has been set on the fd");
        return -EINVAL;
    }
    if (length == 0 || length > 128) {
        printk("The length of the given input is invalid");
        return -EMSGSIZE;
    }
    kfree(msg_slots[minor]->active_channel->message);
    msg_slots[minor]->active_channel->message = kmalloc(sizeof(char) * BUF_LEN, GFP_KERNEL);
    for (i = 0; i < BUF_LEN && i < length; i++)
    {
        get_user(msg_slots[minor]->active_channel->message[i], &buffer[i]);
        bytes_written++;
    }
    return bytes_written;
}

//==================== DEVICE SETUP =============================

struct file_operations Fops = {
    .owner = THIS_MODULE, // Required for correct count of module usage. This prevents the module from being removed while used.
    .open = device_open,
    .read = device_read,
    .write = device_write,
    .unlocked_ioctl = device_ioctl,
};

static int __init simple_init(void)
{
    int major;
    size_t i;
    
    for (i = 0; i < 257; i++)
    {
        msg_slots[i] = NULL;
    }
    
    // Register driver capabilities. Obtain major num
    major = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

    // Negative values signify an error
    if( major < 0 ) {
        printk( KERN_ERR "%s registraion failed for  %d\n", DEVICE_FILE_NAME, major );
        return major;
    }
    printk("Registration is successful");
    return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
    size_t i;
    struct msg_slot_channel* curr;
    struct msg_slot_channel* next;
    struct msg_slot* slot;

    // free all the memory
    for (i = 0; i < 257; i++)
    {   
        if (msg_slots[i] != NULL) {
            slot = msg_slots[i];
            curr = slot->head;
            while(curr != NULL) {
                next = curr->next;
                kfree(curr->message);
                kfree(curr);
                curr = next;
            }
            kfree(slot);
        }
    }
    
    // Unregister the device
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);