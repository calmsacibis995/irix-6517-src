/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, 1995, Silicon Graphics, Inc.         *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Frame Scheduler Example -- driver
 */

/*
 * This is a pseudo-driver for testing the
 * Frame Scheduler DDI
 */

#include "sys/types.h"
#include "sys/edt.h"
#include "sys/pio.h"
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "sys/sema.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/hwgraph.h"
#include "sys/groupintr.h"

/*
 * This is the interrupt group I have to direct the
 * interrupt to. In this example I will fake a hardware
 * interrupt by sending an intercpu group interrupt
 * to this group. When a real hardware device is
 * present, the hardware would directly interrupt
 * the cpu running the frame scheduler.
 */
static intrgroup_t* my_intrgroup = 0;

/*
 * Define the device flag to notify the kernel that this
 * driver is designed to operate on multiprocessor systems.
 */
int frsdrvdevflag = D_MP;

/*
 * This is the FRS initialization function called from the
 * frame scheduler creation method whenever a user creates
 * a frame scheduler.
 */

void
frsdrv_frs_set(intrgroup_t* intrgroup)
{
	/*
	 * Since this is a pseudo-driver just to
	 * illustrate the the usage of the FRS DDI,
	 * we do not need to initialize anything.
	 *
	 * Real device drivers may choose to use this
	 * opportunity to prepare for frame scheduler usage,
	 * since the frame scheduler becomes armed and ready
	 * to receive interrupts upon the return of this
	 * routine.
	 */

	my_intrgroup = intrgroup;

#ifdef FRSDRV_DEBUG        
	cmn_err(CE_NOTE, "This is the set routine\n");
#endif        
}

/*
 * This is the FRS termination function called from the
 * frame scheduler destructor method whenever a user destroys
 * a frame scheduler.
 */

void
frsdrv_frs_clear(void)
{
	/*
	 * This is the place to cleanup state
	 * and probably quiesce the device
	 */

#ifdef FRSDRV_DEBUG
	cmn_err(CE_NOTE, " This is the clear routine\n");
#endif        
}


/*
 * This is the basic device driver initialization routine called from
 * the IRIX initialization procedure.
 *
 * This routine is called once per configured device. A device is
 * "configured" if it appears in the system/irix.sm configuration file.
 */

void
frsdrvinit(void)
{
	vertex_hdl_t vhdl;
	int rval;

	/*
	 * This is the place to initialize the device
	 * driver (e.g., create a hardware graph node).
	 */

	rval = hwgraph_char_device_add(hwgraph_root,
				       "frsdrv/1",
				       "frsdrv",
				       &vhdl);

	hwgraph_chmod(vhdl, HWGRAPH_PERM_EXTERNAL_INT);

	/*
	 * This is also the appropriate place to export the driver to
	 * the frame scheduler.
	 *
         * Im using Frame Scheduler Device Driver Identifier <3>.
         */
	rval = frs_driver_export(3, frsdrv_frs_set, frsdrv_frs_clear);

	cmn_err(CE_NOTE,
		"Frame Scheduler example driver (frsdrv) is installed\n");
}


/*
 * This is the interrupt routine called when the
 * hardware device interrupts.
 */

void
frsdrvintr(struct eframe_s *ep, unsigned int arg)
{

	/*
	 * Pass control to the Frame Scheduler
	 */
	frs_handle_driverintr();

	/*
	 * Re-enable interrupts, etc.
	 */
        
#ifdef FRSDRV_DEBUG
	cmn_err(CE_NOTE, "This is the intr routine\n");
#endif        
}

  
int
frsdrvopen(dev_t dev, int flags)
{

#ifdef FRSDRV_DEBUG
	cmn_err(CE_NOTE, "frsdrv_open has been called\n");
#endif
	return (0);
}

int
frsdrvclose(dev_t dev, int flags)
{

#ifdef FRSDRV_DEBUG
	cmn_err(CE_NOTE, "frsdrv_close has been called\n");
#endif
	return (0);
}

int
frsdrvwrite(dev_t dev)
{

#ifdef FRSDRV_DEBUG
	cmn_err(CE_NOTE, "frsdrv_write has been called\n");
#endif
	return (0);
}

int
frsdrvread(dev_t dev)
{

#ifdef FRSDRV_DEBUG
	cmn_err(CE_NOTE, "frsdrv_read has been called\n");
#endif
	return (0);
}

int
frsdrvioctl(dev_t dev, uint cmd, char *arg, int flags)
{

#ifdef FRSDRV_DEBUG
	cmn_err(CE_NOTE, "frsdrv_ioctl has been called\n");
#endif

	/*
	 * Since this is a pseudo-driver, I will be
         * faking the hardware interrupts by sending
         * this intercpu group interrupt every time
         * an ioctl is done on this driver.
	 */

        if (my_intrgroup) {
                sendgroupintr(my_intrgroup);
                return (0);
        } 

	return (ENODEV);
}
