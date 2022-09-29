/**************************************************************************
 *                                                                        *
 *                  Copyright (C) 1995 Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *    Kitchenstubs: Stubs for Kitchen Sync system
 *
 *    This module encapsulates access by drivers to the ksync portion
 *    of the hw graph.  
 *
 *    There are two types of ksync devices: producers and consumers.
 *    Both types need to register their presence during driver attachment.
 *    They must un-register themselves during driver detachment.
 *    
 *    The hwgraph created by kysync_xxx will be used by applications
 *    via the libksync library, which will control switching of the
 *    ksync master, and notification of consumers when such an event
 *    occurs.
 *    
 */

#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/debug.h>
#include <sys/driver.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/ioerror.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/cdl.h>
#include <sys/errno.h>
#include <sys/ksync.h>
#if defined (SN0)
#include <sys/SN/klconfig.h>
#endif
int                     kitchen_sync_devflag = D_MP;

mutex_t ksync_graph_lock;  /* Ksync graph lock */

/* 
 * Public Interface 
 */
int ksync_attach( vertex_hdl_t   driver_vtx, 
		  char          *name, 
		  ksync_role_t   procon,
		  void         (*notify_func)(int), 
		  int            module_id );

int ksync_detach( vertex_hdl_t *driver_vtx, 
		  char         *name, 
		  ksync_role_t  procon, 
		  int           module_id );

int ksync_notify( vertex_hdl_t  driver_vtx, 
		  int           msg, 
		  int           module_id );

void
kitchen_sync_init()
{
}

/* ARGSUSED */
void
ksync_root_name( char *buf, int module_id)
{
}


/*
 * Name:
 *   ksync_attach
 *
 * Args:
 *   driver_vtx   - vertex handle of actual driver
 *   name         - name of edge to add
 *   procon       - KSYNC_CONSUMER,KSYNC_PRODUCER,KSYNC_BOTH, or KSYNC_NEUTRAL
 *   notify_func  - function to call on ksync_notify or NULL if not desired
 *
 * Purpose:
 *    Called by drivers to attach to the ksync graph.  On return, driver
 * will be in ksync inventory, and may send and receive ksync_notify
 * events if requested.
 *
 * Returns:
 *   0     - success
 *   EBADF - driver_vtx invalid  
 *   ENXIO - graph operations failed
 */
/* ARGSUSED */
int
ksync_attach( vertex_hdl_t driver_vtx, 
	      char        *name, 
	      ksync_role_t procon,
	      void       (*notify_func)(int),
	      int          module_id )
{
    return 0;
}

/*
 * Name:
 *  ksync_detach
 *
 * Args:
 *   driver_vtx - pointer vertex handle of actual driver
 *   name       - name of edge to remove
 *   procon     - KSYNC_CONSUMER,KSYNC_PRODUCER,KSYNC_BOTH, or KSYNC_NEUTRAL
 *
 * Purpose:
 *    To remove driver from ksync inventory.
 *
 * Notes:
 *    Got to find ksync vertex by name and destroy it, 
 *    not the one it points to.
 *
 * Returns:
 *   0     - success
 *   EBADF - driver_vtx invalid  
 *   ENXIO - graph operations failed
 *
 */
/* ARGSUSED */
int 
ksync_detach( vertex_hdl_t *driver_vtx, 
	      char         *name, 
	      ksync_role_t procon,
	      int          module_id )
{
    return 0;
}

/*
 * Name:
 *   ksync_notify
 *
 * Args:
 *   driver_vtx - vertex handle of master driver
 *   msg        - integer messge 
 *
 * Purpose:
 *   to notify ksync consumers of change in ksync.  Each
 *   consumer who registered a callback at attach time
 *   will get called back with msg as arg.
 *
 * Returns:
 *   0     - success
 *   EBADF - driver_vtx invalid  
 *   ENXIO - graph operations failed
 */
/* ARGSUSED */
int 
ksync_notify( vertex_hdl_t driver_vtx, int msg, int module_id )
{
    return 0;
}


#if _MIPS_SIM == _ABI64  /* xlate functions used in this file */

/*
 * Name:
 *    irix5_to_kstat_t
 *
 * Args:
 *   enum xlate_mode mode  - abi mode
 *   void *to, 		   - target structure ( kernel kstat_t )
 *   int count             - size of kernel structure
 *   xlate_info_t *info    - translation struct
 *
 * Purpose:
 *   To convert 32-bit kstat_t structs to 64-bit kstat_t structs.
 *   The name string is copied in.
 *
 * Returns:
 *     0
 *
 */

/* ARGSUSED */
int
irix5_to_kstat_t(
    enum xlate_mode mode, 
    void *to, 
    int count, 
    xlate_info_t *info)
{
        COPYIN_XLATE_PROLOGUE(_irix5_kitchen_sync_s, kstat_s);

        bcopy(target->kName,source->kName,KSTAT_NAME_LEN);
        target->kFlags = source->kFlags;

        return 0;
}

/*
 * Name:
 *    kstat_t_to_irix5
 *
 * Args:
 *   void *from         -   address of struct being translated
 *   int count          -   size of target struct
 *   xlate_info_t *info -   xlation info struct
 *
 * Purpose:
 *   To convert 64-bit kstat_t structs to 32-bit kstat_t structs.
 *   The name string is copied out.
 *
 * Returns:
 *     0
 */

/* ARGSUSED */
int
kstat_t_to_irix5(void *from, 
		 int count, 
		 xlate_info_t *info)
{
        XLATE_COPYOUT_PROLOGUE(kstat_s,_irix5_kitchen_sync_s);

        bcopy(source->kName,target->kName,KSTAT_NAME_LEN);

        target->kFlags = source->kFlags;

        return 0;
}

#endif /* _MIPS_SIM */
