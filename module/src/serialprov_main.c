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

#define DELAY_TIME 2*HZ

/* Module information */
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");


static struct tty_struct *tty_struct_to_listen;

static struct tty_port *tty_port_to_listen;

struct timer_list listener_timer;


static void tty_listener(struct timer_list *t)
{
	pr_debug("serialprov: running listener");
	
	tty_port_to_listen = tty_struct_to_listen->port;
	
	if (tty_port_to_listen == NULL){
	
		// Pointer is null, port is not open - abort
		
		pr_err("serialprov: error loading tty_port!");
		
	} else {
	
		// Pointer is valid, get data
	
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
	
	char serial_device[] = "tty4";

	dev_t device;	
	
	retval = tty_dev_name_to_number( serial_device, &device );
	
	if (retval == -ENODEV ){
	
		pr_err("serialprov: couldn't open device %s!", serial_device);
		
		goto ret_error;
	} 
	
	pr_debug("serialprov: MAJOR = %d, MINOR = %d", MAJOR(device), MINOR(device) );
	
	tty_struct_to_listen = tty_kopen_exclusive(device);
	
	if (tty_struct_to_listen == NULL){
	
		retval = -1;
	
		pr_err("serialprov: error loading tty_struct!");
		
		goto ret_error;		
	}
	
	timer_setup(&listener_timer, tty_listener, 0);

	listener_timer.expires = jiffies + DELAY_TIME;

	add_timer(&listener_timer);
			
	pr_info("serialprov: Serial Device Provenance module initialized");
	
	return 0;
	
free_tty_struct:

	tty_kclose( tty_struct_to_listen );
	
ret_error:

	return retval;		
}


static void __exit serialprov_exit(void)
{
	pr_debug("serialprov: exiting driver");
	
	int retval;
	
	if ( tty_struct_to_listen != NULL ){
	
		tty_kclose( tty_struct_to_listen );
		
	}
	
	retval = del_timer( &listener_timer );
	
	if (retval != 0 ){
	
		pr_info("Active tty_listener deleted");
	
	}
	
}

module_init(serialprov_init);
module_exit(serialprov_exit);
