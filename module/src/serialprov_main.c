/*
 * VirtualBot TTY driver
 *
 * Copyright (C) 2023 Bruno Policarpo (bruno.freitas@cefet-rj.br)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2 of the License.
 *
 * This driver emulates the behaviour of a serial port by creating 2 pairs of devices:
 * each one connected from one to another. 
 * 
 * Writing on on device will make its content avaliable to be read by the other
 * 
 * It was initially base on Tiny TTY driver from https://github.com/martinezjavier/ldd3/blob/master/tty/tiny_tty.c
 * It does not rely on any backing hardware, but creates a timer that emulates data being received
 * from some kind of hardware.
 */

#include <linux/module.h>

#include <linux/export.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/cdev.h>

#include <linux/uaccess.h>
#include <linux/termios.h>
#include <linux/seq_file.h>

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <linux/circ_buf.h>
#include <linux/string.h>

#include <linux/of.h>

#include <serialprov.h>

#define DRIVER_VERSION "v0.0"
#define DRIVER_AUTHOR "Bruno Policarpo <bruno.freitas@cefet-rj.br>"
#define DRIVER_DESC "SerialProv Driver"

#define DELAY_TIME HZ

/* Module information */
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

// static char buffer[256];


static struct serialprov_dev *serialprov_devices;


ssize_t serialprov_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{


	return 0;


}


int serialprov_open(struct inode *inode, struct file *filp)
{
	struct serialprov_dev *dev; /* device information */

#if 0
	dev = container_of(inode->i_cdev, struct serialprov_dev, cdev);
	
	filp->private_data = dev; /* for other methods */

	/* now trim to 0 the length of the device if open was write-only */
	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;

		scull_trim(dev); /* ignore errors */
		up(&dev->sem);
	}
	
#endif	
	
	
	return 0;          /* success */
}


int serialprov_release(struct inode *inode, struct file *filp)
{
	return 0;
}


static struct file_operations serialprov_fops = {
	.owner   = THIS_MODULE,
//	.llseek =   serialprov_llseek,
	.read =     serialprov_read,
//	.write =    serialprov_write,
	.open =     serialprov_open,
	.release =  serialprov_release
};




static struct tty_struct *tty_struct_to_listen;

static struct tty_port *tty_port_to_listen;

struct timer_list listener_timer;

static int counter = 0;

static void tty_listener(struct timer_list *t)
{
	pr_debug("serialprov: running listener");
	
	// struct tty_buffer *p, *next;	
	
	tty_port_to_listen = tty_struct_to_listen->port;
	
	struct tty_bufhead *buf;
	
	if (tty_port_to_listen == NULL){
	
		// Pointer is null, port is not open - abort
		
		pr_err("serialprov: error loading tty_port!");
		
	} else {
	
		// Pointer is valid, get data
		
		tty_buffer_lock_exclusive (tty_port_to_listen);
		
		buf = &(tty_port_to_listen->buf);
		
		pr_debug( "serialprov: PING %d", counter++ );

		pr_debug("serialprov: buf->head = %p", buf->head );
		
		pr_debug("serialprov: buf->mem_limit = %u", buf->mem_limit );
		
		if (buf->head != NULL){
			pr_debug("serialprov: buf->head->used = %d", buf->head->used );		
			pr_debug("serialprov: buf->head->size = %d", buf->head->size );			
			pr_debug("serialprov: buf->head->commit = %d", buf->head->commit );
			pr_debug("serialprov: buf->head->next = %p", buf->head->next );			
			pr_debug("serialprov: buf->head->read = %d", buf->head->read );
			
			
			u8 *c = char_buf_ptr(buf->head, buf->head->read);
			
			pr_debug("serialprov: buf->head[0] = %c", c[ 0 ] );
			
						
		} else {
			pr_warn("serialprov: buf->head is a NULL pointer!");
		}
				
#if 0	
		pr_debug("serialprov: buf->tail = %p", buf->tail );				
		if (buf->tail != NULL){

			pr_debug("serialprov: buf->tail->used = %d", buf->tail->used );						
			pr_debug("serialprov: buf->tail->size = %d", buf->tail->size );			
			pr_debug("serialprov: buf->tail->commit = %d", buf->tail->commit );			
			pr_debug("serialprov: buf->tail->next = %p", buf->tail->next );			
			pr_debug("serialprov: buf->tail->read = %d", buf->tail->read );
		
			for ( int i =0; i < 254; i++){
						
				pr_debug("serialprov: [debug] buf->tail->data[ %d ] = %lx", 
					i, 
					buf->tail->data[ i ] );
													
			}		
				
		} else {
			pr_warn("serialprov: buf->tail is a NULL pointer!");
		}		
#endif	
		

#if 0
		while ( (p = buf->head) != NULL) {
		
			//pr_debug( "\nsize: %d", p->size );
		
		
			p = p->next;
		}
#endif		

		
		tty_buffer_unlock_exclusive (tty_port_to_listen);
	}

	/* resubmit the timer again */		
	timer_setup(t, tty_listener, 0);

	t->expires = jiffies + DELAY_TIME;

	add_timer(t);
}



static int __init serialprov_init(void)
{
	pr_debug("serialprov: Serial Device Provenance init");
	
	int retval;
	
	tty_struct_to_listen = NULL;
	tty_port_to_listen = NULL;	
	
	char serial_device[] = "ttyUSB1";

	dev_t devno_to_listen, userspace_devno;	
	
	retval = tty_dev_name_to_number( serial_device, &devno_to_listen );
	
	if (retval == -ENODEV ){
	
		pr_err("serialprov: couldn't open device %s!", serial_device);
		
		goto ret_error;
	} 
	
	pr_debug("serialprov: MAJOR = %d, MINOR = %d", 
		MAJOR(devno_to_listen), 
		MINOR(devno_to_listen) );
	
	tty_struct_to_listen = tty_kopen_shared(devno_to_listen);
	
	if (tty_struct_to_listen == NULL){
	
		retval = -1;
	
		pr_err("serialprov: error loading tty_struct!");
		
		goto ret_error;		
	}
	
	retval = alloc_chrdev_region( &userspace_devno, 
		0, 
		1,
		"serialprov");
		
	if (retval != 0 ){
	
		pr_err("serialprov: couldn't allocate userspace char device!");
	
		goto free_tty_struct;	
	
	}	
	
	// Alocacao do arquivo especial
	serialprov_devices = kmalloc( sizeof(struct serialprov_dev) , GFP_KERNEL );
	
	if (!serialprov_devices){
	
		pr_err("serialprov: error allocating serialprov device!");
	
		goto free_chr_device;
	}
	
	serialprov_devices->devno = userspace_devno;
	
	cdev_init(&serialprov_devices->cdev, &serialprov_fops);
	
	serialprov_devices->cdev.owner = THIS_MODULE;
	retval = cdev_add(&serialprov_devices->cdev, userspace_devno, 1);
	
	if (retval){
		pr_err("serialprov: error allocating ");
		
		goto free_serialprov_dev;
	
	}
	
	
	// Timer eh a ultima coisa a instanciar
	timer_setup(&listener_timer, tty_listener, 0);

	listener_timer.expires = jiffies + DELAY_TIME;

	add_timer(&listener_timer);
			
	pr_info("serialprov: Serial Device Provenance module initialized");
	
	return 0;
	
free_serialprov_dev:

	kfree( serialprov_devices );
	
free_chr_device:

	unregister_chrdev_region( userspace_devno, 1 );

free_tty_struct:

	tty_kclose( tty_struct_to_listen );
	
ret_error:

	return retval;		
}


static void __exit serialprov_exit(void)
{
	pr_debug("serialprov: exiting driver");
	
	int retval;
	
	retval = del_timer( &listener_timer );
	
	if (retval != 0 ){
	
		pr_info("serialprov: Active tty_listener deleted");
	
	}
	
	
	if ( tty_struct_to_listen != NULL ){
		
		tty_kclose( tty_struct_to_listen );
		
		pr_info("serialprov: Closing openned tty device");
		
	}
	
	cdev_del(&serialprov_devices->cdev);
	
	pr_debug("serialprov: cdev_del");	
	
	unregister_chrdev_region(serialprov_devices->devno, 1);
	
	pr_debug("serialprov: unregister_chrdev_region");		
	
	kfree( serialprov_devices );
	
	
}

module_init(serialprov_init);
module_exit(serialprov_exit);
