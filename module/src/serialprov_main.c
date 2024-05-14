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

/* Module information */
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");



#if 0
static int __init virtualbot_init(void)
{
	int retval;
	unsigned i;

	/* allocate the tty driver */
	//virtualbot_tty_driver = alloc_tty_driver(virtualbot_TTY_MINORS);

	/*
	 * Initializing the VirtialBot driver
	 * 
	 * This is the main hardware emulation device
	 * 
	*/

	virtualbot_tty_driver = tty_alloc_driver( VIRTUALBOT_MAX_TTY_MINORS, \
		TTY_DRIVER_REAL_RAW );

	if (!virtualbot_tty_driver)
		return -ENOMEM;

	/* initialize the tty driver */
	virtualbot_tty_driver->owner = THIS_MODULE;
	virtualbot_tty_driver->driver_name = VIRTUALBOT_DRIVER_NAME;
	virtualbot_tty_driver->name = VIRTUALBOT_TTY_NAME;
	virtualbot_tty_driver->major = VIRTUALBOT_TTY_MAJOR,
	virtualbot_tty_driver->minor_start = 0,
	virtualbot_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	virtualbot_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	virtualbot_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	virtualbot_tty_driver->init_termios = tty_std_termios;
	virtualbot_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	virtualbot_tty_driver->init_termios.c_iflag = 0;
	virtualbot_tty_driver->init_termios.c_oflag = 0;
	
	// Must be 0 to disable ECHO flag
	virtualbot_tty_driver->init_termios.c_lflag = 0;
	
	//virtualbot_tty_driver->init_termios = tty_std_termios;
	//virtualbot_tty_driver->init_termios.c_cflag = B38400 | CS8 | CREAD;
	//virtualbot_tty_driver->init_termios.c_lflag = 0;
	//virtualbot_tty_driver->init_termios.c_ispeed = 38400;
	//virtualbot_tty_driver->init_termios.c_ospeed = 38400;

	tty_set_operations(virtualbot_tty_driver, 
		&virtualbot_serial_ops);

	pr_debug("virtualbot: set operations");

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		tty_port_init( &virtualbot_tty_port[ i ]);
		pr_debug("virtualbot: port %i initiliazed", i);

		tty_port_register_device( &virtualbot_tty_port[ i ], 
			virtualbot_tty_driver, 
			i, 
			NULL);
		pr_debug("virtualbot: port %i linked", i);

		virtualbot_table[ i ] = NULL;
		
		mutex_init( &virtualbot_lock[ i ] );
	}

	/* register the tty driver */
	retval = tty_register_driver(virtualbot_tty_driver);

	if (retval) {
		pr_err("virtualbot: failed to register virtualbot tty driver");

		tty_driver_kref_put(virtualbot_tty_driver);

		return retval;
	}
	
	//pr_info("virtualbot: driver initialized (" DRIVER_DESC " " DRIVER_VERSION  ")" );

	/*
	 * 
	 * Initializing the VirtualBot Commander
	 * 
	 * This is the main interface between
	 * 
	*/

	vb_comm_tty_driver = tty_alloc_driver( VIRTUALBOT_MAX_TTY_MINORS,
		TTY_DRIVER_REAL_RAW );

	if (!vb_comm_tty_driver){
		retval = -ENOMEM;
	}

	/* initialize the driver */
	vb_comm_tty_driver->owner = THIS_MODULE;
	vb_comm_tty_driver->driver_name = VB_COMM_DRIVER_NAME;
	vb_comm_tty_driver->name = VB_COMM_TTY_NAME;
	vb_comm_tty_driver->major = VB_COMM_TTY_MAJOR,
	vb_comm_tty_driver->minor_start = 0,
	vb_comm_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	vb_comm_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	vb_comm_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	vb_comm_tty_driver->init_termios = tty_std_termios;
	vb_comm_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	vb_comm_tty_driver->init_termios.c_iflag = 0;
	vb_comm_tty_driver->init_termios.c_oflag = 0;	
	
	// Must be 0 to disable ECHO flag
	vb_comm_tty_driver->init_termios.c_lflag = 0;	

	tty_set_operations(vb_comm_tty_driver, 
		&vb_comm_serial_ops);

	pr_debug("vb-comm: set operations");

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		tty_port_init(& vb_comm_tty_port[ i ] );
		pr_debug("vb-comm: port %i initiliazed", i);

		tty_port_register_device( & vb_comm_tty_port[ i ], 
			vb_comm_tty_driver, 
			i, 
			NULL);
		pr_debug("vb-comm: port %i linked", i);

		// Port state is initially NULL
		vb_comm_table[ i ] = NULL;
		
		mutex_init( &vb_comm_lock[ i ] );
	}


	/* register the tty driver */
	retval = tty_register_driver(vb_comm_tty_driver);

	if (retval) {

		pr_err("vb-comm: failed to register vb-comm tty driver");

		tty_driver_kref_put(vb_comm_tty_driver);

		return retval;
	}

	for ( i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++ ){
		mutex_init( &virtualbot_global_port_lock[ i ] );
	}

	pr_info("Serial Port Emulator initialized (" DRIVER_DESC " " DRIVER_VERSION  ")" );

	return retval;
}

static void __exit virtualbot_exit(void)
{
	struct virtualbot_serial *virtualbot;
	struct vb_comm_serial *vb_comm;
	int i;

	// struct list_head *pos, *n;

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; ++i) {
		
		tty_unregister_device(virtualbot_tty_driver, i);
		
		pr_debug("virtualbot: device %d unregistered" , i);

		tty_port_destroy(virtualbot_tty_port + i);

		pr_debug("virtualbot: port %i destroyed", i);
	}

	tty_unregister_driver(virtualbot_tty_driver);

	tty_driver_kref_put(virtualbot_tty_driver);

	pr_debug("virtualbot: driver unregistered");

	/* shut down all of the timers and free the memory */
	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		virtualbot = virtualbot_table[i];

		pr_debug("virtualbot: freeing VB %i", i);

		if (virtualbot) {
			/* close the port */
			while (virtualbot->open_count)
				do_close(virtualbot);

			/* shut down our timer and free the memory */
			del_timer(&virtualbot->timer);

			/* deallocate receive buffer */
			kfree( virtualbot->recv_buffer.buf );
			
			/* destroy structure mutex */
			mutex_destroy( &virtualbot_lock[ i ] );
			
			kfree(virtualbot);
			virtualbot_table[i] = NULL;
		}

	}

	/**
	 * 
	 * Unregistering The Comm part 
	 * 
	*/

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; ++i) {

		tty_unregister_device(vb_comm_tty_driver, i);
		
		pr_debug("vb-comm: device %d unregistered" , i);

		tty_port_destroy(vb_comm_tty_port + i);

		pr_debug("vb-comm: port %i destroyed", i);
	}

	tty_unregister_driver(vb_comm_tty_driver);

	tty_driver_kref_put(vb_comm_tty_driver);

	// For future
	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		vb_comm = vb_comm_table[i];

		pr_debug("vb-comm: freeing VB %i", i);

		if (vb_comm) {
			/* close thvirtualbote port */
			while (vb_comm->open_count)
				vb_comm_do_close(vb_comm);
				
			mutex_destroy( &vb_comm_lock[ i ] );

			/* shut down our timer and free the memory */
			//del_timer(&virtualbot->timer);
			kfree(vb_comm);
			vb_comm_table[i] = NULL;
		}
	}

	mutex_destroy( &virtualbot_global_port_lock[ i ] );
}
#endif

static struct tty_struct *tty_to_listen;

static int __init serialprov_init(void)
{
	pr_debug("serialprov: Serial Device Provenance init");
	
	int retval;
	
	tty_to_listen = NULL;
	
	char serial_device[] = "tty25";

	dev_t device;	
	
	retval = tty_dev_name_to_number( serial_device, &device );
	
	if (retval == -ENODEV ){
		pr_err("serialprov: couldn't open device %s!", serial_device);
		
		goto ret_error;
	}
	
	tty_to_listen = tty_kopen_shared(device);
	
	if (tty_to_listen == NULL){
	
		retval = -1;
	
		pr_err("serialprov: error loading tty_struct!");
		
		goto ret_error;		
	}
	
	pr_info("serialprov: Serial Device Provenance module initialized");
	
	return 0;
	
ret_error:

	return retval;		
}


static void __exit serialprov_exit(void)
{
	pr_debug("serialprov: exiting driver");
	
	if ( tty_to_listen != NULL ){
	
		tty_kclose( tty_to_listen );
		
	}
	
}

module_init(serialprov_init);
module_exit(serialprov_exit);
