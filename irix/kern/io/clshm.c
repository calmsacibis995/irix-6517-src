/*
 * clshm.c
 * -------
 * The driver for inter-partition shared-memory communication over CrayLink. 
 *
 * Copyright 1998, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include "sys/types.h"		/* for v_gethandle etc. */
#include "sys/errno.h"		/* for error numbers */
#include "sys/cred.h"		/* for cred_t etc. defs */
#include "ksys/ddmap.h" 	/* for vhandl_t, v_mapphys etc. defs */
#include "sys/mman.h"		
#include "ksys/partition.h"	/* for partition func defs. */
#include "sys/ddi.h"		/* should be last; getminor ... etc. */

/* includes for find_mld */
#include "sys/nodepda.h"	/* for cnodeid_to_vertex */
#include "sys/prctl.h"		/* for PR_THREADS */
#include "os/proc/pproc_private.h" /* for proc_t */
#include "sys/uthread.h"	/* for uthread_t */
#include "os/numa/pmo_ns.h"	/* for pmo_ns_t */
#include "os/numa/pm.h"		/* for pm_t */
#include "os/numa/aspm.h"	/* for getpmo_ns */

#include "sys/SN/clshm.h"
#include "ksys/clshmdev.h"
#include "sys/SN/clshmd.h"

#define	IS_LOCKED	1

#if defined(DEBUG)
#define	CLSHM_DEBUG
/* comment out next line and compile debug kernel to get debug output */
#undef 	CLSHM_DEBUG
#endif

#ifdef CLSHM_DEBUG
int	clshm_debug = 10;
int	xdump_flag = 0;
#define dprintf(lvl, x) if (clshm_debug >= lvl) printf x
#else   /* ! CLSHM_DEBUG */
#define dprintf(lvl, x)
#endif	/* ! CLSHM_DEBUG */


/* FUNCTIONS are declared in this order */

/*debugging functions */
static 	void 	dump_clshm_state(clshm_dev_t *cd);
static 	void 	xdump(char *bytes, int num_bytes);

/* intialization */
        int 	clshminit(void);
static 	void	clshm_part_activate(partid_t partn);
static 	void	clshm_part_deactivate(partid_t partn);

/* openning */
        int 	clshmopen(dev_t *devp, int flag);
static 	int 	open_shm(dev_t *devp, int flag);

/* ioctls and making shmids */
        int 	clshmioctl(dev_t dev, int cmd, void *arg, int mode, 
			   cred_t *cred_p, int *rvalp);
static 	int 	ctl_ioctl(dev_t dev, int cmd, void *arg, int mode, 
		     cred_t *cred_p, int *rvalp);
static 	int 	allocate_local_shmid(clshm_dev_t *ctl_dev, clshm_msg_t *msg);
static 	int 	allocate_remote_shmid(clshm_dev_t *ctl_dev, clshm_msg_t *msg);
static 	int 	allocate_cross_pages(clshm_dev_t *ctl_dev, int shmid,
				     off_t off, int len, int prot, 
				     pid_t pmo_pid, pmo_handle_t pmo_handle, 
				     pmo_type_t pmo_type, partid_t asking_part,
				     local_map_record_t **place_map_rec);
static  remote_map_record_t * make_new_remote_map_record(clshm_dev_t *ctl_dev, 
			             partid_t part, shmid_in_msg shmid, 
				     len_in_msg len, off_t off);

/* mld helper functions */
static 	mldref_t find_mld(pid_t pmo_pid, pmo_handle_t pmo_handle,
			  pmo_type_t pmo_type);


/* removing shmids */
static 	int 	remove_local_remote_shmid(clshm_dev_t *ctl_dev, 
					  clshm_msg_t *msg);
static 	int 	remove_local_shmid(clshm_dev_t *ctl_dev, shmid_in_msg shmid);
static 	int 	remove_remote_shmid(clshm_dev_t *ctl_dev, partid_t part, 
			       shmid_in_msg shmid);

/* mapping functions */
        int 	clshmmap(dev_t dev, vhandl_t *vt, off_t off, int len, 
			 int prot);
static 	int 	ctl_mmap(dev_t dev, vhandl_t *vt, off_t off, int len, 
			 int prot); 
static 	int 	shm_mmap(dev_t dev, vhandl_t *vt, off_t off, int len, 
			 int prot);
static 	int 	set_maps(clshm_dev_t *ctl_dev, vhandl_t *vt, 
			 shmid_in_msg shmid, partid_t part, 
			 remote_map_record_t *rem_map);
static	int	register_in_maptable(clshm_dev_t *ctl_dev, vhandl_t *vt, 
				     shmid_in_msg shmid, partid_t part, 
				     remote_map_record_t *rem_map);

/* daemon communication */
static 	int 	send_to_clshmd(clshm_dev_t  *ctl_dev, clshm_msg_t *msg);

/* error recovery */
static 	int 	clshm_err_handler(eframe_t *efr, paddr_t  phys_err_addr, 
				  part_error_t err_type);
static 	int 	recover_from_error(paddr_t  fault_addr, 
				   part_error_t  access_type);
static 	void 	remove_remote_page_on_error(clshm_dev_t *ctl_dev,
				partid_t part, remote_map_record_t *rem_elem, 
				remote_map_record_t *rem_prev);

/* unmapping and closing */
        int 	clshmunmap(dev_t dev, vhandl_t *vt);
static 	int 	shm_unmap(dev_t dev, vhandl_t *vt);
        int 	clshmclose(dev_t dev);

/* removing and killing funcitons */
static 	void 	remove_part_from_maptab(clshm_dev_t *ctl_dev, partid_t part);
static 	int 	unregister_from_maptable(clshm_dev_t *ctl_dev, vhandl_t *vt, 
					 int process);
static 	void 	signal_processes(clshm_dev_t  *ctl_dev, partid_t part);
static 	void 	free_remote_map_records(clshm_dev_t *ctl_dev, partid_t part);
static 	void 	free_local_map_records(clshm_dev_t *ctl_dev);
static 	void 	free_shm_state(clshm_dev_t  *ctl_dev);
static 	void 	release_ctl_resources(clshm_dev_t  *ctl_dev);


/* global variables */
int		clshmdevflag = D_MP;
clshm_dev_t  	*GlobCtlDev[MAX_UNITS];
static  int	clshm_started_up[MAX_UNITS];

/* dump_clshm_state
 * ----------------
 * Parameters:
 *	cd:	general clshm_dev structure
 *	
 * Dump all state that we can.
 */
static void dump_clshm_state(clshm_dev_t *cd)
{
    local_map_record_t *lp;
    map_tab_entry_t *mt;
    remote_map_record_t *rp;
    int i, j;

    dprintf(0, ("clshm: DUMP DRIVER STATE\n\n"));
    dprintf(0, ("\tGENERAL DATA FIELDS:\n"));
    dprintf(0, ("\tmagic should be \"clshm\" == %s\n", cd->clshm_magic));
    dprintf(0, ("\tstate: INITTED = %s, MMAPPED = %s, OPENNED = %s, "
		"EXOPEN = %s, SHUT = %s\n", 
		((cd->c_state & CL_SHM_C_ST_INITTED) ? "Y" : "N"), 
		((cd->c_state & CL_SHM_C_ST_MMAPPED) ? "Y" : "N"), 
		((cd->c_state & CL_SHM_C_ST_OPENNED) ? "Y" : "N"), 
		((cd->c_state & CL_SHM_C_ST_EXOPEN) ? "Y" : "N"), 
		((cd->c_state & CL_SHM_C_ST_SHUT) ? "Y" : "N")));
    dprintf(0, ("\tmax_pages = %d, page_size = %d, clshmd_pages = %d\n",
		cd->max_pages, cd->page_size, cd->clshmd_pages));
    dprintf(0, ("\tclshmd_ref = 0x%llx, clshmd_buff = 0x%llx\n\t"
		"clshmd_ID = 0x%x, daem_vers_num = %d, drv_vers_num\n", 
		cd->clshmd_ref, cd->clshmd_buff, cd->clshmd_ID, 
		cd->daem_minor_vers, CLSHM_DRV_MINOR_VERS));

    dprintf(0, ("\n\tLOCAL PAGE INFO:\n"));
    for(lp = cd->loc_page_info; lp != NULL; lp = lp->next) {
	dprintf(0, ("\tshmid = 0x%x, len = %d, off = %lld, num_pages = %d\n", 
		    lp->shmid, lp->len, lp->off, lp->num_pages));
	for(i = 0; i < lp->num_pages; i++) {
	    dprintf(0, ("\t\tpfdat = 0x%llx, kpaddr = 0x%llx\n", 
			lp->pages[i].pfdat, lp->pages[i].kpaddr));
	}
    }

    dprintf(0, ("\n\tREMOTE PARTITION INFO:\n"));
    for(i = 0; i < MAX_PARTITIONS; i++) {
	if(!(cd->remote_parts[i].state & CLSHM_PART_S_PART_UP)) {
	    continue;
	}
	dprintf(0, ("\n\tPARTITION #%d:\n", i));
	dprintf(0, ("\tstate: DAEM_CONTACT = %s\n\n",
		    ((cd->remote_parts[i].state & CLSHM_PART_S_DAEM_CONTACT) ? 
		     "Y" : "N")));
	dprintf(0, ("\tMAP TABLE:\n"));
	for(mt = cd->remote_parts[i].map_tab; mt != NULL; mt = mt->next) {
	    dprintf(0, ("\tshmid = 0x%x, pid = %d, gid = %d, "
			"vt_handle = %x\n", 
			mt->page_info->shmid, mt->pid, 
			mt->gid, mt->vt_handle));
	    dprintf(0, ("\t\tsignal_num = %d, err_handled = %d\n",
			mt->signal_num, mt->err_handled));
	}
	dprintf(0, ("\tREMOTE MAP RECORDS\n"));
	for(rp = cd->remote_parts[i].rem_page_info; rp != NULL; 
	    rp = rp->next) {
	    dprintf(0, ("\tshmid = 0x%x, len = %d, off = %lld, "
			"num_pages = %d, ready = %d, count = %d\n",
			rp->shmid, rp->len, rp->off, rp->num_pages, 
			rp->ready, rp->count));
	    dprintf(0, ("\tpaddrs: "));
	    for(j = 0; j < rp->num_pages; j++) {
		if(j != 0 && (j % 6) == 0) {
		    dprintf(0, ("\n\t\t"));
		}
		dprintf(0, ("0x%llx ", rp->paddrs[j]));
	    }
	    dprintf(0, ("\n\tpfns: "));
	    for(j = 0; j < rp->num_pages; j++) {
		if(j != 0 && (j % 9) == 0) {
		    dprintf(0, ("\n\t\t"));
		}
		dprintf(0, ("0x%x ", rp->pfns[j]));
	    }
	    dprintf(0, ("\n\tmsg_recv of %d: ", rp->num_msgs));
	    for(j = 0; j < rp->num_msgs; j++) {
		if(rp->msg_recv[j]) {
		    dprintf(0, ("%d ", j));
		}
	    }
	    dprintf(0, ("\n\n"));
	}	
    }
    dprintf(0, ("DUMP DRIVER STATE DONE\n"));
}

/* xdump
 * -----
 * Parameters:
 *	bytes: 		bytes to dump
 *	num_bytes:	number of bytes to dump
 */
/* ARGSUSED */
static void xdump(char *bytes, int num_bytes)
{
#   ifdef	CLSHM_DEBUG
    int		i;

    if(xdump_flag)  {
        if(! bytes || ! num_bytes)  {
	    cmn_err(CE_WARN, "xdump has ptr or num bytes as 0");
        }
        else  {
	    for(i = 0; i < num_bytes; i++)   {
	        printf("%d: %x\n", i, bytes[i]);
	    }
        }
    }
#   endif	/* CLSHM_DEBUG */
    return;
}


/* clshminit
 * ---------
 * Parameters:
 *	return:	Always returns 0
 *
 * Try to init the clshm driver.  Do so by setting up all internal
 * structures and hwgraph entries for this driver.
 */
int clshminit(void)
{
    uint		unit;	/* if we ever decide to have > 1 maj dev */
    clshm_dev_t 	*ctl_dev;
    graph_error_t       ge;
    char                name[MAXPATHLEN];
    dev_t               td1, td2; /* ctl and shm devices */
    clshm_dev_info_t    *dinfo;
    int			i;

    dprintf(5, ("clshminit\n"));

    for(unit = 0; unit < MAX_UNITS; unit++) {
	/* this mem. will never be freed; ctl dev will not
	 *  be re-allocated: it remins after init */
	ctl_dev =  kmem_zalloc(sizeof(clshm_dev_t), KM_SLEEP);

	/* create the ctl device to sent control messages to driver
	 * and to map memory in the daemon */
	dinfo = kmem_zalloc(sizeof(clshm_dev_info_t), KM_SLEEP);
	dinfo->type = CTL_TYPE;
	dinfo->dev_parms = (void *) ctl_dev;
	dinfo->unit = unit;
	sprintf(name, "%s/%d/ctl", EDGE_LBL_CLSHM, unit);
	if(GRAPH_SUCCESS !=
	   (ge = hwgraph_char_device_add(hwgraph_root, name, 
					 EDGE_LBL_CLSHM, &td1))){
		cmn_err(CE_WARN, "clshminit: hwgraph_device_add: failed: "
			"%d: %s\n", ge, name);
		break;
	}
	device_info_set(td1, dinfo);
	hwgraph_chmod(td1, CLSHM_CTL_DEV_PERMS);

	/* create the shm device for all the maps for shared memory */
	dinfo = kmem_zalloc(sizeof(clshm_dev_info_t), KM_SLEEP);
	dinfo->type = SHM_TYPE;
	dinfo->dev_parms = (void *) ctl_dev;
	dinfo->unit = unit;
	sprintf(name, "%s/%d/shm", EDGE_LBL_CLSHM, unit);
	if(GRAPH_SUCCESS !=
	   (ge = hwgraph_char_device_add(hwgraph_root, name, 
					 EDGE_LBL_CLSHM, &td2))){
		cmn_err(CE_WARN, "clshminit: hwgraph_device_add: failed: "
			"%d: %s\n", ge, name);
		break;
	}
	device_info_set(td2, dinfo);
	hwgraph_chmod(td2, CLSHM_SHM_DEV_PERMS);

	/* general clshm_dev fields initted */
	clshm_started_up[unit] = 0;
	bzero(ctl_dev, sizeof(clshm_dev_t));
	SET_MAGIC(ctl_dev);
	init_mutex(&ctl_dev->devmutex, MUTEX_DEFAULT, 
		   "clshm_mut", unit);
	ctl_dev->c_state = CL_SHM_C_ST_INITTED;

	ctl_dev->max_pages = CLSHM_DEF_NUM_PAGES;
	ctl_dev->page_size = CLSHM_DEF_PAGE_SIZE;
	ctl_dev->clshmd_pages = CLSHM_DEF_DAEM_PAGES;
	ctl_dev->clshmd_timeout = CLSHM_DEF_DAEM_TIMEOUT;

	ctl_dev->daem_minor_vers = -1;
	
	for(i = 0; i < MAX_PARTITIONS; i++) {
	    ctl_dev->remote_parts[i].partid = i;
	}

	GlobCtlDev[unit] = ctl_dev;
    }

    /* register handlers */
    part_register_handlers(clshm_part_activate, clshm_part_deactivate);

    dprintf(5, ("clshminit%d: initialization complete.\n", unit));
    return(0);
}

/* turn a whole number into a two digit hex number */
static	char _hex[] = "0123456789abcdef";
#define CLSHM_HEX(_x, _s) (_s)[0] = _hex[((_x) >> 4)]; \
                        (_s)[1] = _hex[((_x) & 0xf)];\
                        (_s)[2] = '\0'

/* clshm_part_activate
 * -------------------
 * Parameters:
 *	partn:	partition number that is coming up
 *
 * This is the registered handler for a partition coming up.
 * Mark in our list that this partition is up and then send a message
 * to the daemon that is partition has come up.
 */
static void clshm_part_activate(partid_t partn)
{
    clshm_msg_t         msg;
    int		    	error;
    clshm_dev_t	    	*ctl_dev = GlobCtlDev[0];
    graph_error_t       ge;
    char                name[MAXPATHLEN], part_string[3];
    dev_t               td;

    dprintf(5, ("clshm Inside clshm_part_activate, partn up is %d\n", partn));
    
    LOCK(ctl_dev->devmutex);
    
    /* make sure that we don't already think this partition is up */
    if(partn >= MAX_PARTITIONS || partn < 0) {
	cmn_err(CE_WARN, "clshm_part_activate: was given a bad partition "
		"number %d in partition activation!!!!\n", partn);
	return;
    }

    if(ctl_dev->remote_parts[partn].state & CLSHM_PART_S_PART_UP) {
	cmn_err(CE_WARN, "clshm_part_activate: was given a partition "
		"number %d which was already activated!!!!\n", partn);
	return;
    }
    ctl_dev->remote_parts[partn].state = CLSHM_PART_S_PART_UP;

    UNLOCK(ctl_dev->devmutex);

    /* add the partition to the hwgraph */
    CLSHM_HEX(partn, part_string);
    sprintf(name, "%s/%s/%s", EDGE_LBL_CLSHM, EDGE_LBL_CLSHM_PART, 
	    part_string);
    if(GRAPH_SUCCESS != (ge = hwgraph_path_add(hwgraph_root, name, &td))){
	cmn_err(CE_WARN, "clshm_part_activate: hwgraph_device_add: "
		"failed: %d: %s\n", ge, name);
    } else {
	hwgraph_chmod(td, CLSHM_PART_PERMS);
    }
    /* end hwgraph stuff */


    /* msg will go to local daem only; src says which
     *  partn has just come up; dst unused
     */

    /* tell the daemon that is partition is up if the daem is up */
    if(ctl_dev->clshmd_buff)  {    /* if daemon is up */
	msg.type = DRV_TO_DAEM_NEW_PART;
	msg.src_part = part_id();
	msg.dst_part = partn;
	msg.len = 0;
	dprintf(10, ("clshm Sending to clshmd; ptr is 0x%x\n",
		     ctl_dev->clshmd_buff));
	error = send_to_clshmd(ctl_dev, &msg);
	if(error)  {
	    cmn_err(CE_WARN, "clshm: Could not inform clshmd "
		    "about upcoming partn %d\n", partn);
	}
    }
}


/* clshm_part_deactivate
 * ---------------------
 * Parameters:
 *	partn:	partition to deactivate
 *
 * deactivate handler given to the partition code for when a partition
 * goes up or down.  I
 */
static void clshm_part_deactivate(partid_t partn)
{
    clshm_msg_t         msg;
    int		    	error = 0;
    clshm_dev_t	    	*ctl_dev = GlobCtlDev[0];
    graph_error_t       ge;
    char                name[MAXPATHLEN], part_string[3];
    vertex_hdl_t	connect_vrtx, vrtx_to_nuke;


    ASSERT_ALWAYS(part_id() != partn);

    dprintf(5, ("clshm Inside clshm_part_deactivate, partn down is %d\n", 
		partn));
	
    /* remove partition from hwgraph */
    sprintf(name, "%s/%s", EDGE_LBL_CLSHM, EDGE_LBL_CLSHM_PART);
    if(GRAPH_SUCCESS != (ge = hwgraph_path_lookup(hwgraph_root, name,
						  &connect_vrtx, NULL))) {
	cmn_err(CE_WARN, "clshm_part_deactivate: hwgraph_path_lookup: "
		"failed: %d: %s\n", ge, name);
	error = 1;
    } 
    if(!error) {
	CLSHM_HEX(partn, part_string);
	if (GRAPH_SUCCESS != (ge = hwgraph_edge_remove(connect_vrtx, 
						       part_string, 
						       &vrtx_to_nuke)))  {
	    cmn_err(CE_WARN, "clshm_part_deactivate: hwgraph_edge_remove: "
		    "failed: %d: %s/%s\n", ge, name, part_string);
	    error = 1;
	}
    }

    if(!error) {
	dprintf(5, ("clshm: Removed edge %s/%s, refcnt %d\n", name, 
		    part_string, hwgraph_vertex_refcnt(vrtx_to_nuke)));
	while(hwgraph_vertex_refcnt(vrtx_to_nuke) > 1)  {
	    if (GRAPH_SUCCESS != (ge = hwgraph_vertex_unref(vrtx_to_nuke))) {
		cmn_err(CE_WARN, "clshm_part_deactivate: "
			"hwgraph_vertex_unref: failed: %d: %s/%s\n", ge, 
			name, part_string);
		error = 1;
	    }
	}
    }

    if(!error) {
	dprintf(5, ("clshm: Reduced edge %s/%s refcnt to %d\n", name, 
		    part_string, hwgraph_vertex_refcnt(vrtx_to_nuke)));
	if (GRAPH_SUCCESS != (ge = hwgraph_vertex_destroy(vrtx_to_nuke))) {
	    cmn_err(CE_WARN, "clshm_part_deactivate: hwgraph_vertex_destroy: "
		    "failed: %d: %s/%s\n", ge, name, part_string);
	    error = 1;
	}
    }
    /* end hwgraph stuff */

    LOCK(ctl_dev->devmutex);

    /* make sure that we don't already think this partition is up */
    if(partn >= MAX_PARTITIONS || partn < 0) {
	cmn_err(CE_WARN, "clshm_part_deactivate: was given a bad partition "
		"number %d in partition activation!!!!\n", partn);
	UNLOCK(ctl_dev->devmutex);
	return;
    }

    if(!(ctl_dev->remote_parts[partn].state & CLSHM_PART_S_PART_UP)) {
	cmn_err(CE_WARN, "clshm_part_deactivate: was given a partition "
		"number %d which was not activated!!!!\n", partn);
	UNLOCK(ctl_dev->devmutex);
	return;
    }


    signal_processes(ctl_dev, partn);
    remove_part_from_maptab(ctl_dev, partn);
    ctl_dev->remote_parts[partn].map_tab = NULL;
    ctl_dev->remote_parts[partn].rem_page_info = NULL;
    ctl_dev->remote_parts[partn].state = 0;
    /* don't have to send a RMID to all the ids for this
     * partition cause we know that this partition is going 
     * to go away in the daemon regardless */


    UNLOCK(ctl_dev->devmutex);

    /* msg will go to local daem only; src says which
     *  partn has just gone down; dst unused
     */
    if(ctl_dev->clshmd_buff)  {    /* if daemon is up */
	msg.type = DRV_TO_DAEM_PART_DEAD;
	msg.src_part = part_id();
	msg.dst_part = partn;
	msg.len = 0;
	error = send_to_clshmd(ctl_dev, &msg);
	if(error)  {
	    cmn_err(CE_WARN, "clshm: Could not inform clshmd "
		    "about failed partn %d\n",
		    partn);
	}
    }
}


/* clshmopen
 * ---------
 * Parameters:
 *	devp:	device info 
 *	flag:	flags for this open
 *	return:	errno values
 *
 * Open for ctl and shm.  For ctl nothing is required here.  But for
 * shm make sure that the driver is in a state that we can open
 * the file (the daemon isn't going down and mapped the control device).
 */
int clshmopen(dev_t *devp, int flag)
{
    clshm_dev_t  	*ctl_dev;
    clshm_dev_info_t    *dev_info;
    vertex_hdl_t    	vhdl = dev_to_vhdl(*devp);

    dprintf(10, ("clshmopen: device 0x%x\n", *devp));
    dev_info = (clshm_dev_info_t *) device_info_get(vhdl);
    if(!dev_info)  {
	cmn_err(CE_PANIC, "NULL dev_info in clshmopen\n");
    }

    ctl_dev = (clshm_dev_t *) dev_info->dev_parms;
    if(! IS_CTL_DEV(dev_info))  {
	if(ctl_dev->c_state & CL_SHM_C_ST_SHUT)  {
	    /* shutdown pending; no more non-ctl opens allowed */
	    dprintf(0, ("clshm: attempt to open device after shutdown "
			"scheduled\n"));
	    return(EINVAL);
	}
	if(!(ctl_dev->c_state & CL_SHM_C_ST_MMAPPED)) {
	    dprintf(0, ("clshm: haven't mapped daemon yet\n"));
	    return(EINVAL);
	}

	dprintf(10, ("clshm Going to open non-ctl device\n"));
	return(open_shm(devp, flag));
    }
    return(0);
}



/* open_shm
 * --------
 * Parameters:
 *	devp:	pointer to the dev info we need
 *	flag:	flag passed into the open call
 *	return:	errno values
 *
 * Open the shm device.  Make sure that we can open
 * it with the given flags and set the flags in the device to say that
 * this device is open.
 */
static int open_shm(dev_t *devp, int flag)
{
    clshm_dev_t  *ctl_dev;
    int	    error = 0;

    clshm_dev_info_t    *dev_info;
    
    dev_info = (clshm_dev_info_t *) device_info_get(*devp);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in shm open of clshm\n");
    }
    ASSERT(!IS_CTL_DEV(dev_info));

    ctl_dev = (clshm_dev_t *) dev_info->dev_parms;

    CHECK_CL_SHM_C(ctl_dev);
    LOCK(ctl_dev->devmutex);

    /* check to make sure that FEXCL is only set once and return error
     * if we are trying to open the device when someone else has it
     * open in another state.
     */
    if(flag & FEXCL) {
	if(ctl_dev->c_state & CL_SHM_C_ST_OPENNED) {
	    cmn_err(CE_WARN, "clshm: Attempt to open clshm shm device in "
		    "O_EXCL when already openned\n");
	    error = EINVAL;
	} else {
	    ctl_dev->c_state |= CL_SHM_C_ST_EXOPEN;
	}
    } else if(ctl_dev->c_state & CL_SHM_C_ST_EXOPEN) {
	cmn_err(CE_WARN, "clshm: Attempt to open clshm shm device when "
		"already openned in O_EXCL mode\n");
	error = EINVAL;
    }

    /* say that a shm device has been opened (no more configs to
     * the driver is allowed by user */
    if(!error) {
	ctl_dev->c_state |= CL_SHM_C_ST_OPENNED;
    }

    UNLOCK(ctl_dev->devmutex);
    return(error);
}



/* clshmioctl
 * ----------
 * Parameters:
 *	dev:	device info
 *	cmd:	command
 *	arg:	arg passed in
 *	mode:	ignored by other functions
 *	cred_p:	ignored by other functions
 *	rvalp:	ignored by other functions
 *	return:	errno values
 *
 * Calls into ctl ioctl routines.
 */
/* ARGSUSED */
int clshmioctl(dev_t dev, int cmd, void *arg, int mode, 
	       cred_t *cred_p, int *rvalp)
{
    clshm_dev_info_t    *dev_info;

    dev_info = (clshm_dev_info_t *) device_info_get(dev);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in clshmioctl \n");
    }
    if(IS_CTL_DEV(dev_info)) {
	return(ctl_ioctl(dev, cmd, arg, mode, cred_p, rvalp));
    }
    return(EINVAL);
}


/* ctl_ioctl
 * ---------
 * Parameters:
 *	dev:	device info
 *	cmd:	ioctl that we are passed
 *	arg:	arg to ioctl
 *	mode:	ignored
 *	cred_p:	ignored
 *	rvalp:	ignored
 *	return:	errno values
 *
 * Field all the control ioctls.  These should be coming from the
 * daemon and clshmctl and that is all!!
 */
/* ARGSUSED */
static int ctl_ioctl(dev_t dev, int cmd, void *arg, int mode, 
		     cred_t *cred_p, int *rvalp)
{
    int		error = 0;
    clshm_dev_t   *ctl_dev;
    clshm_dev_info_t    *dev_info;
    u_int	unit;

    dev_info = (clshm_dev_info_t *) device_info_get(dev);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in ctl_ioctl of clshm\n");
    }
    ASSERT(IS_CTL_DEV(dev_info)); 
    unit = dev_info->unit;
    ctl_dev = (clshm_dev_t *)  dev_info->dev_parms;


    CHECK_CL_SHM_C(ctl_dev);
    LOCK(ctl_dev->devmutex);

    switch(cmd)  {
	default: {
	    error = EINVAL;
	    break;
   	}

	/* set the config */
	case CL_SHM_SET_CFG:  {
	    clshm_config_t  	cfg;

            if(copyin((caddr_t) arg, (caddr_t) &cfg, 
					sizeof(clshm_config_t)) < 0) {
                error =  EFAULT;
                break;
            }

	    dprintf(5, ("clshm: cfg copied; %d pages, %d daem pgs, "
			"%d daem timeout\n", 
			cfg.max_pages, cfg.clshmd_pages));

	    /* if already opened, we can't configure */
	    dprintf(5, ("clshm State of ctl dev is 0x%x\n", ctl_dev->c_state));

	    if(cfg.page_size != ctl_dev->page_size ||
	       cfg.major_version != CLSHM_DRV_MAJOR_VERS ||
	       cfg.minor_version != CLSHM_DRV_MINOR_VERS) { 
		dprintf(0, ("clshm WARNING: can't change page size or "
			    "major and minor version of driver\n"));
		error = EINVAL;
		break;
	    }

	    /* sanity-check parameters */
	    if(cfg.max_pages <= 0 || cfg.max_pages > MAX_CLSHM_PGS) {
		dprintf(0, ("clshm WARNING: trying to configure clshm with "
			    "bad parameters\n"));
		error = EINVAL;
		break;
	    }

	    if(ctl_dev->c_state & (CL_SHM_C_ST_OPENNED | 
				   CL_SHM_C_ST_MMAPPED)) {
		dprintf(0, ("clshm: non control device already openned "
			    "or mapped so can't change config\n"));
		error = EINVAL;
		break;
	    }

	    ctl_dev->max_pages = cfg.max_pages;
	    ctl_dev->clshmd_pages = cfg.clshmd_pages;
	    ctl_dev->clshmd_timeout = cfg.clshmd_timeout;
	    
            break;
	}


	case CL_SHM_GET_CFG:  {
	    clshm_config_t  cfg;

	    dprintf(5, ("clshm ctl_ioctl: CL_SHM_GET_CFG, started: %d\n",
		clshm_started_up[unit]));

	    dprintf(10, ("clshm CL_SHM_GET_CFG ioctl: pgs %d, dpgs %d\n", 
			 ctl_dev->max_pages, ctl_dev->clshmd_pages));

	    cfg.max_pages = ctl_dev->max_pages;
	    cfg.page_size = ctl_dev->page_size;
	    cfg.major_version = CLSHM_DRV_MAJOR_VERS;
	    cfg.minor_version = CLSHM_DRV_MINOR_VERS;
	    cfg.clshmd_pages = ctl_dev->clshmd_pages;
	    cfg.clshmd_timeout = ctl_dev->clshmd_timeout;
	    
            if(copyout((caddr_t) &cfg, (caddr_t) arg,
					sizeof(clshm_config_t)) < 0) {
                error =  EFAULT;
                break;
            }
	    break;
	}

	case CL_SHM_STARTUP:  {
	
	    if(! clshm_started_up[unit])  {
		clshm_started_up[unit] = 1;

	    }
	    else {
		dprintf(0, ("clshm: multiple startups without "
			    "intervening shutdown \n"));
		error = ENOTTY;
	    }
	    break;
	}


	case CL_SHM_SHUTDOWN:  {
	    clshm_msg_t msg;
	    partid_t part;
	    
	    if(!clshm_started_up[unit]) {
		dprintf(0, ("clshm: shutdown when not up in the first "
			    "place\n"));
		error = EINVAL;
		break;
	    }

	    part = part_id();

	    ctl_dev->c_state |= CL_SHM_C_ST_SHUT;
	    clshm_started_up[unit] = 0;
	    ctl_dev->remote_parts[part].state &= ~CLSHM_PART_S_DAEM_CONTACT;

	    /* tell the daemon to die first */
	    msg.type = DRV_TO_DAEM_CLSHM_DIE_NOW;
	    msg.src_part = part;
	    msg.dst_part = part;
	    msg.len = 0;
	    if(send_to_clshmd(ctl_dev, &msg) || !(ctl_dev->c_state & 
						  (CL_SHM_C_ST_MMAPPED))) {
		dprintf(5, ("clshm: shutdown when daem already dead\n"));
		release_ctl_resources(ctl_dev);
	    } else  {
		dprintf(5, ("clshm: daemon sent die message\n"));
	    }

	    break;
	}

	/* we got a message from the daemon, check what it has sent us */
        case CL_SHM_DAEM_MSG: {
	    clshm_msg_t		msg;

            if(copyin((caddr_t) arg, (caddr_t) &msg, 
					sizeof(clshm_msg_t)) < 0) {
                error =  EFAULT;
                break;
	    }

	    dprintf(3, ("clshm: recv DAEM_MSG: sent msg: type %c, src %d, "
			"dst %d, len %d\n", msg.type, msg.src_part, 
			msg.dst_part, msg.len));

	    if(msg.src_part >= MAX_PARTITIONS || msg.src_part < 0 ||
	       msg.dst_part >= MAX_PARTITIONS || msg.dst_part < 0) {
		dprintf(0, ("clshm: bad partition number in msg\n"));
		error = EINVAL;
		break;
	    }

	    xdump(msg.body, msg.len);
	    switch(msg.type) {
	        case DAEM_TO_DRV_NEED_SHMID: {
		    dprintf(5, ("clshm: DAEM_TO_DRV_NEED_SHMID\n"));
		    error = allocate_local_shmid(ctl_dev, &msg);
		    break;
		}

	        case DAEM_TO_DRV_TAKE_SHMID: {
		    dprintf(5, ("clshm: DAEM_TO_DRV_TAKE_SHMID\n"));
		    error = allocate_remote_shmid(ctl_dev, &msg);
		    break;
		}

	        case DAEM_TO_DRV_NEED_PATH: {
		    shmid_in_msg shmid;
		    local_map_record_t *loc_map;
		    char *cpy_ptr, *name;
		    int cpy_len, cpy_avail;
		    vertex_hdl_t vertex;

		    dprintf(5, ("clshm: DAEM_TO_DRV_NEED_PATH\n"));
		    /* check the message */
		    bcopy(msg.body, &shmid, sizeof(shmid_in_msg));
		    if(msg.len != sizeof(shmid_in_msg) ||
		       SHMID_TO_PARTITION(shmid) != part_id()) {
			error = EINVAL;
			dprintf(1, ("clshm: bad length or shmid partition "
				    "to NEED_PATH\n"));
			break;
		    }
		    
		    /* find the local map */
		    for(loc_map = ctl_dev->loc_page_info; loc_map != NULL; 
			loc_map = loc_map->next) {
			if(loc_map->shmid == shmid) {
			    break;
			}
		    }
		    if(!loc_map) {
			error = EINVAL;
			dprintf(1, ("clshm: can't find shmid locally\n"));
			break;
		    }

		    /* get the node path and put it in the message if
		     * the memory has a node associated with it. */
		    cpy_ptr = msg.body + sizeof(shmid);
		    cpy_len = sizeof(shmid);
		    cpy_avail = sizeof(msg.body) - sizeof(shmid);
		    if(loc_map->nodeid != CNODEID_NONE) {
			vertex = cnodeid_to_vertex(loc_map->nodeid);
			name = vertex_to_name(vertex, cpy_ptr, cpy_avail);
			if(name == cpy_ptr) {
			    cpy_len += strlen(cpy_ptr);
			}
		    }

		    msg.type = DRV_TO_DAEM_TAKE_PATH;
		    msg.len = cpy_len;
		    error = send_to_clshmd(ctl_dev, &msg);
		    break;
		}

	        case DAEM_TO_DRV_RMID: {
		    error = remove_local_remote_shmid(ctl_dev, &msg);
		    break;
		}

	    	case DAEM_TO_DRV_PART_DEAD:  {

		    dprintf(5, ("clshm: DAEM_TO_DRV_PART_DEAD Partn %d "
				"dead\n", msg.src_part));
		    if(msg.src_part != part_id()) {
			signal_processes(ctl_dev, msg.src_part);
			remove_part_from_maptab(ctl_dev, msg.src_part);
			ctl_dev->remote_parts[msg.src_part].state &=
			    ~CLSHM_PART_S_DAEM_CONTACT;
		    } else {
			error = EINVAL;
		    }

		    break;
		}

	        case DAEM_TO_DRV_NEW_PART: {
		    pgsz_in_msg page_size;
		    vers_in_msg vers_major, vers_minor;
		    int len;
		    partid_t part;

		    dprintf(5, ("clshm: DAEM_TO_DRV_NEW_PART Part %d "
				"coming back from the dead\n",
				msg.src_part));
		    part = msg.src_part;
		    if(ctl_dev->remote_parts[part].state != 
		       CLSHM_PART_S_PART_UP) {
			dprintf(1, ("clshm: part is %d, and the state "
				    "is perhaps bad\n", part));
			/* CLEAN UP */
			error = EINVAL;
			break;
		    }

		    /* if we are geting first message from our daemon
		     * about us, then check the version and the page
		     * size to make sure that we are compatable */
		    if(part == part_id()) {
			if(msg.len != sizeof(pgsz_in_msg) + 
			   sizeof(vers_in_msg) + sizeof(vers_in_msg)) {
			    dprintf(0, ("clshm: bad version checking call\n"));
			    error = EINVAL;
			    break;
			}
		    
			bcopy(msg.body, &vers_major, sizeof(vers_in_msg));
			len = sizeof(vers_in_msg);
			bcopy(msg.body+len, &vers_minor, sizeof(vers_in_msg));
			len += sizeof(vers_in_msg);
			bcopy(msg.body+len, &page_size, sizeof(pgsz_in_msg));
			if(vers_major != CLSHM_DRV_MAJOR_VERS) {
			    cmn_err(CE_WARN, "clshm: bad major versions, "
				    "driver = %d and deamon = %d\n", 
				    CLSHM_DRV_MAJOR_VERS, vers_major);
			    error = EINVAL;
			    break;
			} else if(page_size != ctl_dev->page_size) {
			    cmn_err(CE_WARN, "clshm: bad pagesize, "
				    "driver = %d and deamon = %d\n", 
				    ctl_dev->page_size, page_size);
			    error = EINVAL;
			    break;
			}
			ctl_dev->daem_minor_vers = vers_minor;
		    } else {
			if(msg.len != 0) {
			    dprintf(0, ("clshm: bad NEW_PART len\n"));
			    error = EINVAL;
			    break;
			}
		    }

		    ctl_dev->remote_parts[part].state |= 
			CLSHM_PART_S_DAEM_CONTACT;
		    break;
		}

	        case DAEM_TO_DRV_DUMP_DRV_STATE: {
		    dprintf(5, ("clshm: DAEM_TO_DRV_DUMP_DRV_STATE\n"));
		    dump_clshm_state(ctl_dev);
		    break;
		}

	        default: {
		    dprintf(5, ("clshm: INVALID DAEM MESSAGE PASSED\n"));
		    error = EINVAL;
		    break;
		}
	    }
	    break;
	}
    }
    UNLOCK(ctl_dev->devmutex);
    return(error);
}



/* allocate_local_shmid
 * --------------------
 * Parameters:
 *	ctl_dev:	general device info
 *	msg:		NEED_SHMID message
 *	return:		errno values
 *
 * If the local allocation has already happened for the shmid gotten
 * from the msg, then just permit the new partition to the page.  If
 * not, then allocate the new space for the user.  Make sure to make
 * a remote map entry for this as well.  Send back the message to
 * the daemon if the allocation we successful.
 */
static int allocate_local_shmid(clshm_dev_t *ctl_dev, clshm_msg_t *msg)
{
    /* shouldn't need the key ever */
    len_in_msg len;
    shmid_in_msg shmid;
    pid_in_msg pmo_pid;
    pmoh_in_msg pmo_handle;
    pmot_in_msg pmo_type;
    msgnum_in_msg msg_num;
    int page_off, cpy_len, msg_len, iaddr;
    off_t off;
    char *cpy_ptr, *msg_ptr;
    local_map_record_t *loc_map = NULL;
    remote_map_record_t *rem_map = NULL;
    int error = 0, i;
    partid_t asking_part, our_part;

    /* read from msg */
    cpy_len = sizeof(key_in_msg);
    cpy_ptr = msg->body + sizeof(key_in_msg);
    bcopy(cpy_ptr, &(len), sizeof(len_in_msg));
    cpy_len += sizeof(len_in_msg);
    cpy_ptr += sizeof(len_in_msg);

    bcopy(cpy_ptr, &(shmid), sizeof(shmid_in_msg));
    cpy_len += sizeof(shmid_in_msg);
    cpy_ptr += sizeof(shmid_in_msg);
    
    if(SHMID_TO_PARTITION(shmid) != part_id()) {
	dprintf(1, ("clshm: incorrect partid in shmid\n"));
	return(EINVAL);
    }

    /* need the cpy_ptr and cpy_len for later when sending shmid info 
     * to daemon */
    bcopy(cpy_ptr, &(pmo_pid), sizeof(pid_in_msg));
    bcopy(cpy_ptr + sizeof(pid_in_msg), &(pmo_handle), sizeof(pmoh_in_msg));
    bcopy(cpy_ptr + sizeof(pid_in_msg) + sizeof(pmoh_in_msg), &(pmo_type), 
	  sizeof(pmot_in_msg));


    if(msg->len != cpy_len + sizeof(pid_in_msg) + sizeof(pmoh_in_msg) + 
                   sizeof(pmot_in_msg)) {
	dprintf(1, ("clshm: received bad NEED_SHMID message length\n"));
	return(EINVAL);
    }

    /* extract vital info from the msg info */
    page_off = SHMID_TO_OFFSET(shmid);
    off = page_off * ctl_dev->page_size;
    asking_part = msg->src_part;
    our_part = part_id();

    /* check to make sure asking partition is still up */
    if(ctl_dev->remote_parts[asking_part].state != 
       (CLSHM_PART_S_PART_UP|CLSHM_PART_S_DAEM_CONTACT)) {
	dprintf(0, ("clshm: partitioning asking for permission to page "
		    "is not up %d\n", asking_part));
	return(EINVAL);
    }

    if(ctl_dev->remote_parts[our_part].state !=
       (CLSHM_PART_S_PART_UP|CLSHM_PART_S_DAEM_CONTACT)) {
	dprintf(0, ("clshm: my own partition when down!!!\n"));
	/* SHUTDOWN */
	return(EFAULT);
    }


    /* if shmid is already in local map, then we just need to grant
     * permission for this asking partition */
    for(loc_map = ctl_dev->loc_page_info; loc_map != NULL; 
	loc_map = loc_map->next) {
	if(loc_map->shmid == shmid) {
	    for(i = 0; i < loc_map->num_pages; i++) {
		part_permit_page(asking_part, loc_map->pages[i].pfdat, 
				 pAccessRW);
	    }

	    /* the daemon should already know about this so it isn't
	     * really expecting a new TAKE_SHMID return message here */
	    dprintf(5, ("clshm: permitting part %d to shmid 0x%x\n",
			asking_part, shmid));
	    return(error);
	}
    }

    /* sanity check that the shmid isn't in our remote maps as well */
    for(rem_map = ctl_dev->remote_parts[our_part].rem_page_info; 
	rem_map != NULL; rem_map = rem_map->next) {
	if(rem_map->shmid == shmid) {
	    dprintf(0, ("clshm: found same shmid in remote mappings\n"));
	    return(EINVAL);
	}
    }

    /* allocate the local pages */
    error = allocate_cross_pages(ctl_dev, shmid, off, len, 0, (pid_t) pmo_pid, 
				 pmo_handle, (pmo_type_t) pmo_type,
				 asking_part, &(ctl_dev->loc_page_info));

    if(error) {
	dprintf(0, ("clshm WARNING: can't allocate cross page\n"));
	return(error);
    }

    loc_map = ctl_dev->loc_page_info;

    /* send back to daem -- do so in various messages if can't fit into
     * just one message */
    msg->type = DRV_TO_DAEM_TAKE_SHMID;
    msg->src_part = our_part;
    msg->dst_part = asking_part;
    iaddr = 0;
    msg_num = 0;
    while(iaddr < loc_map->num_pages) {
	msg_len = cpy_len;
	msg_ptr = cpy_ptr;
	bcopy(&msg_num, msg_ptr, sizeof(msgnum_in_msg));
	msg_len += sizeof(msgnum_in_msg);
	msg_ptr += sizeof(msgnum_in_msg);

	for(i = 0; i < ADDRS_PER_MSG && iaddr < loc_map->num_pages; i++) {
	    bcopy(&(loc_map->pages[iaddr].kpaddr), msg_ptr, 
		  sizeof(kpaddr_in_msg));
	    msg_len += sizeof(kpaddr_in_msg);
	    msg_ptr += sizeof(kpaddr_in_msg);
	    iaddr++;
	}
	msg->len = msg_len;
	if(error = send_to_clshmd(ctl_dev, msg)) {
	    dprintf(5, ("clshm: trouble sending to daem\n"));
	    error = EFAULT;
	    break;
	}
	msg_num++;
    }

    /* make the remote map record for this local shmid */
    if(!error) {
	rem_map = make_new_remote_map_record(ctl_dev, our_part, shmid, 
						 len, off);
	rem_map->ready = 1;
	for(i = 0; i < rem_map->num_pages; i++) {
	    rem_map->paddrs[i] = loc_map->pages[i].kpaddr;
	    rem_map->pfns[i] = pnum(rem_map->paddrs[i]);
	    /* don't register with part_register_page -- same pool as
	     * part_allocate_page as far as avl and conflicts */
	}
	for(i = 0; i < rem_map->num_msgs; i++) {
	    rem_map->msg_recv[i] = 1;
	}
    } else {

	/* there has been an error so release the local shmid info */
	for(i = 0; i < loc_map->num_pages; i++) {
	    if(loc_map->pages[i].pfdat) {
		dprintf(5, ("clshm: page_part_free: pfdat 0x%x\n", 
			    loc_map->pages[i].pfdat));
		part_page_free(loc_map->pages[i].pfdat, ctl_dev->page_size);
	    }
	}
	ctl_dev->loc_page_info = loc_map->next;
	kmem_free(loc_map->pages, sizeof(kernel_page_t) * 
		  loc_map->num_pages);
	kmem_free(loc_map, sizeof(local_map_record_t));
    }	

    return(error);
}


/* allocate_remote_shmid
 * ---------------------
 * Parameters:
 *	ctl_dev:	general device info
 *	msg:		message with the remote shmid info in it
 *	return:		errno values
 *
 * Parse through the message and make sure things are sane.  Try putting
 * this message into the remote map record that we have for this shmid.
 * If we have completely gotten the shmid info, then set the space as
 * done and register all the remote pages.
 */
static int allocate_remote_shmid(clshm_dev_t *ctl_dev, clshm_msg_t *msg)
{
    /* shouldn't need the key ever */
    __uint32_t cpy_len;
    int iaddr, page_off, i, error = 0, created_new_rem = 0;
    off_in_msg off;
    char *cpy_ptr;
    len_in_msg len;
    shmid_in_msg shmid;
    msgnum_in_msg msg_num;
    partid_t remote_part;
    remote_map_record_t *rem_map, *rem_prev;

    /* parse message beginning */
    cpy_ptr = msg->body + sizeof(key_in_msg);
    cpy_len = sizeof(key_in_msg);
    bcopy(cpy_ptr, &len, sizeof(len_in_msg));
    cpy_ptr += sizeof(len_in_msg);
    cpy_len += sizeof(len_in_msg);
    
    bcopy(cpy_ptr, &(shmid), sizeof(shmid_in_msg)); 
    cpy_len += sizeof(shmid_in_msg);
    cpy_ptr += sizeof(shmid_in_msg);

    if(shmid == CLSHMD_ABSENT_SHMID) {
	dprintf(0, ("clshm: got shmid == CLSHM_ABSENT_SHMID!!!!!\n"));
	/* we shouldn't need to ever remove this key.  We shouldn't
	 * get to this case.... */
	return(EINVAL);
    }

    bcopy(cpy_ptr, &msg_num, sizeof(msgnum_in_msg));
    cpy_ptr += sizeof(msgnum_in_msg);
    cpy_len += sizeof(msgnum_in_msg);

    remote_part = SHMID_TO_PARTITION(shmid);
    page_off = SHMID_TO_OFFSET(shmid);
    off = page_off * ctl_dev->page_size;

    if(remote_part >= MAX_PARTITIONS || remote_part < 0 || 
       ctl_dev->remote_parts[remote_part].state !=
                        (CLSHM_PART_S_PART_UP|CLSHM_PART_S_DAEM_CONTACT)) {
	dprintf(5, ("clshm: got msg from partition not up or bad %d\n", 
		    remote_part));
	return(EINVAL);
    }

    /* see if we have seen this shmid before */
    for(rem_prev = NULL,
	    rem_map = ctl_dev->remote_parts[remote_part].rem_page_info; 
	rem_map != NULL; rem_prev = rem_map, rem_map = rem_map->next) {
	if(rem_map->shmid == shmid) {
	    break;
	}
    }

    if(!rem_map) {
	if(msg_num != 0) {
	    dprintf(1, ("clshm: got out of order message from partition %d, "
			"received %d as first message\n", remote_part, 
			msg_num));
	    return(EINVAL);
	}
	rem_prev = NULL;
	rem_map = make_new_remote_map_record(ctl_dev, remote_part, shmid, 
					     len, off);
	created_new_rem = 1;
    }

    /* check to make sure it is sane before inserting it */
    if(rem_map->ready || rem_map->len != len  || rem_map->off != off ||
       msg_num >= rem_map->num_msgs || rem_map->msg_recv[msg_num]) {
	dprintf(5, ("clshm: clshmd sent us a bad TAKE_SHMID message\n"));
	if(!created_new_rem) {
	    dprintf(1, ("clshm: someone is sending us a bogus/dup message: "
			"ignor it\n"));
	    return(EINVAL);
	} else {
	    ctl_dev->remote_parts[remote_part].rem_page_info = rem_map->next;
	    kmem_free(rem_map->paddrs, sizeof(paddr_t) * rem_map->num_pages);
	    kmem_free(rem_map->pfns, sizeof(pfn_t) * rem_map->num_pages);
	    kmem_free(rem_map->msg_recv, sizeof(uint) * rem_map->num_msgs);
	    kmem_free(rem_map, sizeof(remote_map_record_t));
	    return(EINVAL);
	}
    } else {
	for(iaddr = msg_num * ADDRS_PER_MSG; 
	    (cpy_len + sizeof(kpaddr_in_msg)) <= msg->len; iaddr++) {
	    bcopy(cpy_ptr, &(rem_map->paddrs[iaddr]), sizeof(kpaddr_in_msg));
	    cpy_ptr += sizeof(kpaddr_in_msg);
	    cpy_len += sizeof(kpaddr_in_msg);
	    rem_map->pfns[iaddr] = pnum(rem_map->paddrs[iaddr]);
	}
    }
	
    if(!error && cpy_len != msg->len) {
	dprintf(0, ("clshm: bad len for getting kaddrs\n"));
	error = EINVAL;
    }

    /* see if we have completed getting message for this shmid */
    if(!error) {
	rem_map->msg_recv[msg_num] = 1;
	rem_map->ready = 1;
	for(msg_num = 0; msg_num < rem_map->num_msgs; msg_num++) {
	    if(!rem_map->msg_recv[msg_num]) {
		rem_map->ready = 0;
		break;
	    }
	}

	if(rem_map->ready) {
	    for(i = 0; i < rem_map->num_pages; i++) {
		if(!part_register_page(rem_map->paddrs[i], ctl_dev->page_size, 
				       clshm_err_handler)) {
		    dprintf(5, ("clshm: can't register cross partition page: "
				"%x\n",	rem_map->paddrs[i]));
		    error = EINVAL;
		    break;
		} 
		dprintf(5, ("clshm registered cross-part page 0x%x\n",
			    rem_map->paddrs[i]));
	    }
	}
	if(error) {
	    for(i--; i >= 0; i--) {
		part_unregister_page(rem_map->paddrs[i]);
	    }
	}
    }
	    
    if(error) {
	if(rem_prev) {
	    rem_prev->next = rem_map->next;
	} else {
	    ctl_dev->remote_parts[remote_part].rem_page_info = rem_map->next;
	}
	kmem_free(rem_map->paddrs, sizeof(paddr_t) * rem_map->num_pages);
	kmem_free(rem_map->pfns, sizeof(pfn_t) * rem_map->num_pages);
	kmem_free(rem_map->msg_recv, sizeof(uint) * rem_map->num_msgs);
	kmem_free(rem_map, sizeof(remote_map_record_t));
    }
    return(error);
}



/* allocate_cross_pages
 * --------------------
 * Parameters:
 *	ctl_dev:	general clshm_dev
 *	shmid:		shmid for this allocation
 *	off:		offset of the allocation in the shm file
 *	len:		length of this allocation
 *	prot:		protection: NOT USED
 *	pmo_pid:	pid for the pmo_handle that we are passed
 *	pmo_handle:	pmo handle for where to allocate pages
 *	pmo_type:	pmo type for where to allocate pages
 *	asking_part:	what partition wants these pages
 *	place_map_rec:	a pointer to what should point to this new
 *			local page info that we create
 *	return:		errno values
 *
 * Someone wants access to these new pages.  So go through and ask the
 * partition code for the appropriate number of pages (in NBPP chunks)
 * so that we can map this section to the user (partition that wants it).
 */
/* ARGSUSED */
static int allocate_cross_pages(clshm_dev_t *ctl_dev, int shmid, off_t off, 
				int len, int prot, pid_t pmo_pid, 
				pmo_handle_t pmo_handle,
				pmo_type_t pmo_type, partid_t asking_part,
				local_map_record_t **place_map_rec)
{
    int error = 0, alloc_flags, i;
    local_map_record_t *loc_rec = NULL;
    part_error_handler_t handler_ptr;
    caddr_t kvaddr;
    mldref_t mld = NULL;

    handler_ptr = (part_error_handler_t) clshm_err_handler;

    /* make a new local record so we can save that we have allowed this
     * to other partition */
    loc_rec = kmem_zalloc(sizeof(local_map_record_t), KM_SLEEP);
    loc_rec->shmid = shmid;
    loc_rec->len = len;
    loc_rec->off = off;
    loc_rec->nodeid = CNODEID_NONE;
    loc_rec->num_pages = btoc(len);
    loc_rec->pages = kmem_zalloc(sizeof(kernel_page_t) * 
				   loc_rec->num_pages, KM_SLEEP);
    
    dprintf(5, ("clshm Allocating %d bytes (%d pgs) kvspace\n", len, 
		btoc(len)));
    
    /* set the alloc flags to send into part_allocate_page */
    alloc_flags = /*VM_NOSLEEP|*/ VM_DIRECT | VM_PHYSCONTIG | VM_CACHEALIGN;

    /* if someone has specified where we should place this memory, then
     * set it here */
    if(pmo_handle != CLSHMD_ABSENT_PMO_HANDLE && 
       pmo_type != CLSHMD_ABSENT_PMO_TYPE) {
	dprintf(5, ("clshm: got %d for pmo, %d for type\n", pmo_handle, 
		    pmo_type));

	/* make sure that the mld is released after this function
	 * gets it 	pmo_decref(mld, pmo_ref_find); */
	mld = find_mld(pmo_pid, pmo_handle, pmo_type);

	if(mld == NULL)  {
	    dprintf(0, ("clshm: invalid MLD passed in NEED SHMID\n"));
	    error = EINVAL;
	    kmem_free(loc_rec->pages, 
		      sizeof(kernel_page_t) * loc_rec->num_pages);
	    kmem_free(loc_rec, sizeof(local_map_record_t));
	    return(error);
	}
	
	if(!mld_isplaced(mld))  {
	    dprintf(0, ("clshm: MLD passed in NEED SHMID hasn't been "
			"placed yet\n"));
	    error = EINVAL;
	    pmo_decref(mld, pmo_ref_find);
	    kmem_free(loc_rec->pages, 
		      sizeof(kernel_page_t) * loc_rec->num_pages);
	    kmem_free(loc_rec, sizeof(local_map_record_t));
	    return(error);
	}
	loc_rec->nodeid = mld_getnodeid(mld);

	if(loc_rec->nodeid == CNODEID_NONE) {
	    error = EFAULT;
	    pmo_decref(mld, pmo_ref_find);
	    kmem_free(loc_rec->pages, 
		      sizeof(kernel_page_t) * loc_rec->num_pages);
	    kmem_free(loc_rec, sizeof(local_map_record_t));
	    return(error);
	}
	/* we no longer need the mld cause we have decided on the
	 * node placement */
	pmo_decref(mld, pmo_ref_find);
    } 

    /* go through and get all the pages that we need */
    for(i = 0; i < loc_rec->num_pages; i++) {
	if(loc_rec->nodeid == CNODEID_NONE) {
	    loc_rec->pages[i].pfdat = 
		part_page_allocate(asking_part, pAccessRW, 0, alloc_flags,
				   ctl_dev->page_size, handler_ptr);
	} else {
	    loc_rec->pages[i].pfdat = 
		part_page_allocate_node(loc_rec->nodeid, asking_part, 
					pAccessRW, 0, alloc_flags, 
					ctl_dev->page_size, handler_ptr);
	    
	}
	dprintf(5, ("clshm Space allocated for clshm: pfdat %x, "
		    "handler %x\n", loc_rec->pages[i].pfdat, 
		    handler_ptr));
	
	if(!loc_rec->pages[i].pfdat)  {
	    dprintf(0, ("clshm Couldn't allocate space in mmap\n"));
	    error = ENOMEM;
	    break;
	}
    
	/* get kvaddr so we can bzero the space before giving it to 
	 * the user */
	loc_rec->pages[i].kpaddr = pfdattophys(loc_rec->pages[i].pfdat);

	/* uncached access */
	/* kvaddr = (caddr_t) PHYS_TO_K1(loc_rec->pages[i].kpaddr);
	 * dprintf(5, ("clshm Got uncached mode; kvaddr is %x, kpaddr  "
	 *             "%x\n", kvaddr, 
	 *             loc_rec->pages[i].kpaddr));
	 * part_poison_page(loc_rec->pages[i].pfdat);
	 */

	kvaddr = (caddr_t) PHYS_TO_K0(loc_rec->pages[i].kpaddr);
	dprintf(5, ("clshm Got cached mode; kvaddr is %x kpaddr %x\n",
		    kvaddr, loc_rec->pages[i].kpaddr));

	/* zero out the page */
	bzero(kvaddr, ctl_dev->page_size);
	
	if(IS_KSEG0(kvaddr))  {
	    dprintf(5, ("clshm kvaddr is in K0 seg\n"));
	}
	if(IS_KSEG1(kvaddr))  {
	    dprintf(5, ("clshm kvaddr is in K1 seg\n"));
	}
	dprintf(5, ("clshm kvspace is at %x, length %d bytes, off %llx "
		    "(%llx)\n", kvaddr, ctl_dev->page_size, off, 
		    loc_rec->off));
    }

    /* we had an error, clean up all the stuff that we allocated before
     * we return the error */
    if(error) {
	for(; i >=0; i--) {
	    if(loc_rec->pages[i].pfdat) {
		dprintf(5, ("clshm: page_part_free: pfdat 0x%x\n", 
			    loc_rec->pages[i].pfdat));
		part_page_free(loc_rec->pages[i].pfdat, ctl_dev->page_size);
	    }
	}
	kmem_free(loc_rec->pages, sizeof(kernel_page_t) * 
		  loc_rec->num_pages);
	kmem_free(loc_rec, sizeof(local_map_record_t));	

    } else {

	if(loc_rec->nodeid == CNODEID_NONE) {
	    /* even if we didn't place the local record on a specific node,
	     * see if the all pages go onto the same node -- then we can have
	     * a valid nodeid */
	    cnodeid_t nodeid = pfdattocnode(loc_rec->pages[0].pfdat);
	    for(i = 1; i < loc_rec->num_pages; i++) {
		if(pfdattocnode(loc_rec->pages[i].pfdat) != nodeid) {
		    break;
		}
	    }
	    /* got all the way through so we have all pages on the same
	     * node!!! */
	    if(i == loc_rec->num_pages) {
		loc_rec->nodeid = nodeid;
	    }
	}

	/* place on the local page list */
	loc_rec->next = (*place_map_rec);
	(*place_map_rec) = loc_rec;
    }

    return(error);
}


/* make_new_remote_map_record
 * --------------------------
 * Parameters:
 *	ctl_dev:	general device info
 *	part:		partition to put this new remote map record on
 *	shmid:		shmid for this new remote map record
 *	len:		length of new record
 *	off:		offset of new record
 *	return:		new remote_map_record_t
 *
 * Just allocate all the memory needed for this new remote map record
 * and set as many of the fields as we can right now.
 */
static remote_map_record_t * make_new_remote_map_record(clshm_dev_t *ctl_dev, 
			          partid_t part, shmid_in_msg shmid, 
				  len_in_msg len, off_t off)
{
    remote_map_record_t *rem_map;

    rem_map = kmem_zalloc(sizeof(remote_map_record_t), KM_SLEEP);
    rem_map->shmid = shmid;
    rem_map->len = len;
    rem_map->off = off;
    rem_map->num_pages = btoc(len);
    rem_map->paddrs = kmem_zalloc(sizeof(paddr_t) * 
				  rem_map->num_pages, KM_SLEEP);
    rem_map->pfns = kmem_zalloc(sizeof(pfn_t) * rem_map->num_pages, KM_SLEEP);
    rem_map->num_msgs = (rem_map->num_pages / ADDRS_PER_MSG) +
	((rem_map->num_pages % ADDRS_PER_MSG) ? 1 : 0);
    rem_map->msg_recv = kmem_zalloc(sizeof(uint) * rem_map->num_msgs,
				    KM_SLEEP);
    bzero(rem_map->msg_recv, rem_map->num_msgs);
    rem_map->ready = 0;
    rem_map->count = 0;

    rem_map->next = ctl_dev->remote_parts[part].rem_page_info;
    ctl_dev->remote_parts[part].rem_page_info = rem_map;

    return(rem_map);
}


/* find_mld
 * --------
 * Parameters:
 *	pmo_pid:	pid of uthread we want
 *	pmo_handle:	pmo_handle to get mld from
 *	pmo_type:	type of pmo_handle given
 * 	return:		mld of pmo_handle/pid
 *	
 * convert a pid to a uthread pointer and then to mld for the pmo_handle
 * parameter.  Null is return of error.  Uses the appropriate calls to 
 * make sure that things won't get removed out from under us while we 
 * are accessing the proc, vproc, etc...
 *
 * NOTE: the value returned from this function must have 
 *	pmo_decref(<return value>, pmo_ref_find);
 * 	called on it because the pmo_ns_find function call increments a
 * 	reference count on this pmo object.
 */
static 	mldref_t find_mld(pid_t pmo_pid, pmo_handle_t pmo_handle,
			  pmo_type_t pmo_type)
{
    vproc_t *vproc;
    proc_t *proc, *real_proc;
    uthread_t *ut;
    pmo_ns_t *pmo_ns;

    /* get the vproc and proc */
    if ((vproc = VPROC_LOOKUP(pmo_pid)) == NULL) {
	dprintf(1, ("clshm: find_mld failed for pid_to_vproc, pid: %d\n",
		    pmo_pid));
	return(NULL);
    }

    VPROC_GET_PROC(vproc, &proc);
    if(!proc) {
	dprintf(1, ("clshm: find_mld failed for VPROC_GET_PROC, pid: %d\n",
		    pmo_pid));
	VPROC_RELE(vproc);
	return(NULL);
    }

    /* hold the proxy list of uthreads and find a uthread we can use */
    uscan_hold(&(proc->p_proxy));
    ut = prxy_to_thread(&(proc->p_proxy));
    
    for ( ; ut != NULL; ut = ut->ut_next) {
	/* I assume that all the threads are actually pointing to the proc
	 * so that the first one will succeed and break */
	real_proc = UT_TO_PROC(ut);
	if(real_proc && real_proc->p_pid == pmo_pid) {
	    dprintf(5, ("clshm: find_mld found uthread for pid: %d\n", 
			pmo_pid));
	    break;
	}
    }
    
    if(!ut) {
	dprintf(1, ("clshm: find_mld failed looking for ut in proc for pid: "
		    "%d\n", pmo_pid));
	uscan_rele(&proc->p_proxy);
	VPROC_RELE(vproc);
	return(NULL);
    }

    /* get the pmo_ns and we are done with the uthread and proc/vproc */
    pmo_ns = getpmo_ns(ut->ut_asid);
    uscan_rele(&proc->p_proxy);
    VPROC_RELE(vproc);

    if(!pmo_ns) {
	dprintf(1, ("clshm: find_mld failed getting pmo_ns for pid: %d\n", 
		    pmo_pid));
	return(NULL);
    }
    return(pmo_ns_find(pmo_ns, pmo_handle, pmo_type));
}


/* remove_local_remote_shmid
 * -------------------------
 * Parameters:
 *	ctl_dev:	general device info
 *	msg:		RMID message we received
 *	return:		errno values
 *
 * Go through and remove this shmid from our local pool as well as our
 * remote pool of info.
 */
static int remove_local_remote_shmid(clshm_dev_t *ctl_dev, clshm_msg_t *msg)
{
    shmid_in_msg shmid;
    partid_t part;
    int error = 0;

    bcopy(msg->body, &shmid, sizeof(shmid_in_msg));
    part = SHMID_TO_PARTITION(shmid);

    if(part >= MAX_PARTITIONS || part < 0 ||
       ctl_dev->remote_parts[part].state !=
                          (CLSHM_PART_S_PART_UP|CLSHM_PART_S_DAEM_CONTACT)) {
	dprintf(5, ("clshm: remove_local_remote_shmid bad partition %d\n",
		    part));
	return(EINVAL);
    }
    
    error = remove_remote_shmid(ctl_dev, part, shmid);
    if(error) {
	dprintf(5, ("clshm: error in removing remote shmid\n"));
    }
    if(part == part_id()) {
	error = remove_local_shmid(ctl_dev, shmid);
    }
    return(error);
}


/* remove_local_shmid
 * ------------------
 * Parameters:
 *	ctl_dev:	general device info
 *	shmid:		shmid to remove locally
 *	return:	
 *
 * See if we can find this shmid in the local pages we have mapped to
 * other partitions and then destoy it.
 */
static int remove_local_shmid(clshm_dev_t *ctl_dev, shmid_in_msg shmid)
{
    local_map_record_t *elem, *prev;
    int i;

    for(prev = NULL, elem = ctl_dev->loc_page_info; elem != NULL;
	prev = elem, elem = elem->next) {
	if(elem->shmid == shmid) {
	    break;
	}
    }

    if(!elem) {
	dprintf(5, ("clshm: can't find shmid %x in local map\n", shmid));
	return(EINVAL);
    }
    if(prev) {
	prev->next = elem->next;
    } else {
	ctl_dev->loc_page_info = elem->next;
    }

    for(i = 0; i < elem->num_pages; i++) {
	dprintf(5, ("clshm: page_part_free: pfdat 0x%x\n", 
		    elem->pages[i].pfdat));
	part_page_free(elem->pages[i].pfdat, ctl_dev->page_size);
    }

    kmem_free(elem->pages, sizeof(kernel_page_t) * elem->num_pages);
    kmem_free(elem, sizeof(local_map_record_t));
    return(0);
}

/* remove_remote_shmid
 * -------------------
 * Parameters:
 *	ctl_dev:	general device info
 *	part:		index of partition to remove remote shmid
 *	shmid:		shmid to remove
 *	return:		errno values
 *
 * Remove the remote shmid.  Do this by removeing the remote map
 * record and going through all the maps and killing all processes that
 * think they have access to this page.
 */
static int remove_remote_shmid(clshm_dev_t *ctl_dev, partid_t part, 
			       shmid_in_msg shmid)
{
    remote_map_record_t *rem_elem, *rem_prev;
    map_tab_entry_t *map_elem, *map_prev;
    int i;

    /* find the remote page info */
    for(rem_prev = NULL, rem_elem = 
	    ctl_dev->remote_parts[part].rem_page_info; 
	rem_elem != NULL; rem_prev = rem_elem, rem_elem = rem_elem->next) {
	if(rem_elem->shmid == shmid) {
	    break;
	}
    }

    if(!rem_elem) {
	dprintf(5, ("clshm: can't find shmid %x in remote_map_record to "
		    "to remove\n", shmid));
	return(EINVAL);
    }

    if(rem_prev) {
	rem_prev->next = rem_elem->next;
    } else {
	ctl_dev->remote_parts[part].rem_page_info = rem_elem->next;
    }

    if(rem_elem->count) {
	dprintf(5, ("clshm: RMID of still attached segment, %d attaches\n",
		    rem_elem->count));
    }
    if(!rem_elem->ready) {
	dprintf(5, ("clshm: RMID of still unready segment %x shmid\n",
		    rem_elem->shmid));
    }

    /* remove all maps that have this specified.  Kill all processes
     * that think they have access to this page.
     */
    for(map_prev = NULL, map_elem = ctl_dev->remote_parts[part].map_tab;
	map_elem != NULL; ) {
	if(map_elem->page_info->shmid == shmid) {
	    /* kill off process that hasn't detached */
	    /* for now, just let them hit recovery code when they
	     * don't have access */
	    dprintf(5, ("clshm: detaching pid %d, gid %d, shmid %x\n",
			map_elem->pid, map_elem->gid, shmid));
	    (rem_elem->count)--;
	    if(map_prev) {
		map_prev->next = map_elem->next;
		kmem_free(map_elem, sizeof(map_tab_entry_t));
		map_elem = map_prev->next;
	    } else {
		ctl_dev->remote_parts[part].map_tab = map_elem->next;
		kmem_free(map_elem, sizeof(map_tab_entry_t));
		map_elem = ctl_dev->remote_parts[part].map_tab;
	    }
	} else {
	    map_prev = map_elem;
	    map_elem = map_elem->next;
	}
    }
    if(rem_elem->count) {
	dprintf(5, ("clshm: remote_map_record_t count not 0, %d\n",
		    rem_elem->count));
    }

    /* only unregister if element was ready (was registered).  We don't
     * register our own pages because partition.c uses same avl tree
     * for local and remote pages */
    if(rem_elem->ready && SHMID_TO_PARTITION(rem_elem->shmid) != part_id()) {
	for(i = 0; i < rem_elem->num_pages; i++) {
	    part_unregister_page(rem_elem->paddrs[i]);
	}
	rem_elem->ready = 0;
    } else {
	dprintf(5, ("clshm: no need to unregister pages: didn't get all "
		    "pages, or local pages!!\n"));
    }
    kmem_free(rem_elem->paddrs, sizeof(paddr_t) * rem_elem->num_pages);
    kmem_free(rem_elem->pfns, sizeof(pfn_t) * rem_elem->num_pages);
    kmem_free(rem_elem->msg_recv, sizeof(uint) * rem_elem->num_msgs);
    kmem_free(rem_elem, sizeof(remote_map_record_t));
    return(0);
}


/* clshmmap
 * --------
 * Parameters:
 *	dev:	device info
 *	vt:	vhandle of user mmap
 *	off:	offset of mmap
 *	len:	length of mmap
 *	prot:	protection flag
 *	return:	errno values
 *
 * Send to either shm_mmap or ctl_mmap for real work.
 */
/* ARGSUSED */
int clshmmap(dev_t dev, vhandl_t *vt, off_t off, int len, int prot)
{
    clshm_dev_info_t    *dev_info;


    dev_info = (clshm_dev_info_t *) device_info_get(dev);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in mmap of clshm\n");
    }
    if(IS_SHM_DEV(dev_info)) {
	return(shm_mmap(dev, vt, off, len, prot));
    } else if(IS_CTL_DEV(dev_info)) {
	return(ctl_mmap(dev, vt, off, len, prot));
    }
    return(EINVAL);
}



/* ctl_mmap
 * --------
 * Parameters:
 *	dev:	device info
 *	vt:	vhandle for mmap
 *	off:	offset of mmap IGNORED!
 *	len:	length of mmap
 *	prot:	NOT USED
 *	return:	errno values
 *
 * Map the control device.  Do this only if it hasn't been mapped (only
 * the daemon should be doing this).  
 */
/* ARGSUSED */
static int ctl_mmap(dev_t dev, vhandl_t *vt, off_t off, int len, int prot)
{
    int         error = 0;
    int         btoc_len = btoc(len);
    clshm_dev_t   *ctl_dev ;
    clshm_dev_info_t    *dev_info;

#   ifdef	CLSHM_DEBUG
    u_int   	unit;
#   endif	/* CLSHM_DEBUG */

    dprintf(7, ("clshm: in ctl_mmap\n"));

    /* already determined that it's a ctl device */

    dev_info = (clshm_dev_info_t *) device_info_get(dev);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in ctl map in clshm\n");
    }
    ASSERT(IS_CTL_DEV(dev_info));

#   ifdef	CLSHM_DEBUG
    unit = dev_info->unit;
#   endif	/* CLSHM_DEBUG */

    ctl_dev = (clshm_dev_t *) dev_info->dev_parms;


    CHECK_CL_SHM_C(ctl_dev);
    LOCK(ctl_dev->devmutex);

    /* make sure agree on the size with daemon */
    if(btoc_len != ctl_dev->clshmd_pages)  {
	dprintf(0, ("ctl clshmmap: lengths in mmap and ctl-configured dont't "
		    "match\n"));
	UNLOCK(ctl_dev->devmutex);
	return(EINVAL);
    }

    /* check the flags */
    if(!(ctl_dev->c_state & CL_SHM_C_ST_INITTED) || 
       (ctl_dev->c_state & CL_SHM_C_ST_SHUT)) {
	dprintf(0, ("clshm WARNING: control device not in init state\n"));
	UNLOCK(ctl_dev->devmutex);
	return(EFAULT);
    }
    if(ctl_dev->c_state & CL_SHM_C_ST_MMAPPED)  {
	dprintf(0, ("clshm WARNING: mmap was already performed on clshm "
		    "device %d\n", unit));
	UNLOCK(ctl_dev->devmutex);
	return(EINVAL);
    }

    /* allocate the memory, and map it in */
    dprintf(5, ("clshm Allocating %d bytes (%d pgs) kvspace for ctl\n",
        len, btoc(len)));
    ctl_dev->clshmd_buff = (caddr_t) kvpalloc(btoc(len), 
		    /* VM_NOSLEEP | */  VM_CACHEALIGN, 0);
    if(ctl_dev->clshmd_buff == NULL)  { 
        dprintf(0, ("clshm Couldn't allocate space in ctl-mmap\n"));
	UNLOCK(ctl_dev->devmutex);
	return(ENOMEM);
    }
    dprintf(10, ("clshm unit %d: clshmd buffer allocated\n", unit));

    error = v_mapphys(vt, ctl_dev->clshmd_buff, len);
    if(error)  {
        cmn_err(CE_WARN, "clshmmap: ctl v_mapphys status %d\n", 
	    error);
	UNLOCK(ctl_dev->devmutex);
	return(error);
    }

    ctl_dev->clshmd_ref = proc_ref();
    ctl_dev->clshmd_ID = v_gethandle(vt);
    ctl_dev->c_state |= CL_SHM_C_ST_MMAPPED;
    dprintf(15, ("clshm ctl dev: clshmd space at 0x%x len %d pgs\n",
	ctl_dev->clshmd_buff, ctl_dev->clshmd_pages));

    dprintf(15, ("clshm: handlers for part up/down registered\n"));

    UNLOCK(ctl_dev->devmutex);
    return(0);
}



/* shm_mmap
 * --------
 * Parameters:
 *	dev:	device pointer
 *	vt:	vhandle of user
 *	off:	offset passed by user (shmid in it shifted)
 *	len:	length of map
 *	prot:	not used -- protection bits
 *	return:	errno values
 *
 * 
 */
/* ARGSUSED */
static int shm_mmap(dev_t dev, vhandl_t *vt, off_t off, int len, int prot)
{
    clshm_dev_t  *ctl_dev;
    int		error = 0;
    partid_t 	part;
    shmid_in_msg shmid;
    remote_map_record_t *rem_map;
    clshm_dev_info_t *dev_info;

    dprintf(7, ("clshm: in shm_mmap\n"));

    /* already determined that it's a shm device */
    dev_info = (clshm_dev_info_t *) device_info_get(dev);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in Nctl map in clshm\n");
    }
    ASSERT(IS_SHM_DEV(dev_info));
    ctl_dev = (clshm_dev_t *) dev_info->dev_parms;

    CHECK_CL_SHM_C(ctl_dev);
    LOCK(ctl_dev->devmutex);

    /* make sure not to allow if someone is shutting down */
    if(ctl_dev->c_state & CL_SHM_C_ST_SHUT || 
       !(ctl_dev->c_state & CL_SHM_C_ST_OPENNED)) {
	cmn_err(CE_WARN, "clshm: shutdown pending, so can't map shm "
		"dev\n");
	UNLOCK(ctl_dev->devmutex);
	return(EINVAL);
    }

    /* usr passes us an offset that multiple by page size, assume that 
     * page size is a multiple of 2 and so this is really much like
     * shift */
    shmid = (shmid_in_msg) (off / ctl_dev->page_size);

    dprintf(5, ("clshm: shm_mmap: shmid %x\n", shmid));

    /* find what remote partition we want to map from */
    part = SHMID_TO_PARTITION(shmid);
    if(part >= MAX_PARTITIONS || part < 0 ||
       ctl_dev->remote_parts[part].state !=
                         (CLSHM_PART_S_PART_UP|CLSHM_PART_S_DAEM_CONTACT)) {
	dprintf(0, ("clshm: partition %d down or bad for this shmid\n",
		    part));
	UNLOCK(ctl_dev->devmutex);
	return(EINVAL);
    }

    /* make sure that we have already gotten the remote space */
    for(rem_map = ctl_dev->remote_parts[part].rem_page_info; 
	rem_map != NULL; rem_map = rem_map->next) {
	if(rem_map->shmid == shmid) break;
    }

    if(!rem_map || len != rem_map->len) {
	dprintf(0, ("clshm: need to have got a shmid before a map\n"));
	UNLOCK(ctl_dev->devmutex);
	return(EINVAL);
    }

    if(prot & PROT_WRITE)  {
	cmn_err(CE_WARN, "clshm: mapping a cross-partition area with "
		"write permissions");
    }
    error = set_maps(ctl_dev, vt, shmid, part, rem_map);

    UNLOCK(ctl_dev->devmutex);
    return(error);
}


/* set_maps
 * --------
 * Parameters:
 *	ctl_dev:	general device info
 *	vt:		vhandle of user mapping
 *	shmid:		shmid we want to set map for 
 *	part:		index to part that shmid is for
 *	rem_map:	remote map page for this setting map
 *	return:		errno values
 *
 *
 */
static int set_maps(clshm_dev_t *ctl_dev, vhandl_t *vt, shmid_in_msg shmid,
		    partid_t part, remote_map_record_t *rem_map)
{
    int		error = 0;

    dprintf(5, ("clshm: set_maps: shmid 0x%x\n", shmid));

    /* for uncached space */
    /* to pass into v_mappfns: flags = VM_UNCACHED_PIO; */
    
    /* try to set up the mapping into user space! */
    error = v_mappfns(vt, rem_map->pfns, rem_map->num_pages, 0, 0);

    if(error)  {
	cmn_err(CE_WARN, "clshmmap: v_mappfns status 0x%x\n", error);
	return(error);
    } 	
    dprintf(10, ("clshmmap: v_mappfns of xp page successful\n"));

    error = register_in_maptable(ctl_dev, vt, shmid, part, rem_map);

    if(error)  {
	dprintf(5, ("clshm: problem registering in maptable\n"));
    }

    /* kpaddr may be in remote kernel; so, doing v_gethandle(vt) here 
    ** would get a tlbmiss, and get confused about an invalid kptbl 
    ** entry 
    */
    return(error);
}


/* register_in_maptable
 * --------------------
 * Parameters:
 *	ctl_dev:	General device
 *	vt:		vhandle from the user
 *	shmid:		shmid to register in maptable
 *	part:		index of partition in remote_parts to insert this
 *	rem_map:	remote map record to user for this register
 *	return:	
 *
 * Record a mapping of a particular pid to a shmid (that we should have
 * registered already through getting the pages already).
 */
static int register_in_maptable(clshm_dev_t *ctl_dev, vhandl_t *vt, 
				shmid_in_msg shmid, partid_t part, 
				remote_map_record_t *rem_map)
{
    int	error = 0;
    map_tab_entry_t *map_elem, **last;
    ulong_t pid;
    
    /* get pid of this call */
    if((drv_getparm(PPID, &pid)) == -1)  {
        dprintf(0, ("clshm: problem getting pid in shm_mmap\n"));
        return(EINVAL);
    } 

    dprintf(10, ("clshm uaddr of shm mapping in pid %d is %x\n",
	pid, v_getaddr(vt)));

    /* make sure this pid hasn't already mapped this shmid */
    for(last = &(ctl_dev->remote_parts[part].map_tab), 
	    map_elem = *last; map_elem != NULL; 
	last = &(map_elem->next), map_elem = *last) {
	if(map_elem->pid == pid && map_elem->page_info->shmid == shmid &&
	   map_elem->vt_handle == v_gethandle(vt)) {
	    break;
	}
    }
    if(map_elem) {
	dprintf(1, ("clshm NOTICE: multiple identical mappings to "
		    "shmid: 0x%x for pid %d ignored\n", shmid, pid));
	return(EINVAL);
    }

    /* create a new map element for this mapping */
    map_elem = kmem_zalloc(sizeof(map_tab_entry_t), KM_SLEEP);
    map_elem->pid = pid;
    if(drv_getparm(PPGRP, &(map_elem->gid)) == -1)  {
	dprintf(0, ("clshm: problem getting gid in drv_getparm\n"));
	kmem_free(map_elem, sizeof(map_tab_entry_t));
	return(EINVAL);
    }
    map_elem->signal_num = SIGKILL;
    map_elem->page_info = rem_map;

    /* this is the first attach, so tell the daemon for autocleanup
     * purposes */
    if(rem_map->count == 0) {	
	clshm_msg_t msg;
	shmid_in_msg shmid;

	shmid = rem_map->shmid;
	msg.type = DRV_TO_DAEM_ATTACH;
	msg.src_part = part_id();
	msg.dst_part = SHMID_TO_PARTITION(shmid);
	msg.len = sizeof(shmid_in_msg);
	bcopy(&shmid, msg.body, sizeof(shmid_in_msg));
	
	if(error = send_to_clshmd(ctl_dev, &msg)) {
	    dprintf(0, ("clshm: can't sent to daemon\n"));
	    return(error);
	}
    }

    (rem_map->count)++;
    map_elem->err_handled = 0;
    map_elem->vt_handle = v_gethandle(vt);
    map_elem->next = ctl_dev->remote_parts[part].map_tab;
    ctl_dev->remote_parts[part].map_tab = map_elem;
    
    dprintf(15, ("clshm Registered pid %d (gid %d) len %d, off %d "
		 "sig-id %d at index %d\n",
		 map_elem->pid, map_elem->gid, map_elem->page_info->len, 
		 map_elem->page_info->off,
		 (int) map_elem->signal_num, part));
    
    return(error);
}


/* send_to_clshmd
 * --------------
 * Parameters:
 *	ctl_dev:	general device info
 *	msg:		message to send off
 *	return:		errno values
 *
 * Send a message to the daemon through the mapped memory.
 */
static int send_to_clshmd(clshm_dev_t  *ctl_dev, clshm_msg_t *msg)
{
    __uint32_t          *send_head, *send_tail, *max_send, *dummy;
    clshm_msg_t         *msg_area, *curmsg;
    __uint32_t		*cur_ptr;
    int			error = 0;

    /* check parameters */
    if(! msg)  {
	dprintf(0, ("clshm: got NULL msg ptr in send_to_clshmd\n"));
        return(EINVAL);
    }
    if(msg->len > MAX_MSG_LEN)  {
	dprintf(0, ("clshm: message too long (%d bytes, max %d)\n",
	    msg->len, MAX_MSG_LEN));
	return(EFAULT);
    }
    if(NULL == ctl_dev->clshmd_buff) {
	cmn_err(CE_WARN, "clshm: send_to_clshmd called when clshmd_buff "
		"wasn't configured; ignoring message");
	return(EFAULT);
    }

    /* set up pointers */
    send_head = (__uint32_t *) ctl_dev->clshmd_buff;
    send_tail = send_head + 1;
    max_send = send_tail + 1;
    dummy = max_send + 1;
    cur_ptr = dummy + 1;
    msg_area = (clshm_msg_t *) cur_ptr;
    curmsg = &(msg_area[*send_head]);

    /* space available? [[race with clshmd tail update?]] */
    if(*send_tail == (((*send_head) + 1) % *max_send))  {
        dprintf(0, ("clshm Run out of space in clshmd buffer \n"));
        error = ENOMEM;
    }
    else  {
        /* write a new element, and update send_head */
        /* don't assume that msg fields are contiguous */
        curmsg->type = msg->type;
        curmsg->src_part = msg->src_part;
        curmsg->dst_part = msg->dst_part;
        curmsg->len = msg->len;
	dprintf(5, ("clshm: send_to_clshmd: Msg type %c, src %d, dest %d, "
		    "len %d\n", curmsg->type, curmsg->src_part, 
		    curmsg->dst_part, curmsg->len));

        bcopy(msg->body, curmsg->body, curmsg->len);
        *send_head = ((*send_head) + 1) % *max_send;
	dprintf(5, ("clshm send_to_clshmd: sent message; head %d tail %d \n",
	    *send_head, *send_tail));
    }

    if(!error) {
	dprintf(3, ("clshm: send_to_clshmd: sent msg: type %c, src %d, "
		"dst %d, len %d\n", msg->type, msg->src_part, 
		msg->dst_part, msg->len));
    }

    return(error);
}



/* clshm_err_handler
 * -----------------
 * Parameters:
 *	efr:		eframe for this error on cross partition page
 *	phys_err_addr:	physical address error occured
 *	err_type:	enum for type of error encountered
 *	return:		send result to error or not
 *
 * Error handler registered with the partition code when we register
 * a cross partition page.  We do the recovery and either tell the user
 * that we hit an error or not.
 */
/* ARGSUSED */
static int clshm_err_handler(eframe_t *efr, paddr_t  phys_err_addr, 
			     part_error_t err_type)
{
    int		status;

    cmn_err(CE_WARN, "clshm: error on paddr 0x%x, type %d, eframe 0x%x\n",
	    phys_err_addr, err_type, efr);
    status = recover_from_error(phys_err_addr, err_type);
    if(status) {
	dprintf(5, ("clshm returning PART_ERROR_RESULT_USER \n"));
	cmn_err(CE_WARN, "clshm: error on address 0x%x seen and handled!\n", 
		phys_err_addr);
	return(PART_ERROR_RESULT_USER);	/* sends BUSERR to user */
    }
    else  {
	dprintf(5, ("clshm returning PART_ERROR_RESULT_NONE \n"));
    	return(PART_ERROR_RESULT_NONE);	/* don't know about addr! */
    }
}


/* recover_from_error
 * ------------------
 * Parameters:
 *	fault_addr:	address we got the fault on
 *	access_type:	type of fault we got
 *	return:		0 for failure and 1 for success in finding 
 *			fault_addr
 *
 * Print out a message about the access type we have hit and then
 * go though and find the place where the address coincides with.  
 * Kill all processes that reference this page.
 */
static int recover_from_error(paddr_t  fault_addr, part_error_t  access_type)
{
    paddr_t	start_phys_addr, end_phys_addr;
    int		unit;
    clshm_dev_t   *ctl_dev;
    int 	i, paddr_matched = 0, page;
    remote_map_record_t *elem, *prev;

    dprintf(0, ("clshm Recover from error, paddr 0x%x\n", fault_addr));
    dprintf(0, ("clshm err-type: "));
    switch(access_type)  {
        default:
	    cmn_err(CE_PANIC, "error 0x%x is unknown to clshm!\n", 
		    access_type);
	    break;
	case PART_ERR_RD_TIMEOUT:
	    dprintf(0, ("Read timed out\n"));
	    break;
        case PART_ERR_NACK:
	    dprintf(0, ("Nack error on page\n"));
	    break;
        case PART_ERR_WB:
	    dprintf(0, ("Write back error\n"));
	    break;
	case PART_ERR_RD_DIR_ERR:
	    dprintf(0, ("Read err on dir-entry \n"));
	    break;
	case PART_ERR_POISON:
	    dprintf(0, ("Access to poisoned address \n"));
	    break;
	case PART_ERR_ACCESS:
	    dprintf(0, ("Access violation \n"));
	    break;
	case PART_ERR_UNKNOWN:
	    dprintf(0, ("Error unknown to part. code \n"));
	    break;
	case PART_ERR_PARTIAL:
	    dprintf(0, ("Partial read/write error \n"));
	    break;
	case PART_ERR_DEACTIVATE:
	    dprintf(0, ("Partition de-activation error \n"));
	    break;
    }

    for(unit = 0; unit < MAX_UNITS; unit++)  {
	/* not shutdown already? */
	if(!clshm_started_up[unit])  {
	    continue;
	}  
	ctl_dev = GlobCtlDev[unit];
	
	ASSERT_ALWAYS(ctl_dev != NULL);
    
	dprintf(5, ("clshm LOCK done; can't test recovery via ioctl\n"));
	LOCK(ctl_dev->devmutex);
    	
	/* if error in an paddr allocated from local partition */
	/* normally, this case should not arise! */
	for(i = 0; i < MAX_PARTITIONS; i++)  {
	    if(!(ctl_dev->remote_parts[i].state & CLSHM_PART_S_PART_UP)) {
		continue;
	    }

	    /* go through all remote pages */
	    for(elem = ctl_dev->remote_parts[i].rem_page_info, prev = NULL;
		elem != NULL; prev = elem, elem = elem->next) {
		for(page = 0; page < elem->num_pages; page++) {
		    /* is our error on this page? */
		    start_phys_addr = elem->paddrs[page];
		    end_phys_addr = start_phys_addr + ctl_dev->page_size - 1;

		    /* found a match, so remove the page from existence on
		     * the error */
		    if(fault_addr >= start_phys_addr &&
		       fault_addr <= end_phys_addr) {
			paddr_matched = 1;
			remove_remote_page_on_error(ctl_dev, i, elem, prev);
			break;
		    }
		} /* for all pages in a single remote mapping */

		if(paddr_matched) {
		    break;
		}

	    } /* for all remote mappings */
	    if(paddr_matched) {
		break;
	    }

	} /* for all partitions */

	dprintf(5, ("clshm UNLOCK done; can't test recovery via ioctl\n"));
	UNLOCK(ctl_dev->devmutex);

    }  /* for all units */

    dprintf(5, ("clshm Returning %d from recover_from_error()\n",
	paddr_matched));

    return(paddr_matched);
}


/* remove_remote_page_on_error
 * ---------------------------
 * Parameters:
 *	ctl_dev:	control device pointer
 *	part:		index of part to remove page from
 *	rem_elem:	the element to remove
 *	rem_prev:	the element previous to removed elem
 *	
 * we have received an error on the remotely mapped page specified by
 * the partition index into the remote_parts array and the rem_elem
 * (with the rem_prev set so we can easily remove it from the rem_
 * page_info link list).  Go through all the map_tab_entries for this
 * remote mapping and kill processes before freeing up all the memory
 * associated with this page
 */
static void remove_remote_page_on_error(clshm_dev_t *ctl_dev, partid_t part, 
					remote_map_record_t *rem_elem, 
					remote_map_record_t *rem_prev)
{
    int pids[CLSHM_MAX_DELETE_PIDS];
    int	signals[CLSHM_MAX_DELETE_PIDS];
    int num_pids = 0, done = 0, i, error;
    map_tab_entry_t *map_elem;
    clshm_msg_t msg;
    shmid_in_msg shmid;

    if(part == part_id()) {
	dprintf(0, ("clshm: got hw errors on local partitions!!!!\n"));
	return;
    }
    
    
    while(!done) {
	done = 1;
	for(map_elem = ctl_dev->remote_parts[part].map_tab;
	    map_elem != NULL; map_elem = map_elem->next) {
	    if(map_elem->page_info->shmid == rem_elem->shmid) {
		
		/* if we are overflowing our array, we'll go through 
		 * again after we delete these ones */
		if(num_pids == CLSHM_MAX_DELETE_PIDS) {
		    done = 0;
		    break;
		}

		/* make sure this isn't a duplicate pid */
		for(i = 0; i < num_pids; i++) {
		    if(map_elem->gid == pids[i]) {
			break;
		    }
		}
		if(i == num_pids) {
		    signals[num_pids] = map_elem->signal_num;
		    /* use gid since we want children to die as well */
		    pids[num_pids++] = map_elem->gid;
		}
	    } 
	}

	/* remove all the pids we have found so far */
	for(i = 0; i < num_pids; i++) {
	    error = unregister_from_maptable(ctl_dev, NULL, pids[i]);
	    if(error) {
		dprintf(0, ("clshm: ERROR: we can't unregister pid from "
			    "map table %d\n", map_elem->pid));
	    }
	    signal(pids[i], signals[i]);
	}
    }

    /* tell the daemon that we have destoyed this shmid */
    shmid = rem_elem->shmid;
    msg.type = DRV_TO_DAEM_RMID;
    msg.src_part = part_id();	
    msg.dst_part = SHMID_TO_PARTITION(shmid);
    msg.len = sizeof(shmid_in_msg);
    bcopy(&shmid, msg.body, sizeof(shmid_in_msg));
    error = send_to_clshmd(ctl_dev, &msg);
    if(error) {
	dprintf(0, ("clshm: trying to send RMID to daem failed shmid 0x%x\n",
		    shmid));
    }

    /* find all map table entries and kill these processes */
    if(rem_prev) {
	rem_prev->next = rem_elem->next;
    } else {
	ctl_dev->remote_parts[part].rem_page_info = rem_elem->next;
    }

    if(rem_elem->count) {
	dprintf(0, ("clshm: ERROR: couldn't get down to just no references "
		    "to a remote map record\n"));
    }
    
    /* free everything */
    if(rem_elem->ready) {
	for(i = 0; i < rem_elem->num_pages; i++) {
	    part_unregister_page(rem_elem->paddrs[i]);	
	}
    }
    kmem_free(rem_elem->paddrs, sizeof(paddr_t) * rem_elem->num_pages);
    kmem_free(rem_elem->pfns, sizeof(pfn_t) * rem_elem->num_pages);
    kmem_free(rem_elem->msg_recv, sizeof(uint) * rem_elem->num_msgs);
    kmem_free(rem_elem, sizeof(remote_map_record_t));
}



/* clshmunmap
 * ----------
 * Parameters:
 *	dev:	device info
 *	vt:	vhandle of users map
 *	return:	errno values
 *
 * Send to shm_unmap or if it is a control device, then mark it as
 * unmapped and free up resources, if the SHUTDOWN has been called.
 */
/* ARGSUSED2 */
int clshmunmap(dev_t dev, vhandl_t *vt)
{
    clshm_dev_info_t    *dev_info;
    clshm_dev_t	*ctl_dev;

    dev_info = (clshm_dev_info_t *) device_info_get(dev);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in clshmunmap \n");
    }
    if(IS_SHM_DEV(dev_info)) {
	return(shm_unmap(dev, vt));
    } else {
	ctl_dev = (clshm_dev_t *) dev_info->dev_parms;
	dprintf(1, ("clshmunmap called for ctl dev\n"));
	/* DAEM died!!!! -- only reason for unmap */
	ctl_dev->c_state &= (~CL_SHM_C_ST_MMAPPED);
	if(ctl_dev->c_state & CL_SHM_C_ST_SHUT) {
	    release_ctl_resources(ctl_dev);
	}
        return(0);
    }
    /* NOTREACHED */
}


/* shm_unmap
 * ---------
 * Parameters:
 *	dev:	device info
 *	vt:	vhandle of users map
 *	return:	errno values
 *
 * Unregister this process with this vt from the map tables.
 */
/* ARGSUSED */
static int shm_unmap(dev_t dev, vhandl_t *vt)
{
    int         error = 0;
    clshm_dev_info_t    *dev_info;
    clshm_dev_t  *ctl_dev;

    dev_info = (clshm_dev_info_t *) device_info_get(dev);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in Nctl unmap of clshm\n");
    }
    ASSERT(IS_SHM_DEV(dev_info));
    ctl_dev = (clshm_dev_t *) dev_info->dev_parms;

    CHECK_CL_SHM_C(ctl_dev);
    LOCK(ctl_dev->devmutex);

    dprintf(5, ("clshm: unregister_from_maptable called from shm_unmap "
		"for ctl_dev 0x%x\n", ctl_dev));
    error = unregister_from_maptable(ctl_dev, vt, THIS_PROCESS);

    /* resources will be released when device is closed */

    UNLOCK(ctl_dev->devmutex);
    return(error);
}



/* clshmclose
 * ----------
 * Parameters:
 *	dev:	dev info to close
 *	return:	errno values
 *
 * A ctl close does nothing and if flags or correct, then free up
 * everyone that still thinks they have access to our memory from
 * shm device
 */
int clshmclose(dev_t dev)
{
    ulong_t	closing_pid;
    clshm_dev_t  *ctl_dev;
    int     error = 0;

    clshm_dev_info_t    *dev_info;

    dprintf(5, ("clshmclose: device 0x%x\n", dev));
    dev_info = (clshm_dev_info_t *) device_info_get(dev);
    if(!dev_info)   {
	cmn_err(CE_PANIC, "NULL dev_info in clshmclose \n");
    }
    ctl_dev = (clshm_dev_t *)  dev_info->dev_parms;

    if((drv_getparm(PPID, &closing_pid)) == -1)  {
        cmn_err(CE_WARN, "clshm: problem getting pid in close()\n");
    }

    CHECK_CL_SHM_C(ctl_dev);
    LOCK(ctl_dev->devmutex);

    if(IS_SHM_DEV(dev_info))   {
	dprintf(10, ("clshm unregister_from_maptable called from close "
		     "for ctl_dev 0x%x\n", ctl_dev));
	/* we know that all processes have closed this file */
	unregister_from_maptable(ctl_dev, NULL, ALL_PROCESSES);
	ctl_dev->c_state &= ~(CL_SHM_C_ST_OPENNED | CL_SHM_C_ST_EXOPEN);
    } else {
	/* ctl device: the daemon unmap cleans everything up, 
	 * so a close does nothing */
    }


    UNLOCK(ctl_dev->devmutex);
    dprintf(10, ("clshm Returning %d from clshmclose\n", error));
    return(error);
}


/* remove_part_from_maptab
 * -----------------------
 * Parameters:
 *	ctl_dev:	general dev info
 *	part:		index of partition in remote_parts to remove
 *
 * Go through and remove the remote partition from the ctl_dev.  This
 * includes all the remote_parts info.  Assume that we are just freeing
 * memory at this point.  Someone else should be sending signals and
 * dealing with the users before calling this function.
 */
static void remove_part_from_maptab(clshm_dev_t *ctl_dev, partid_t part)
{
    map_tab_entry_t	*map_elem, *map_prev;

    dprintf(5, ("clshm entered remove_part_from_maptab, partid %d\n",
		part));

    /* free all the mapped pids to this driver */
    for(map_elem = ctl_dev->remote_parts[part].map_tab, map_prev = NULL;
	map_elem != NULL; 
	map_prev = map_elem, map_elem = map_elem->next) {
	(map_elem->page_info->count)--;
	if(map_prev) {
	    kmem_free(map_prev, sizeof(map_tab_entry_t));
	}
	dprintf(5, ("clshm remove_part_from_maptab:")); 
	dprintf(5, ("clshm remove pid %d (gid %d) from part %d\n", 
		    map_elem->pid, map_elem->gid, part));
    }   	
    if(map_prev) {
	kmem_free(map_prev, sizeof(map_tab_entry_t));
    }
    ctl_dev->remote_parts[part].map_tab = NULL;

    /* free the remote map records */
    free_remote_map_records(ctl_dev, part);

    dprintf(10, ("clshm Done with remove_part_from_maptab\n"));
}


/* unregister_from_maptable
 * ------------------------
 * Parameters:
 *	ctl_dev:	general device info
 *	vt:		user vhandle that we are unregistering
 *	process:	either THIS_PROCESS or ALL_PROCESSES to kill off
 *	return:		errno values
 * 
 * Go through the mapped entries and kill off either just this process or
 * all processes for a particular vhandle (or all vhandles for this process
 * if it is dying).
 */
static int unregister_from_maptable(clshm_dev_t *ctl_dev, vhandl_t *vt, 
				    int process)
{
    int			i;
    map_tab_entry_t	*map_elem, *prev;
    ulong_t		pid;

    dprintf(5, ("clshm entered unregister_from_maptable\n"));

    /* only this process to kill off */
    if(process == THIS_PROCESS) {
	if(drv_getparm(PPID, &pid))  {
	    cmn_err(CE_WARN, "clshm: internal error in "
		    "drv_getparm in unregister\n");
	    return(EFAULT);
	}
    }	    

    /* go through all partitions to kill off vhandle and process */
    for(i = 0; i < MAX_PARTITIONS; i++) {
	if(!(ctl_dev->remote_parts[i].state & CLSHM_PART_S_PART_UP)) {
	    continue;
	}
	for(prev = NULL, map_elem = ctl_dev->remote_parts[i].map_tab; 
	    map_elem != NULL; ) {
	    
	    /* if we match the process number and the vhandle destroy it */
	    if((process == ALL_PROCESSES || map_elem->pid == pid) &&
	       (!vt || map_elem->vt_handle == v_gethandle(vt)))  {

		pid = map_elem->pid;
		dprintf(5, ("clshm: unregister_from_maptable:")); 
		dprintf(5, ("clshm: remove pid %d (gid %d) from part %d "
			    "shmid = %x\n", map_elem->pid, map_elem->gid,
			    i, map_elem->page_info->shmid));

		if(map_elem->page_info->count == 0) {
		    dprintf(0, ("clshm: negative count for page\n"));
		    return(EFAULT);
		}
		(map_elem->page_info->count)--;

		/* if the count just became 1 we need to tell the
		 * driver in case it needs to autocleanup anything */
		if(map_elem->page_info->count == 0) {
		    clshm_msg_t msg;
		    shmid_in_msg shmid;
		    int error;

		    shmid = map_elem->page_info->shmid;
		    msg.type = DRV_TO_DAEM_DETACH;
		    msg.src_part = part_id();
		    msg.dst_part = SHMID_TO_PARTITION(shmid);
		    msg.len = sizeof(shmid_in_msg);
		    bcopy(&shmid, msg.body, sizeof(shmid_in_msg));

		    if(error = send_to_clshmd(ctl_dev, &msg)) {
			dprintf(0, ("clshm: can't sent to daemon\n"));
			return(error);
		    }
		}

		if(prev) {
		    prev->next = map_elem->next;
		    kmem_free(map_elem, sizeof(map_tab_entry_t));
		    map_elem = prev->next;
		} else {
		    ctl_dev->remote_parts[i].map_tab = map_elem->next;
		    kmem_free(map_elem, sizeof(map_tab_entry_t));
		    map_elem = ctl_dev->remote_parts[i].map_tab;
		}
	    } else {
		prev = map_elem;
		map_elem = map_elem->next;
	    }
	}
    }	/* for all processes in map_tab */

    dprintf(10, ("clshm Done with unregister_from_maptable\n"));
    return(0);
}


/* signal_processes
 * ----------------
 * Parameters:
 *	ctl_dev:	general device info
 *	part:		index in remote_parts to send signal to
 *
 * Send the signal specified in the mapped entries.  All pids that have 
 * segments mapped from this partition need to be signaled.
 */
static void signal_processes(clshm_dev_t  *ctl_dev, partid_t part)
{
    remote_partition_t *remote_part = NULL;
    map_tab_entry_t *map_elem;

    remote_part = &(ctl_dev->remote_parts[part]);
    
    for(map_elem = remote_part->map_tab; map_elem != NULL; 
	map_elem = map_elem->next) {
	/* send the registered signal to the process group */
	signal(map_elem->gid, map_elem->signal_num);
	cmn_err(CE_WARN|CE_CPUID, "clshm: Process %d (group %d) was sent "
		"signal %d because backing partition in clshm "
		"has failed\n",
		map_elem->pid, map_elem->gid, 
		map_elem->signal_num);
    }  /* for all maptab entries */
}


/* free_remote_map_records
 * -----------------------
 * Parameters:
 *	ctl_dev:	general device info
 *	part:		index of partition to remove
 *
 * unregister all pages assocated with remote map records and then
 * free the memory as well for this partition.
 */
static void free_remote_map_records(clshm_dev_t *ctl_dev, partid_t part)
{
    remote_map_record_t *prev, *elem;
    int i;
    

    for(prev = NULL, elem = ctl_dev->remote_parts[part].rem_page_info; 
	elem != NULL; prev = elem, elem = elem->next) {

	/* only unregister if element was ready (was registered).  We don't
	 * register our own pages because partition.c uses same avl tree
	 * for local and remote pages */

	if(elem->ready && SHMID_TO_PARTITION(elem->shmid) != part_id()) {
	    for(i = 0; i < elem->num_pages; i++) {
		part_unregister_page(elem->paddrs[i]);
		dprintf(7, ("clshm: free_remote_map_records: "
			    "unregistering page address %x\n", 
			    elem->paddrs[i]));
	    }
	}

	if(prev) {
	    kmem_free(prev->paddrs, sizeof(paddr_t) * prev->num_pages);
	    kmem_free(prev->pfns, sizeof(pfn_t) * prev->num_pages);
	    kmem_free(prev->msg_recv, sizeof(uint) * prev->num_msgs);
	    kmem_free(prev, sizeof(remote_map_record_t));
	}		
    }
    if(prev) {
	kmem_free(prev->paddrs, sizeof(paddr_t) * prev->num_pages);
	kmem_free(prev->pfns, sizeof(pfn_t) * prev->num_pages);
	kmem_free(prev->msg_recv, sizeof(uint) * prev->num_msgs);
	kmem_free(prev, sizeof(remote_map_record_t));
    }
    ctl_dev->remote_parts[part].rem_page_info = NULL;
}


/* free_local_map_records
 * ----------------------
 * Parameters:
 *	ctl_dev:	general device info
 *
 * Go through and release all the memory associated with local map
 * records.  This is only called when everything is getting freed up
 * so we don't have to worry about all the references into this link
 * list.
 */
static void free_local_map_records(clshm_dev_t *ctl_dev)
{
    local_map_record_t *prev, *elem;
    int i;
    
    for(prev = NULL, elem = ctl_dev->loc_page_info; elem != NULL; prev = elem, 
	elem = elem->next) {
	for(i = 0; i < elem->num_pages; i++) {
	    dprintf(5, ("clshm: page_part_free: pfdat 0x%x\n", 
			elem->pages[i].pfdat));
	    part_page_free(elem->pages[i].pfdat, ctl_dev->page_size);
	}
	if(prev) {
	    kmem_free(prev->pages, sizeof(kernel_page_t) * prev->num_pages);
	    kmem_free(prev, sizeof(local_map_record_t));
	}	
    }
    if(prev) {
	kmem_free(prev->pages, sizeof(kernel_page_t) * prev->num_pages);
	kmem_free(prev, sizeof(local_map_record_t));
    }
    ctl_dev->loc_page_info = NULL;
}


/* free_shm_state
 * --------------
 * Parameters:
 *	ctl_dev:	general device info
 *
 * Go though all info about each remote partition and free it up as well
 * as telling everyone that things are going away.  If we have any locally
 * allocated pages, also free up those.
 */
static void free_shm_state(clshm_dev_t  *ctl_dev)
{
    int i;

    dprintf(7, ("clshm: in free_shm_state\n"));
    free_local_map_records(ctl_dev);
    for(i = 0; i < MAX_PARTITIONS; i++) {
	if(ctl_dev->remote_parts[i].state & CLSHM_PART_S_PART_UP) {
	    signal_processes(ctl_dev, i);
	    remove_part_from_maptab(ctl_dev, i);
	    ctl_dev->remote_parts[i].rem_page_info = NULL;
	    ctl_dev->remote_parts[i].map_tab = NULL;
	    ctl_dev->remote_parts[i].state &= ~(CLSHM_PART_S_DAEM_CONTACT);
	}
    }
}


/* release_ctl_resource
 * --------------------
 * Parameters:
 *	ctl_dev:	general device info
 *
 * Clean up all the info about this driver really.  Make sure that
 * we clear up all the info about sharing with the daemon ad all the state
 * we have about shm structures as well.
 */
static void release_ctl_resources(clshm_dev_t  *ctl_dev)
{
    dprintf(5, ("clshmclose: releasing shm structures\n"));

    free_shm_state(ctl_dev);
    mutex_destroy(&ctl_dev->devmutex);
    dprintf(5, ("clshmclose: released kernel memory for ctl structures\n"));

    if(ctl_dev->clshmd_buff)  {
        kvpfree((void *) ctl_dev->clshmd_buff, ctl_dev->clshmd_pages);
        ctl_dev->clshmd_buff = NULL;
        dprintf(5, ("clshmclose: released kernel memory for clshmd \n"));
    }
    else  {
        dprintf(0, ("clshm WARNING: close of clshm couldn't find clshmd "
		    "space\n"));
    }

    if(ctl_dev->clshmd_ref)  {
        if(proc_signal(ctl_dev->clshmd_ref, SIGKILL))  {
            dprintf(1, ("clshm WARNING: Couldn't find clshmd at release ctl; "
			"daemon exited already? \n"));
	}

        proc_unref(ctl_dev->clshmd_ref);
        ctl_dev->clshmd_ref = NULL;
    }
    ctl_dev->c_state = CL_SHM_C_ST_INITTED;
}


