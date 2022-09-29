#ident "io/scsi.c: $Revision: 1.127 $"


#include <sys/types.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/driver.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/scsi.h>
#include <sys/invent.h>
#include <sys/sema.h>
#include <sys/buf.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <sys/smfd.h>
#include <sys/cmn_err.h>
#include <ksys/vfile.h>
#include <sys/atomic_ops.h>
#include <sys/cdl.h>
#include <sys/failover.h>
#include <ksys/hwg.h>

#ifdef DEBUG
#define NOTIMP(what) cmn_err(CE_WARN, "%s function not registered/implented for this SCSI controller\n", what)
#else
#define NOTIMP(what)
#endif

char *scsi_adaperrs_tab[NUM_ADAP_ERRS] = {
	"",
	"device does not respond to selection",
	"Controller protocol error or SCSI bus reset",
	"SCSI bus parity error",
	"Parity/ECC error in system memory during DMA",
	"Command timed out",
	"Buffer not correctly aligned in memory",
	"Unit attention received on another command causes retry",
	"Driver protocol error"
};

/*
 * NOTE: these tables are used by the smfd, tpsc, and dksc drivers,
 * and will probably be used by other SCSI drivers in the future.
 * That is why they are now in scsi.c, even though not referenced here.
 */
char *scsi_key_msgtab[SC_NUMSENSE] = {
	"No sense",				/* 0x0 */
	"Recovered Error",			/* 0x1 */
	"Device not ready",			/* 0x2 */
	"Media error",				/* 0x3 */
	"Device hardware error",		/* 0x4 */
	"Illegal request",			/* 0x5 */
	"Unit Attention",			/* 0x6 */
	"Data protect error",			/* 0x7 */
	"Unexpected blank media",		/* 0x8 */
	"Vendor Unique error",			/* 0x9 */
	"Copy Aborted",				/* 0xa */
	"Aborted command",			/* 0xb */
	"Search data successful",		/* 0xc */
	"Volume overflow",			/* 0xd */
	"Reserved (0xE)",			/* 0xe */
	"Reserved (0xF)",			/* 0xf */
};

/*
 * Additional Sense Qualifier message table.
 * There is no table in the PROM.
 */
char *scsi_addit_msgtab[SC_NUMADDSENSE]
#ifndef PROM
	= {
	NULL,	/* 0x00 */
	"No index/sector signal",	/* 0x01 */
	"No seek complete",	/* 0x02 */
	"Write fault",	/* 0x03 */
	"Not ready to perform command",		/* 0x04 */
	"Unit does not respond to selection",	/* 0x05 */
	"No reference position",	/* 0x06 */
	"Multiple drives selected",	/* 0x07 */
	"LUN communication error",	/* 0x08 */
	"Track error",	/* 0x09 */
	"Error log overflow",	/* 0x0a */
	NULL,	/* 0x0b */
	"Write error",	/* 0x0c */
	NULL,	/* 0x0d */
	NULL,	/* 0x0e */
	NULL,	/* 0x0f */
	"ID CRC or ECC error",	/* 0x10  */
	"Unrecovered data block read error",    /* 0x11 */
	"No addr mark found in ID field",       /* 0x12 */
	"No addr mark found in Data field",	/* 0x13 */
	"No record found",	/* 0x14 */
	"Seek position error",	/* 0x15 */
	"Data sync mark error",	/* 0x16 */
	"Read data recovered with retries",	/* 0x17 */
	"Read data recovered with ECC",	/* 0x18 */
	"Defect list error",	/* 0x19 */
	"Parameter overrun",	/* 0x1a */
	"Synchronous transfer error",	/* 0x1b */
	"Defect list not found",	/* 0x1c */
	"Compare error",	/* 0x1d */
	"Recovered ID with ECC",	/* 0x1e */
	NULL,	/* 0x1f */
	"Invalid command code",	/* 0x20 */
	"Illegal logical block address",	/* 0x21 */
	"Illegal function",	/* 0x22 */
	NULL,	/* 0x23 */
	"Illegal field in CDB",	/* 0x24 */
	"Invalid LUN",	/* 0x25 */
	"Invalid field in parameter list",	/* 0x26 */
	"Media write protected",	/* 0x27 */
	"Media change",	/* 0x28 */
	"Device reset",	/* 0x29  */
	"Log parameters changed",	/* 0x2a */
	"Copy requires disconnect",	/* 0x2b */
	"Command sequence error",	/* 0x2c  */
	"Update in place error",	/* 0x2d */
	NULL,	/* 0x2e */
	"Tagged commands cleared",	/* 0x2f */
	"Incompatible media",	/* 0x30 */
	"Media format corrupted",	/* 0x31 */
	"No defect spare location available",	/* 0x32 */
	"Media length error",	/* 0x33 (spec'ed as tape only) */
	NULL,	/* 0x34 */
	NULL,	/* 0x35 */
	"Toner/ink error",	/* 0x36 */
	"Parameter rounded",	/* 0x37 */
	NULL,	/* 0x38 */
	"Saved parameters not supported",	/* 0x39 */
	"Medium not present",	/* 0x3a */
	"Forms error",	/* 0x3b */
	NULL,	/* 0x3c */
	"Invalid ID msg",	/* 0x3d */
	"Self config in progress",	/* 0x3e */
	"Device config has changed",	/* 0x3f */
	"RAM failure",	/* 0x40 */
	"Data path diagnostic failure",	/* 0x41 */
	"Power on diagnostic failure",	/* 0x42 */
	"Message reject error",	/* 0x43 */
	"Internal controller error",	/* 0x44 */
	"Select/Reselect failed",	/* 0x45 */
	"Soft reset failure",	/* 0x46 */
	"SCSI interface parity error",	/* 0x47 */
	"Initiator detected error",	/* 0x48 */
	"Inappropriate/Illegal message",	/* 0x49 */
	"Command phase error", /* 0x4a */
	"Data phase error", /* 0x4b */
	"Failed self configuration", /* 0x4c */
	NULL, /* 0x4d */
	"Overlapped cmds attempted", /* 0x4e */
	NULL, /* 0x4f */
	NULL, /* (tape only) 0x50 */
	NULL, /* (tape only) 0x51 */
	NULL, /* (tape only) 0x52 */
	"Media load/unload failure", /* 0x53 */
	NULL, /* 0x54 */
	NULL, /* 0x55 */
	NULL, /* 0x56 */
	"Unable to read table of contents", /* 0x57 */
	"Generation (optical device) bad", /* 0x58 */
	"Updated block read (optical device)", /* 0x59 */
	"Operator request or state change", /* 0x5a */
	"Logging exception", /* 0x5a */
	"RPL status change", /* 0x5c */
	"Self Diagnostics predict unit will fail soon", /* 0x5d */
	NULL, /* 0x5e */
	NULL, /* 0x5f */
	"Lamp failure", /* 0x60 */
	"Video acquisition error/focus problem", /* 0x61 */
	"Scan head positioning error", /* 0x62 */
	"End of user area on track", /* 0x63 */
	"Illegal mode for this track", /* 0x64 */
	NULL, /* 0x65 */
	NULL, /* 0x66 */
	NULL, /* 0x67 */
	NULL, /* 0x68 */
	NULL, /* 0x69 */
	NULL, /* 0x6a */
	NULL, /* 0x6b */
	NULL, /* 0x6c */
	NULL, /* 0x6d */
	NULL, /* 0x6e */
	NULL, /* 0x6f */
	"Decompression error", /* 0x70 (DAT only; may be in scsi3) */
}
#endif /* PROM */
;

/* =======================================================================
 * scsi generic layer's new infractructure changes are implemented
 * in using connection-driver list
 * =======================================================================
 */

#define	NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

cdl_p                   scsi_registry = NULL;

void 			scsi_init(void);
int			scsi_driver_register(int,char *);
void			scsi_driver_unregister(char *);
int			scsi_device_is_attached(vertex_hdl_t);

/* This is needed to provide mutual exclusion in the case of
 * mutliple processe trying to add/delete aliases due
 * to reprobing the bus , fx'ing the disk , adding a new tape drive etc.
 */
mutex_t			scsi_alias_mutex;


/*
 * called once during device driver initializtion 
 */
void
scsi_init(void)
{
    cdl_p                   cp;

#if DEBUG && ATTACH_DEBUG
    printf("scsi_init\n");
#endif
    /* Allocate the registry.
     * We might already have one.
     * If we don't, go get one.
     * MPness: someone might have
     * set one up for us while we
     * were not looking; use an atomic
     * compare-and-swap to commit to
     * using the new registry if and
     * only if nobody else did first.
     * If someone did get there first,
     * toss the one we allocated back
     * into the pool.
     */
    if (scsi_registry == NULL) {
	cp = cdl_new(EDGE_LBL_SCSI, "vendor", "device");
	if (!compare_and_swap_ptr((void **) &scsi_registry, NULL, (void *) cp)) {
	    cdl_del(cp);
	}
    }
    ASSERT(scsi_registry != NULL);
    /* Initialize the mutex to provide mutual exclusion into modifying
     * the scsi alias directories "/hw/disk" , "/hw/rdisk" , "/hw/tape" etc.
     */
    init_mutex(&scsi_alias_mutex, MUTEX_DEFAULT,"scsi_alias_lock", 0);
}


/*ARGSUSED2 */
int
scsi_driver_register(int 	unit_type,
		     char 	*driver_prefix)
{
    /* a driver's init routine might call
     * scsi_driver_register before the
     * system calls scsi_init; so we
     * make the init call ourselves here.
     */
    if (scsi_registry == NULL)
	scsi_init();

    return cdl_add_driver(scsi_registry,
			  unit_type, PREFIX_TO_KEY(driver_prefix), /* key1, key2 */
			  driver_prefix, 0);
}

/*
 * Remove an initialization function.
 */
void
scsi_driver_unregister(char *driver_prefix)
{
    /* before a driver calls unregister,
     * it must have called register; so
     * we can assume we have a registry here.
     */
    ASSERT(scsi_registry != NULL);

    cdl_del_driver(scsi_registry, driver_prefix);
}

/* create a vertex that corresponds to this scsi device
 * and add the info associated with that vertex 
 */
vertex_hdl_t
scsi_device_register(vertex_hdl_t	lun_vhdl,
		     u_char		*inv,
		     char		*edge_name)
{
	/* REFERENCED */
	graph_error_t		rv;
	vertex_hdl_t		known_unit_vhdl;
	scsi_unit_info_t	*unit_info;
	scsi_lun_info_t		*lun_info;
	int			type = inv[0] & 0x1f;

	rv = hwgraph_path_add(lun_vhdl,
			      edge_name,
			      &known_unit_vhdl);

	ASSERT(rv == GRAPH_SUCCESS);

	/* just make sure that this vertex doesn't exist already */

	if (!hwgraph_fastinfo_get(known_unit_vhdl)) {
		/*
		 * FIGURE OUT THE INFO COMMON TO ALL SCSI UNITS
		 */
		lun_info = scsi_lun_info_get(lun_vhdl);
		NEW(unit_info);
		SUI_UNIT_VHDL(unit_info)	= known_unit_vhdl;	
		SUI_SM_CREATED(unit_info) 	= 0;
		SUI_INV(unit_info) 		= inv;		/* inventory info */
		SUI_LUN_INFO(unit_info) 	= lun_info;	/* pointer to lun info */
		SUI_CTINFO(unit_info)		= 0;		/* type specific info set & used by 
								 * the relevant driver 
								 */
		mutex_init(&SUI_OPENSEMA(unit_info),
			   MUTEX_DEFAULT,
			   (type == 0) ? "smo" : "ctops"); /* init the open semaphore */
	       
		scsi_unit_info_put(known_unit_vhdl,unit_info);
	}
	hwgraph_vertex_unref(known_unit_vhdl);
	return known_unit_vhdl;
}

/* check if there is a driver registered to handle this kind
 * of device. if so call that driver's attach routine to build
 * it's graph namespace. if no driver is registered yet create
 * well-known path where the driver can look for all the devices
 * it is supposed to handle when it is registered.
 */

int
scsi_device_attach(vertex_hdl_t known_unit_vhdl, 
		   char		*driver_prefix)
{
	int			unit_type;
	scsi_unit_info_t	*unit_info;
	
	unit_info = scsi_unit_info_get(known_unit_vhdl);
	unit_type = SUI_TYPE(unit_info);	
	/* we don't start attaching things until
	 * all the driver init routines (including
	 * scsi_init) have been called; so we
	 * can assume here that we have a registry.
	 */
	ASSERT(scsi_registry != NULL);

	return cdl_add_connpt(scsi_registry, 
			      unit_type, PREFIX_TO_KEY(driver_prefix), /* key1, key2 */
			      known_unit_vhdl);
}

/* check if there is a driver registered to handle this kind
 * of device. if so call that driver's detach routine to remove
 * it's graph namespace. 
 */

void
scsi_device_detach(vertex_hdl_t known_unit_vhdl)
{
	int			unit_type;
	scsi_unit_info_t	*unit_info;
	
	unit_info = scsi_unit_info_get(known_unit_vhdl);
	unit_type = SUI_TYPE(unit_info);	
	/* we don't start attaching things until
	 * all the driver init routines (including
	 * scsi_init) have been called; so we
	 * can assume here that we have a registry.
	 */
	ASSERT(scsi_registry != NULL);

	cdl_del_connpt(scsi_registry, 
		       unit_type, -1, /* key1, wildcard */
		       known_unit_vhdl);
}

/* ======================================================================= */
	

/*
 * interface functions to scsi controller info 
 */

/* 
 * allocate memory for and initialize the scsi controller info
 */
scsi_ctlr_info_t *
scsi_ctlr_info_init(void) 
{
	scsi_ctlr_info_t	*info;

	info = (scsi_ctlr_info_t *)kern_calloc(1,sizeof(*info));
	ASSERT(info);
	
	return info;
}

void
scsi_ctlr_info_free(vertex_hdl_t 	vhdl)
{
	scsi_ctlr_info_t	*info;

	info = (scsi_ctlr_info_t *)hwgraph_fastinfo_get(vhdl);
	if (info)
		kern_free(info);
}
/*
 * associate the controller info  with its vertex
 */
void
scsi_ctlr_info_put(vertex_hdl_t 	vhdl,
		   scsi_ctlr_info_t 	*info)
{
	(void)hwgraph_fastinfo_set(vhdl,(arbitrary_info_t)info);
}

/*
 * get the controller info associated with this vertex
 */
scsi_ctlr_info_t *
scsi_ctlr_info_get(vertex_hdl_t		vhdl)
{
	scsi_ctlr_info_t	*info;

	info = (scsi_ctlr_info_t *)hwgraph_fastinfo_get(vhdl);
	ASSERT(info);

	return	info;
}



/*
 * interface functions to scsi target info 
 */

/* 
 * allocate memory for and initialize the scsi target info
 */
scsi_targ_info_t *
scsi_targ_info_init(void) 
{
	scsi_targ_info_t	*info;

	info = (scsi_targ_info_t *)kern_calloc(1,sizeof(*info));
	ASSERT(info);
	
	return info;
}


void
scsi_targ_info_free(vertex_hdl_t 	vhdl)
{
	scsi_targ_info_t	*info;

	info = (scsi_targ_info_t *)hwgraph_fastinfo_get(vhdl);
	hwgraph_fastinfo_set(vhdl,NULL);
	if (info) 
		kern_free(info);
	
}

/*
 * associate the target info  with its vertex
 */
void
scsi_targ_info_put(vertex_hdl_t 	vhdl,
		   scsi_targ_info_t 	*info)
{
	(void)hwgraph_fastinfo_set(vhdl,(arbitrary_info_t)info);
}

/*
 * get the target info associated with this vertex
 */
scsi_targ_info_t *
scsi_targ_info_get(vertex_hdl_t		vhdl)
{
	scsi_targ_info_t	*info;

	info = (scsi_targ_info_t *)hwgraph_fastinfo_get(vhdl);
	ASSERT(info);

	return	info;
}

/*
 * check if target vertex already exists 
 */
int
scsi_targ_vertex_exist(vertex_hdl_t	ctlr_vhdl,
		       targ_t		targ,
		       vertex_hdl_t	*to)
{
	graph_error_t		rv;
	char			loc_str[LOC_STR_MAX_LEN];

	sprintf(loc_str,TARG_NUM_FORMAT,targ);
	if ((rv = hwgraph_traverse(ctlr_vhdl,
				   loc_str,
				   to)) == GRAPH_SUCCESS)
		hwgraph_vertex_unref(*to);
	return ((rv == GRAPH_NOT_FOUND) ? 
		VERTEX_NOT_FOUND 	: 
		VERTEX_FOUND);
}

int
scsi_node_port_vertex_exist(vertex_hdl_t ctlr_vhdl,
			    uint64_t	 node, uint64_t	 port,
			    vertex_hdl_t *to)
{
	graph_error_t	rv;
	char		loc_str[LOC_STR_MAX_LEN];

	sprintf(loc_str, NODE_PORT_NUM_FORMAT, node, port);
	if ((rv = hwgraph_traverse(ctlr_vhdl,
				   loc_str,
				   to)) == GRAPH_SUCCESS)
		hwgraph_vertex_unref(*to);
	return ((rv == GRAPH_NOT_FOUND) ? 
		VERTEX_NOT_FOUND 	: 
		VERTEX_FOUND);
}

/*
 * remove the target vertex corr. to the given
 * unit number
 */
vertex_hdl_t
scsi_targ_vertex_remove(vertex_hdl_t 	ctlr_vhdl,
			targ_t		targ)

{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	vertex_hdl_t		vhdl_1,vhdl_2;
	char			loc_str[LOC_STR_MAX_LEN];

	rv = hwgraph_traverse(ctlr_vhdl,
			      TARG_FORMAT,
			      &vhdl_1);
	ASSERT((rv == GRAPH_SUCCESS) &&
	       (vhdl_1 != GRAPH_VERTEX_NONE));
	/*
	 * we can do this because this function gets called
	 * only if scsi_lun_vertex_remove decided to remove the
	 * "lun" vertex in the target/# directory because there are no
	 * luns  left.
	 * eg : target/#/lun had 0 1 as subdirectories.
	 * scsi_lun_vertex_remove(...,1) will not remove the "lun"
	 * vertex because there is still target/#/lun/0.in this case
	 * scsi_targ_vertex_remove won't get called.
	 *
	 * eg: target/#/lun had 2 as the only subdir
	 * scsi_lun_vertex_remove(..., 2) will remove the "lun"
	 * vertex also and will return GRAPH_VERTEX_NONE.in this case
	 * scsi_targ_vertex_remove will get called and clearly we
	 * can remove # vertex of the "target/#" path
	 *
	 * remove the edge corr. to specific target
	 *
	 * XXX- unfortunately, the reference counts on
	 * the verticies are not right, so just removing
	 * the edges does not cause the verticies to go
	 * away, and trying to destroy the verticies
	 * themselves will fail (since io/graph still
	 * thinks there are outstanding verticies). So,
	 * no matter what you *think* is happening, you
	 * are still ending up with dangling verticies.
	 */
	sprintf(loc_str,"%d",targ);
	rv = hwgraph_edge_remove(vhdl_1,
				 loc_str,
				 &vhdl_2);
	ASSERT((rv == GRAPH_SUCCESS) &&
	       (vhdl_2 != GRAPH_VERTEX_NONE));
	scsi_targ_info_free(vhdl_2);
	hwgraph_vertex_unref(vhdl_2);
	hwgraph_vertex_destroy(vhdl_2);
	/* check if there is a vertex for any specific target */
	/* 127 WAS SCSI_MAXTARG until qlfc broke value for O2.  This needs to
	** be replaced with code using "edge_get_next" */
	for (targ = 0 ; targ < 127 ; targ++) {
		char		str[3];
		vertex_hdl_t	vhdl_3;

		sprintf(str,"%d",targ);
		if (hwgraph_traverse(vhdl_1,str,&vhdl_3) == GRAPH_SUCCESS) {
			hwgraph_vertex_unref(vhdl_3);
			hwgraph_vertex_unref(vhdl_1);
			return vhdl_3;
		}
	}

	/* no specific target vertex exists. remove the "target"
	 * edge under <ctlr>/# 
	 */
	rv = hwgraph_edge_remove(ctlr_vhdl,TARG_FORMAT,NULL);
	ASSERT(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(vhdl_1);
	hwgraph_vertex_destroy(vhdl_1);
	return GRAPH_VERTEX_NONE;
}


/*
 * Remove the port vertex corresponding to the given port number
 * Also remove node vertex if warranted.
 */
vertex_hdl_t
scsi_node_port_vertex_remove(vertex_hdl_t ctlr_vhdl, uint64_t node, uint64_t port)
{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	vertex_hdl_t		node_vhdl, nodenum_vhdl, port_vhdl, tmpvhdl;
	char			loc_str[LOC_STR_MAX_LEN];
	graph_edge_place_t	edge;

	rv = hwgraph_traverse(ctlr_vhdl, "node", &node_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (node_vhdl != GRAPH_VERTEX_NONE));
	sprintf(loc_str, "%llx", node);
	rv = hwgraph_traverse(node_vhdl, loc_str, &nodenum_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (nodenum_vhdl != GRAPH_VERTEX_NONE));
	rv = hwgraph_traverse(nodenum_vhdl, "port", &port_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (port_vhdl != GRAPH_VERTEX_NONE));

	/*
	 * We can do this because this function gets called
	 * only if scsi_lun_vertex_remove decided to remove the
	 * "lun" vertex in the port/# directory because there are no
	 * luns left.
	 *
	 * Example: node/#/port/#/lun had 0 1 as subdirectories.
	 *   scsi_lun_vertex_remove(...,1) will not remove the "lun"
	 *   vertex because there is still target/#/lun/0.  In this case
	 *   scsi_targ_vertex_remove won't get called.
	 *
	 * Example: node/#/port/#/lun had 2 as the only subdir
	 *   scsi_lun_vertex_remove(..., 2) will remove the "lun"
	 *   vertex also and will return GRAPH_VERTEX_NONE.  In this case
	 *   scsi_node_port_vertex_remove will get called and clearly we
	 *   can remove # vertex of the "port/#" path.
	 *
	 * In a similar vein, we can determine if the node vertex should
	 * be removed.
	 *
	 * remove the edge corr. to specific target
	 *
	 * XXX- unfortunately, the reference counts on
	 * the verticies are not right, so just removing
	 * the edges does not cause the verticies to go
	 * away, and trying to destroy the verticies
	 * themselves will fail (since io/graph still
	 * thinks there are outstanding verticies). So,
	 * no matter what you *think* is happening, you
	 * are still ending up with dangling verticies.
	 */
	sprintf(loc_str, "%llx", port);
	rv = hwgraph_edge_remove(port_vhdl, loc_str, &tmpvhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (tmpvhdl != GRAPH_VERTEX_NONE));
	scsi_targ_info_free(tmpvhdl);
	hwgraph_vertex_unref(tmpvhdl);
	hwgraph_vertex_destroy(tmpvhdl);

	/*
	 * At this point, the vertex corresponding to the port number has been
	 * removed.  Now see if we need to remove port and node number.  This
	 * would be the case if the node had been accessible through just one
	 * port.
	 */
	edge = EDGE_PLACE_WANT_REAL_EDGES;
	rv = hwgraph_edge_get_next(port_vhdl, loc_str, &tmpvhdl, &edge);
	hwgraph_vertex_unref(port_vhdl);
	hwgraph_vertex_unref(nodenum_vhdl);
	if (rv == GRAPH_SUCCESS) {
		hwgraph_vertex_unref(tmpvhdl);
		hwgraph_vertex_unref(node_vhdl);
		return tmpvhdl;
	}
	hwgraph_edge_remove(nodenum_vhdl, "port", NULL);
	hwgraph_vertex_destroy(port_vhdl);
	sprintf(loc_str, "%llx", node);
	hwgraph_edge_remove(node_vhdl, loc_str, NULL);
	hwgraph_vertex_destroy(nodenum_vhdl);

	/*
	 * Now see if the node vertex needs to be removed.  (I.E. if
	 * only this one node was being accessed).
	 */
	edge = EDGE_PLACE_WANT_REAL_EDGES;
	rv = hwgraph_edge_get_next(node_vhdl, loc_str, &tmpvhdl, &edge);
	hwgraph_vertex_unref(node_vhdl);
	if (rv == GRAPH_SUCCESS) {
		hwgraph_vertex_unref(tmpvhdl);
		return tmpvhdl;
	}
	hwgraph_edge_remove(ctlr_vhdl, "node", NULL);
	hwgraph_vertex_destroy(node_vhdl);

	return GRAPH_VERTEX_NONE;
}


/*
 * add a target vertex to the graph and put the info 
 * associated with it
 */
vertex_hdl_t
scsi_targ_vertex_add(vertex_hdl_t	ctlr_vhdl,
		     targ_t		targ)
{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	vertex_hdl_t		targ_vhdl = GRAPH_VERTEX_NONE;
	char			loc_str[LOC_STR_MAX_LEN];
	scsi_targ_info_t	*targ_info;	
	scsi_ctlr_info_t	*ctlr_info = scsi_ctlr_info_get(ctlr_vhdl);

	if (scsi_targ_vertex_exist(ctlr_vhdl,targ,&targ_vhdl))
		return(targ_vhdl);
	/* 
	 * create the new vertex for the scsi disk to
	 * hang off the resp. controller vertex
	 */
	sprintf(loc_str,TARG_NUM_FORMAT,targ);
	rv = hwgraph_path_add(ctlr_vhdl,loc_str,&targ_vhdl);

	ASSERT((rv == GRAPH_SUCCESS) && (targ_vhdl != GRAPH_VERTEX_NONE));

	/*
	 * allocate memory 
	 */
	targ_info 			= scsi_targ_info_init();
	STI_TARG_VHDL(targ_info)	= targ_vhdl;
	STI_TARG(targ_info)		= targ;
	STI_CTLR_INFO(targ_info) 	= ctlr_info;

	/*
	 * put the info in the graph
	 */
	(void)scsi_targ_info_put(targ_vhdl,targ_info);
	/* Unreference the reference to targ_vhdl got thru
	 * hwgraph_path_add above 
	 */
	hwgraph_vertex_unref(targ_vhdl);
	return targ_vhdl;
}

vertex_hdl_t
scsi_node_port_vertex_add(vertex_hdl_t ctlr_vhdl, uint64_t node, uint64_t port)
{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	vertex_hdl_t		targ_vhdl = GRAPH_VERTEX_NONE;
	char			loc_str[LOC_STR_MAX_LEN];
	scsi_targ_info_t	*targ_info;	
	scsi_ctlr_info_t	*ctlr_info = scsi_ctlr_info_get(ctlr_vhdl);

	if (scsi_node_port_vertex_exist(ctlr_vhdl, node, port, &targ_vhdl))
		return(targ_vhdl);
	/* 
	 * create the new vertex for the scsi disk to
	 * hang off the resp. controller vertex
	 */
	sprintf(loc_str, NODE_PORT_NUM_FORMAT, node, port);
	rv = hwgraph_path_add(ctlr_vhdl, loc_str, &targ_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (targ_vhdl != GRAPH_VERTEX_NONE));

	/*
	 * allocate memory 
	 */
	targ_info 			= scsi_targ_info_init();
	STI_TARG_VHDL(targ_info)	= targ_vhdl;
	STI_CTLR_INFO(targ_info) 	= ctlr_info;
	STI_NODE(targ_info)		= node;
	STI_PORT(targ_info)		= port;

	/*
	 * put the info in the graph
	 */
	(void)scsi_targ_info_put(targ_vhdl,targ_info);

	/* Unreference the reference to targ_vhdl from hwgraph_path_add above */
	hwgraph_vertex_unref(targ_vhdl);
	return targ_vhdl;
}

/*
 * interface functions to scsi lun info 
 */

/* 
 * allocate memory for and initialize the scsi lun info
 */
scsi_lun_info_t *
scsi_lun_info_init(void) 
{
	scsi_lun_info_t		*info;

	info = (scsi_lun_info_t *)kern_calloc(1,sizeof(*info));
	ASSERT(info);
	
	return info;
}


void
scsi_lun_info_free(vertex_hdl_t 	vhdl)
{
	scsi_lun_info_t		*info;

	info = (scsi_lun_info_t *)hwgraph_fastinfo_get(vhdl);
	hwgraph_fastinfo_set(vhdl,NULL);
	if (info) 
		kern_free(info);
}

/*
 * associate the lun info  with its vertex
 */
void
scsi_lun_info_put(vertex_hdl_t 		vhdl,
		   scsi_lun_info_t 	*info)
{
	(void)hwgraph_fastinfo_set(vhdl,(arbitrary_info_t)info);
}

/*
 * get the lun info associated with this vertex
 */
scsi_lun_info_t *
scsi_lun_info_get(vertex_hdl_t		vhdl)
{
	scsi_lun_info_t		*info;

	info = (scsi_lun_info_t *)hwgraph_fastinfo_get(vhdl);
	ASSERT(info);

	return	info;
}
/*
 * check if lun vertex already exists 
 */

int
scsi_lun_vertex_exist(vertex_hdl_t	targ_vhdl,
		      lun_t		lun,
		      vertex_hdl_t	*to)
{
	graph_error_t		rv;
	char			loc_str[LOC_STR_MAX_LEN];

	sprintf(loc_str,LUN_NUM_FORMAT,lun);
	if ((rv = hwgraph_traverse(targ_vhdl,
				   loc_str,
				   to)) == GRAPH_SUCCESS)
		hwgraph_vertex_unref(*to);
	return ((rv == GRAPH_NOT_FOUND) ? 
		VERTEX_NOT_FOUND 	: 
		VERTEX_FOUND);
}

/*===========================================================================*/

char *smfd_suffix[] = {"48","96","96hi","3.5.800k","3.5","3.5hi","3.5.20m" };

/*
 * Check for type of several different floppy drives.
 */
static int
smfdtype(u_char *inv)
{
	extern single_inq_str_s		floppy_inq_strings[];
	single_inq_str_t		p;

	for (p = floppy_inq_strings; p->str; ++p) {
		if (strncmp((char *)&inv[p->offset], p->str, strlen(p->str)) == 0)
			return((int)p->arg);
	}
        return 0;
}

/*
 * create aliases from the /hw/rdisk directory of
 * the form fds#d#l0.<suffix> 
 */
void
smfd_alias_make(vertex_hdl_t	mode_vhdl)
{
	int 			type;
	dev_t			hwrdisk,hwdisk;
	/* REFERENCED */
	graph_error_t		rv;
	char			alias_prefix[30];
	vertex_hdl_t		unit_vhdl = hwgraph_connectpt_get(hwgraph_connectpt_get(mode_vhdl));
	int			ctlr_num;
	scsi_unit_info_t	*unit_info;


	/* create if /hw/rdisk directory is not existent */
	if ((hwrdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_RDISK)) == NODEV) {
		rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_RDISK , &hwrdisk);
		ASSERT(rv == GRAPH_SUCCESS);
	}
	/* create if /hw/disk directory is not existent */
	if ((hwdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_DISK)) == NODEV) {
		rv = hwgraph_path_add(hwgraph_root, EDGE_LBL_DISK , &hwdisk);
		ASSERT(rv == GRAPH_SUCCESS);
	}
	

	/* form the "fds#d#l0." part of the alias name */

	unit_info = scsi_unit_info_get(unit_vhdl);
	ctlr_num = device_controller_num_get(SUI_CTLR_VHDL(unit_info));
	if (SUI_LUN(unit_info))
		sprintf(alias_prefix,"fds%dd%dl%d.",ctlr_num,
			SUI_TARG(unit_info),SUI_LUN(unit_info));
	
	else
		sprintf(alias_prefix,"fds%dd%d.",ctlr_num,
			SUI_TARG(unit_info));

	device_controller_num_set(unit_vhdl, ctlr_num);

	for(type = 0 ; type < SMFD_MAX_TYPES ; type++) {
		vertex_hdl_t	floppy_vhdl;
		graph_error_t	rv;
		char		alias[40];
		char		loc_str[40];

		sprintf(alias,"%s%s",alias_prefix,smfd_suffix[type]);
		sprintf(loc_str,"%s/%s",smfd_suffix[type],EDGE_LBL_CHAR);
		if (hwgraph_traverse(unit_vhdl,
				     loc_str,
				     &floppy_vhdl) == GRAPH_SUCCESS) {
			vertex_hdl_t	to;
			/* for each valid suffix append it to the alias
			 * prefix created above to get the full alias
			 */
			if (hwgraph_traverse(hwrdisk,alias,&to) 
			    !=  GRAPH_SUCCESS) {
				rv = hwgraph_edge_add(hwrdisk, floppy_vhdl,alias);
				ASSERT(rv == GRAPH_SUCCESS);rv =rv; 
			}
		}

		sprintf(loc_str,"%s/%s",smfd_suffix[type],EDGE_LBL_BLOCK);
		if (hwgraph_traverse(unit_vhdl,
				     loc_str,
				     &floppy_vhdl) == GRAPH_SUCCESS) {
			vertex_hdl_t	to;

			/* for each valid suffix append it to the alias
			 * prefix created above to get the full alias
			 */
			if (hwgraph_traverse(hwdisk,alias,&to)
			    != GRAPH_SUCCESS) {
				rv = hwgraph_edge_add(hwdisk, floppy_vhdl,alias);
				ASSERT(rv == GRAPH_SUCCESS);rv =rv; 
			}
		}

	}


}
/* 
 * remove the floppy aliases of the type fds#d# aliases
 * from the /hw/disk & /hw/rdisk directories
 */
void
smfd_alias_remove(vertex_hdl_t	unit_vhdl)
{
	int 			type;
	dev_t			hwrdisk,hwdisk;
	char			alias_prefix[30];
	int			ctlr_num;
	scsi_unit_info_t	*unit_info;


	hwrdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_RDISK);
	hwdisk = hwgraph_path_to_dev("/hw/" EDGE_LBL_DISK);
	

	/* form the "fds#d#l0." part of the alias name */

	unit_info = scsi_unit_info_get(unit_vhdl);
	if ((ctlr_num = device_controller_num_get(SUI_CTLR_VHDL(unit_info))) >= 0)
		sprintf(alias_prefix,"fds%dd%dl0.",ctlr_num,SUI_TARG(unit_info));
	else

		sprintf(alias_prefix,"fds-1d%dl0.",SUI_TARG(unit_info));

	for(type = 0 ; type < SMFD_MAX_TYPES ; type++) {
		vertex_hdl_t	to;
		graph_error_t	rv;
		char		alias[40];

		sprintf(alias,"%s%s",alias_prefix,smfd_suffix[type]);
		if ((hwrdisk != NODEV) &&
		    (hwgraph_traverse(hwrdisk,alias,&to) ==  GRAPH_SUCCESS)) {
			rv = hwgraph_edge_remove(hwrdisk,alias,NULL);
			ASSERT(rv == GRAPH_SUCCESS);rv =rv; 
		}
	
		if ((hwdisk != NODEV) &&
		    (hwgraph_traverse(hwdisk,alias,&to) == GRAPH_SUCCESS)) {
			rv = hwgraph_edge_remove(hwdisk,alias,NULL);
			ASSERT(rv == GRAPH_SUCCESS);rv =rv; 
		}
	}
}


/*
 * Remove a multi-directory alias of the format:
 *
 * /hw/<device-type>/<node-number>/<mode-of-access>/<path-of-access>
 *
 * For examples:
 *   /hw/scsi/2000002037003c6c/lun0/c5p2
 *   /hw/rdisk/2000002037003c6c/lun0s7/c5p2
 *
 * We will also remove the node and mode directories if appropriate.
 *
 * The base_vhdl argument should be the vhdl to the /hw/scsi or /hw/disk
 * directory.
 */
void
scsi_alias_edge_remove(vertex_hdl_t base_vhdl, char *node, char *mode, char *path)
{
	vertex_hdl_t		node_vhdl, mode_vhdl;
	graph_edge_place_t	edge;
	/* REFERENCED */
	graph_error_t		rv;

	if (hwgraph_traverse(base_vhdl, node, &node_vhdl) != GRAPH_SUCCESS ||
	    node_vhdl == GRAPH_VERTEX_NONE)
	{
		return;
	}

	if (hwgraph_traverse(node_vhdl, mode, &mode_vhdl) != GRAPH_SUCCESS ||
	    mode_vhdl == GRAPH_VERTEX_NONE)
	{
		hwgraph_vertex_unref(node_vhdl);
		return;
	}

	if (hwgraph_edge_remove(mode_vhdl, path, NULL) != GRAPH_SUCCESS)
	{
		hwgraph_vertex_unref(node_vhdl);
		hwgraph_vertex_unref(mode_vhdl);
		return;
	}

	/*
	 * At this point, the edge pointing to the canonical vertex has been
	 * removed.  Now see if we need to remove the node and mode vertices.
	 */
	edge = EDGE_PLACE_WANT_REAL_EDGES;
	rv = hwgraph_edge_get_next(mode_vhdl, NULL, NULL, &edge);
	hwgraph_vertex_unref(mode_vhdl);
	if (rv == GRAPH_SUCCESS) {
		hwgraph_vertex_unref(node_vhdl);
		return;
	}

	/*
	 * Remove the mode vertex.
	 */
	hwgraph_edge_remove(node_vhdl, mode, NULL);
	hwgraph_vertex_destroy(mode_vhdl);

	/*
	 * Now see if the node vertex needs to be removed.  (I.E. if
	 * only this one node was being accessed).
	 */
	edge = EDGE_PLACE_WANT_REAL_EDGES;
	rv = hwgraph_edge_get_next(node_vhdl, NULL, NULL, &edge);
	hwgraph_vertex_unref(node_vhdl);
	if (rv == GRAPH_SUCCESS) {
		return;
	}
	hwgraph_edge_remove(base_vhdl, node, NULL);
	hwgraph_vertex_destroy(node_vhdl);

	return;
}

/*===========================================================================*/
#define MAX_SCSI_PARTITIONS		16
/*
 * remove the aliases if a particular device(disk/tape/floppy) has
 * been removed.
 */
void
scsi_cleanup(vertex_hdl_t	vhdl,char *lun_id)
{
	scsi_lun_info_t		*lun_info;
	lun_t			lun;
	dev_t			disk_vhdl,lun_vhdl;
	dev_t			tape_vhdl,floppy_vhdl,scsi_vhdl;
	dev_t			hwscsi;
	int			contr;
	targ_t			targ;
	uint64_t		node, port;
	/* REFERENCED */
	graph_error_t		rv;

	/* get the particular lun device vertex we are interested in */
	rv = hwgraph_traverse(vhdl,lun_id,&lun_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	lun_info = scsi_lun_info_get(lun_vhdl);
	contr 	= device_controller_num_get(SLI_CTLR_VHDL(lun_info));
	targ	= SLI_TARG(lun_info);
	node	= SLI_NODE(lun_info);
	port	= SLI_PORT(lun_info);
	lun  	= SLI_LUN(lun_info);

	/* Clean up any inventory information hanging off this lun vertex. */
	(void)hwgraph_inventory_remove(lun_vhdl,-1,-1,-1,-1,-1);

	/* remove the sc#d#.. alias */
	if ((hwscsi = hwgraph_path_to_dev("/hw/"EDGE_LBL_SCSI)) != NODEV) {
		char	nodename[20];
		char	mode[20];
		char	path[40];
		dev_t	to;
		
		if (node) {
			sprintf(nodename, "%llx", node);
			sprintf(mode, "lun%d", lun);
			sprintf(path, "c%dp%llx", contr, port);
			scsi_alias_edge_remove(hwscsi, nodename, mode, path);
		}
		else {
			sprintf(path,"sc%dd%dl%d",contr,targ,lun);
			if (hwgraph_traverse(hwscsi,path,&to) == GRAPH_SUCCESS) {
				hwgraph_edge_remove(hwscsi,path,NULL);
				hwgraph_vertex_unref(to);
			}
		}
		hwgraph_vertex_unref(hwscsi);
	}
	/* check if there's a devscsi edge */
	if (hwgraph_traverse(lun_vhdl,
			     EDGE_LBL_SCSI,
			     &scsi_vhdl) == GRAPH_SUCCESS) {
	        /* 
		 * Remove the inventory info corresponding to the tape
		 * device so that hinv reflects this properly 
		 */
		hwgraph_inventory_remove(scsi_vhdl, -1, -1, -1, -1, -1);
		/* Remove the edge to the scsi vertex in main hwgraph
		 * and destroy the vertex.
		 */
		hwgraph_edge_remove(lun_vhdl,EDGE_LBL_SCSI,NULL);
		scsi_dev_info_free(scsi_vhdl);
		hwgraph_vertex_unref(scsi_vhdl);
		hwgraph_vertex_destroy(scsi_vhdl);
	}

	/* check if the device corresponds to a tape */
	if (hwgraph_traverse(lun_vhdl,
			     EDGE_LBL_TAPE,
			     &tape_vhdl) == GRAPH_SUCCESS) {
		extern void ct_alias_remove(vertex_hdl_t);

		ct_alias_remove(tape_vhdl);
		/* 
		 * Remove the inventory info corresponding to the tape
		 * device so that hinv reflects this properly 
		 */
		hwgraph_inventory_remove(tape_vhdl, -1, -1, -1, -1, -1);
		scsi_device_detach(tape_vhdl);
		hwgraph_vertex_unref(tape_vhdl);
		hwgraph_edge_remove(lun_vhdl,EDGE_LBL_TAPE,NULL);
		/* This has to be done properly once the cdl layer
		 * reference counting is fixed.
		 */
		while (hwgraph_vertex_destroy(tape_vhdl) != GRAPH_SUCCESS);
		hwgraph_vertex_unref(lun_vhdl);
		return;
	}
	/* check if the device corresponds to a floppy */
	if (hwgraph_traverse(lun_vhdl,
			     EDGE_LBL_FLOPPY,
			     &floppy_vhdl) == GRAPH_SUCCESS) {
		extern void smfd_alias_remove(vertex_hdl_t);
		
		smfd_alias_remove(floppy_vhdl);
		/*
		 * Remove the inventory info corresponding to the floppy
		 * device so that hinv reflects this properly
		 */
		hwgraph_inventory_remove(floppy_vhdl, -1, -1, -1, -1, -1);
		return;
	}
	/* check if the device corresponds to a disk */
	if (hwgraph_traverse(lun_vhdl,
			     EDGE_LBL_DISK,
			     &disk_vhdl) == GRAPH_SUCCESS) {
		/* 
		 * Remove the inventory info corresponding to the disk
		 * device so that hinv reflects this properly
		 */
		hwgraph_inventory_remove(disk_vhdl, -1, -1, -1, -1, -1);

		/* Remove the name space beneath the disk vertex */
		dk_namespace_remove(disk_vhdl);
		hwgraph_vertex_unref(disk_vhdl);
		scsi_disk_vertex_remove(lun_vhdl);
		hwgraph_vertex_unref(lun_vhdl);
	}
	else
		hwgraph_vertex_unref(lun_vhdl);
}

/*
 * remove the target vertex corr. to the given
 * unit number
 */
vertex_hdl_t
scsi_lun_vertex_remove(vertex_hdl_t 	targ_vhdl,
			targ_t		lun)

{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	vertex_hdl_t		vhdl_1,vhdl_2;
	char			loc_str[LOC_STR_MAX_LEN];

	rv = hwgraph_traverse(targ_vhdl,
			      LUN_FORMAT,
			      &vhdl_1);
	ASSERT((rv == GRAPH_SUCCESS) &&
	       (vhdl_1 != GRAPH_VERTEX_NONE));

	/*
	 * Cleanup failover structures 
	 */
	{
		scsi_lun_info_t         *lun_info;
		struct scsi_fo_instance *foi;

		sprintf(loc_str,"%d", lun);
		rv = hwgraph_traverse(vhdl_1, 
					loc_str,
					&vhdl_2);
		ASSERT((rv == GRAPH_SUCCESS) &&
			 (vhdl_2 != GRAPH_VERTEX_NONE));
		hwgraph_vertex_unref(vhdl_2);
		lun_info = scsi_lun_info_get(vhdl_2);
		foi = SLI_FO_INFO(lun_info);
		if (foi)
			fo_scsi_lun_remove(vhdl_2);
	}

	/*
	 * remove the edge corr. to specific lun
	 */
	sprintf(loc_str,"%d",lun);
	scsi_cleanup(vhdl_1, loc_str);
	rv = hwgraph_edge_remove(vhdl_1,
				 loc_str,
				 &vhdl_2);
	ASSERT((rv == GRAPH_SUCCESS) &&
	       (vhdl_2 != GRAPH_VERTEX_NONE));

	scsi_lun_info_free(vhdl_2);
	hwgraph_vertex_unref(vhdl_2);
	hwgraph_vertex_destroy(vhdl_2);
	/* check if there is a vertex for any specific lun */
	for (lun = 0 ; lun < SCSI_MAXLUN ; lun++) {
		char		str[2];
		vertex_hdl_t	vhdl_3;

		sprintf(str,"%d",lun);
		if (hwgraph_traverse(vhdl_1,str,&vhdl_3) == GRAPH_SUCCESS) {
			hwgraph_vertex_unref(vhdl_3);
			hwgraph_vertex_unref(vhdl_1);
			return vhdl_3;
		}
	}

	/* no specific lun vertex exists. remove the lun edge under target/# */
	rv = hwgraph_edge_remove(targ_vhdl,LUN_FORMAT,NULL);
	ASSERT(rv == GRAPH_SUCCESS);
	hwgraph_vertex_unref(vhdl_1);
	hwgraph_vertex_destroy(vhdl_1);
	return GRAPH_VERTEX_NONE;
}

/*
 * add a target vertex to the graph and put the info 
 * associated with it
 */

vertex_hdl_t
scsi_lun_vertex_add(vertex_hdl_t	targ_vhdl,
		    lun_t		lun)

{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	vertex_hdl_t		lun_vhdl = GRAPH_VERTEX_NONE;
	char			loc_str[LOC_STR_MAX_LEN];
	scsi_lun_info_t		*lun_info;	
	scsi_targ_info_t	*targ_info = scsi_targ_info_get(targ_vhdl);

	if (scsi_lun_vertex_exist(targ_vhdl,lun,&lun_vhdl))
		return(lun_vhdl);

	/* 
	 * create the new vertex for the scsi disk to
	 * hang off the resp. controller vertex
	 */
	sprintf(loc_str ,LUN_NUM_FORMAT, lun);
	rv = hwgraph_path_add(targ_vhdl, loc_str,&lun_vhdl);
	ASSERT((rv == GRAPH_SUCCESS) && (lun_vhdl != GRAPH_VERTEX_NONE));

	/*
	 * allocate memory 
	 */
	lun_info 			= scsi_lun_info_init();
	SLI_LUN_VHDL(lun_info)		= lun_vhdl;
	SLI_LUN(lun_info)		= lun;
	SLI_TARG_INFO(lun_info) 	= targ_info;

	/*
	 * put the info in the graph
	 */
	(void)scsi_lun_info_put(lun_vhdl,lun_info);
	hwgraph_vertex_unref(lun_vhdl);
	return lun_vhdl;
}
/*
 * interface functions to scsi unit info
 */
scsi_unit_info_t *
scsi_unit_info_get(vertex_hdl_t	unit_vhdl)
{
	scsi_unit_info_t	*unit_info;

	unit_info = (scsi_unit_info_t *)hwgraph_fastinfo_get(unit_vhdl);
	ASSERT(unit_info);
	return unit_info;
}

void
scsi_unit_info_put(vertex_hdl_t 	unit_vhdl,
		   scsi_unit_info_t	*unit_info)
{
	(void)hwgraph_fastinfo_set(unit_vhdl,(arbitrary_info_t)unit_info);
}

/*
 * interface functions to scsi disk info 
 */

/* 
 * allocate memory for and initialize the scsi dev info
 */
scsi_dev_info_t *
scsi_dev_info_init(void) 
{
	scsi_dev_info_t	*info;

	info = (scsi_dev_info_t *)kern_calloc(1,sizeof(*info));
	ASSERT(info);


	return info;
}

void
scsi_dev_info_free(vertex_hdl_t 	vhdl)
{
	scsi_dev_info_t	*info;

	info = (scsi_dev_info_t *)hwgraph_fastinfo_get(vhdl);
	hwgraph_fastinfo_set(vhdl,NULL);
	if (info)
		kern_free(info);
}

/*
 * associate the scsi dev info  with its vertex
 */
void
scsi_dev_info_put(vertex_hdl_t		vhdl,
		   scsi_dev_info_t 	*info)
{
	(void)hwgraph_fastinfo_set(vhdl,(arbitrary_info_t)info);
}

/*
 * get the scsi dev info associated with this vertex
 */
scsi_dev_info_t *
scsi_dev_info_get(vertex_hdl_t		vhdl)
{
	scsi_dev_info_t	*info;

	info = (scsi_dev_info_t *)hwgraph_fastinfo_get(vhdl);
	ASSERT(info);

	return	info;
}
/*
 * interface functions to scsi disk info 
 */

/* 
 * allocate memory for and initialize the scsi disk info
 */
scsi_disk_info_t *
scsi_disk_info_init(void) 
{
	scsi_disk_info_t	*info;

	info = (scsi_disk_info_t *)kern_calloc(1,sizeof(*info));
	ASSERT(info);


	return info;
}

void
scsi_disk_info_free(vertex_hdl_t 	vhdl)
{
	scsi_disk_info_t	*info;

	info = (scsi_disk_info_t *)hwgraph_fastinfo_get(vhdl);
	hwgraph_fastinfo_set(vhdl,NULL);
	if (info)
		kern_free(info);
}

/*
 * associate the disk info  with its vertex
 */
void
scsi_disk_info_put(vertex_hdl_t		vhdl,
		   scsi_disk_info_t 	*info)
{
	(void)hwgraph_fastinfo_set(vhdl,(arbitrary_info_t)info);
}

/*
 * get the disk info associated with this vertex
 */
scsi_disk_info_t *
scsi_disk_info_get(vertex_hdl_t		vhdl)
{
	scsi_disk_info_t	*info;

	info = (scsi_disk_info_t *)hwgraph_fastinfo_get(vhdl);
	ASSERT(info);

	return	info;
}

/*
 * interface functions to scsi part info 
 */

/* 
 * allocate memory for and initialize the scsi part info
 */
scsi_part_info_t *
scsi_part_info_init(void) 
{
	scsi_part_info_t	*info;

	info = (scsi_part_info_t *)kern_calloc(1,sizeof(*info));
	ASSERT(info);
	
	return info;
}


void
scsi_part_info_free(vertex_hdl_t 	vhdl)
{
	scsi_part_info_t	*info;

	info = (scsi_part_info_t *)hwgraph_fastinfo_get(vhdl);
	hwgraph_fastinfo_set(vhdl,NULL);
	if (info)
		kern_free(info);
}

/*
 * associate the part info  with its vertex
 */
void
scsi_part_info_put(vertex_hdl_t 	vhdl,
		   scsi_part_info_t 	*info)
{
	(void)hwgraph_fastinfo_set(vhdl,(arbitrary_info_t)info);
}

/*
 * get the part info associated with this vertex
 */
scsi_part_info_t *
scsi_part_info_get(vertex_hdl_t		vhdl)
{
	scsi_part_info_t	*info;

	info = (scsi_part_info_t *)hwgraph_fastinfo_get(vhdl);
	ASSERT(info);

	return	info;
}

/*
 * Utility to check for CD-ROM key words in inquiry data
 */
int
cdrom_inquiry_test(u_char *inq)
{
	register int i;
	extern char *cdrom_inq_strings[];	/* NULL terminated */

	/* check each string in master.d table against inquiry data */
	for (i = 0; cdrom_inq_strings[i]; ++i) {
		if (strstr((char *) inq, cdrom_inq_strings[i]))
			return 1;
	}

	return 0;
}

/*
 * Check if device is an SGI supported RAID
 */
int
raid_inquiry_test(u_char *inq)
{
	extern double_inq_str_s	raid_inq_strings[];
	double_inq_str_t	p;	

	for (p = raid_inq_strings; p->str1; ++p) {
		if ((strncmp((char *) &inq[p->offset1], p->str1, strlen(p->str1)) == 0) &&
		    (strncmp((char *) &inq[p->offset2], p->str2, strlen(p->str2)) == 0))
			return(1);
	}
	return(0);
}

/*
 * Some devices lie about whether they support multi-LUNs. Exception
 * those cases 
 */
int
scsi_device_is_multilun(u_char *inv)
{
	extern single_inq_str_s         multilun_exc_inq_strings[];
	single_inq_str_t                p;

	if(inv[8] == 0) return 0;	/* hack for bug 564484;
		insite returns no data on lun1-7, but adapter drivers
		forget to check for this */
        for (p = multilun_exc_inq_strings; p->str; ++p) {
                if (strncmp((char *)&inv[p->offset], p->str, strlen(p->str)) == 0)
                        return(0);
        }
        return(1);
}

/*
 * Some type 0 removable devices, like Syquest, need to be attached to
 * both the floppy driver and the disk driver.
 */
int
scsi_device_is_multimode(u_char *inv)
{
	extern single_inq_str_s         multimode_inq_strings[];
        single_inq_str_t                p;

        for (p = multimode_inq_strings; p->str; ++p) {
                if (strncmp((char *)&inv[p->offset], p->str, strlen(p->str)) == 0)
                        return(1);
        }
        return(0);
}

/*
 * check if disk vertex already exists 
 */
int
scsi_disk_vertex_exist(vertex_hdl_t	lun_vhdl,
		       vertex_hdl_t	*disk_vhdl)
{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERNCED */

	if ((rv = hwgraph_traverse(lun_vhdl,
				   EDGE_LBL_DISK,
				   disk_vhdl)) == GRAPH_SUCCESS)
		hwgraph_vertex_unref(*disk_vhdl);
	return ((rv == GRAPH_NOT_FOUND) ? 
		VERTEX_NOT_FOUND 	: 
		VERTEX_FOUND);
}
/*
 * remove the disk vertex corr. 
 */
void
scsi_disk_vertex_remove(vertex_hdl_t 	lun_vhdl)

{
	/* REFERENCED */
	graph_error_t		rv;
	vertex_hdl_t		disk_vhdl;

	rv = hwgraph_edge_remove(lun_vhdl, EDGE_LBL_DISK, &disk_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	scsi_disk_info_free(disk_vhdl);
	hwgraph_vertex_unref(disk_vhdl);
	rv = hwgraph_vertex_destroy(disk_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
}

/*
 * add a disk vertex to the graph and put the info 
 * associated with it
 * create a well known path to this vertex also
 */
/* ARGSUSED */
vertex_hdl_t
scsi_disk_vertex_add(vertex_hdl_t	lun_vhdl,
		     int		type)
{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	vertex_hdl_t		disk_vhdl = GRAPH_VERTEX_NONE;
	scsi_lun_info_t		*lun_info = scsi_lun_info_get(lun_vhdl);
	scsi_disk_info_t	*disk_info = NULL;
	
	if (scsi_disk_vertex_exist(lun_vhdl,&disk_vhdl))
		return(disk_vhdl);
	/* 
	 * create the new vertex for the scsi disk to
	 * hang off the resp. controller vertex
	 */

	rv = hwgraph_path_add(lun_vhdl,
			      EDGE_LBL_DISK,
			      &disk_vhdl);

	ASSERT((rv == GRAPH_SUCCESS) && (disk_vhdl != GRAPH_VERTEX_NONE));

	/*
	 * initialize the disk info 
	 */
	disk_info = scsi_disk_info_init();
	
	/*
	 * put the pointer to the lun info in the
	 * disk info structure
	 */

	SDI_LUN_INFO(disk_info) = lun_info;

	SDI_DISK_VHDL(disk_info) = disk_vhdl;	

	/*
	 * initialize the general-purpose dksc semaphore
	 */
	init_mutex(&(SDI_DKSEMA(disk_info)),
		   MUTEX_DEFAULT,
		   "dks",disk_vhdl);

	/*
	 * put the info in the graph
	 */
	(void)scsi_disk_info_put(disk_vhdl, disk_info);
	hwgraph_vertex_unref(disk_vhdl);
	return disk_vhdl;
}

/*
 * check if a partition vertex already exists 
 */
int
scsi_part_vertex_exist(vertex_hdl_t	disk_vhdl,
		       uchar_t		part,
		       vertex_hdl_t	*to)
{
	/* REFERENCED */
	graph_error_t		rv;
	char			loc_str[LOC_STR_MAX_LEN];


	SPRINTF_PART(part,loc_str);
	if ((rv = hwgraph_traverse(disk_vhdl,
				   loc_str,
				   to)) == GRAPH_SUCCESS)
		hwgraph_vertex_unref(*to);
	return ((rv == GRAPH_NOT_FOUND) ? 
		VERTEX_NOT_FOUND 	: 
		VERTEX_FOUND);
}

/*
 * remove the partition vertex corr. to a given
 * partition number
 */
void
scsi_part_vertex_remove(vertex_hdl_t 	disk_vhdl,
			uchar_t		part)
{
	/* REFERENCED */
	graph_error_t	rv;
	/* REFERENCED */
	char		edge_name[LOC_STR_MAX_LEN];
	vertex_hdl_t	part_vhdl,vhdl;
	vertex_hdl_t	bpart_vhdl = GRAPH_VERTEX_NONE;
	vertex_hdl_t	cpart_vhdl = GRAPH_VERTEX_NONE;
	void 		dk_remove_hw_disk_alias(dev_t);


	SPRINTF_PART(part,edge_name);
	
	/* get to the partition vertex from the disk vertex */
	rv = hwgraph_traverse(disk_vhdl,edge_name,&part_vhdl);
	ASSERT(rv != GRAPH_NOT_FOUND);

	dk_remove_hw_disk_alias(part_vhdl);

	/* Remove the "block" vertex */
	bpart_vhdl = hwgraph_block_device_get(part_vhdl);
	ASSERT(bpart_vhdl != GRAPH_VERTEX_NONE);
	hwgraph_bdevsw_set(bpart_vhdl,NULL);
	hwgraph_edge_remove(part_vhdl,EDGE_LBL_BLOCK,NULL);
	hwgraph_vertex_unref(bpart_vhdl);
	hwgraph_vertex_destroy(bpart_vhdl);
	
	/* Remove the "char" vertex */
	cpart_vhdl = hwgraph_char_device_get(part_vhdl);
	ASSERT(cpart_vhdl != GRAPH_VERTEX_NONE);
	hwgraph_cdevsw_set(cpart_vhdl,NULL);
	hwgraph_edge_remove(part_vhdl,EDGE_LBL_CHAR,NULL);
	hwgraph_vertex_unref(cpart_vhdl);
	hwgraph_vertex_destroy(cpart_vhdl);	

	/* Unreference the ref got to part_vhdl from hwgraph_traverse() 
	 * above.
	 */
	hwgraph_vertex_unref(part_vhdl);

	if ((part == SCSI_DISK_VH_PARTITION) ||
	    (part == SCSI_DISK_VOL_PARTITION)) {
		part_vhdl = disk_vhdl;
		SPRINTF_PART(part,edge_name);
	} else {
		/* get the partition directory vertex handle */
		sprintf(edge_name,"%d",part);
		rv = hwgraph_traverse(disk_vhdl,EDGE_LBL_PARTITION,&part_vhdl);
		ASSERT(rv == GRAPH_SUCCESS);
		hwgraph_vertex_unref(part_vhdl);

	}
	/* remove the edge corresponding to the specific partition */
	rv = hwgraph_edge_remove(part_vhdl,edge_name,&vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	scsi_part_info_free(vhdl);
	hwgraph_vertex_unref(vhdl);
	/* destroy the actual vertex of the specific partition */
	hwgraph_vertex_destroy(vhdl);

	if ((part == SCSI_DISK_VH_PARTITION) ||
	    (part == SCSI_DISK_VOL_PARTITION))
		return;

	/* Check if we can remove the "partition" edge itself .
	 * Basically scan the "partition" directory to see if it
	 * is empty.
	 */
	for(part = 0 ; part < MAX_SCSI_PARTITIONS; part++) {	
		/* Ignore the volume & volume header partitions */
		if ((part == SCSI_DISK_VH_PARTITION) ||
		    (part == SCSI_DISK_VOL_PARTITION))
			continue;
		if (scsi_part_vertex_exist(disk_vhdl,part,&part_vhdl))
			return;
	}
	/* There is no valid partition in the "partition" directory.
	 * Remove the "partition" vertex itself 
	 */
	hwgraph_edge_remove(disk_vhdl,EDGE_LBL_PARTITION,&part_vhdl);
	hwgraph_vertex_unref(part_vhdl);
	hwgraph_vertex_destroy(part_vhdl);
}

/*
 * add a disk vertex to the graph and put the info 
 * associated with it
 * create a well known path to this vertex also
 */

vertex_hdl_t
scsi_part_vertex_add(vertex_hdl_t	disk_vhdl,
		     uchar_t		part,
		     boolean_t		makealias)

{
	/* REFERENCED */
	graph_error_t		rv;
	/* REFERENCED */
	dev_t			part_vhdl = GRAPH_VERTEX_NONE;
	char			loc_str[LOC_STR_MAX_LEN];
	scsi_disk_info_t	*disk_info = scsi_disk_info_get(disk_vhdl);
	scsi_part_info_t	*part_info;      
	vertex_hdl_t		char_vhdl=GRAPH_VERTEX_NONE, blk_vhdl=GRAPH_VERTEX_NONE;
	extern void 		dk_add_hw_disk_alias(dev_t , int );

	if (scsi_part_vertex_exist(disk_vhdl,part,&part_vhdl)) {
		return(part_vhdl);
	}
	SPRINTF_PART(part, loc_str);
	/* 
	 * create the new vertex for the scsi disk to
	 * hang off the resp. controller vertex
	 */
	hwgraph_device_add(disk_vhdl,
			   loc_str,
			   DKSC_PREFIX,
			   &part_vhdl,
			   &blk_vhdl,
			   &char_vhdl);
	ASSERT(part_vhdl != GRAPH_VERTEX_NONE);

#if DEBUG && HWG_DEBUG
	cmn_err(CE_CONT,"%v scsi_part_vertex_add\n",part_vhdl);
#endif
	/*
	 * initialize the part info structure
	 */
	part_info 			= scsi_part_info_init();
	
	/*
	 * put the pointer to the disk info in the
	 * part info
	 */

	SPI_PART_VHDL(part_info)	= part_vhdl;

	SPI_DISK_INFO(part_info) 	= disk_info;

	SPI_PART(part_info)		= part;
	/*
	 * put the info in the graph
	 */
	(void)scsi_part_info_put(part_vhdl,part_info);
	if (char_vhdl != GRAPH_VERTEX_NONE)
		(void)scsi_part_info_put(char_vhdl,part_info);
	if (blk_vhdl != GRAPH_VERTEX_NONE)
		(void)scsi_part_info_put(blk_vhdl,part_info);
	/*
	 * Cannot add alias until after info is set
	 */
	if (makealias == B_TRUE) {
		if ((part != SCSI_DISK_VOL_PARTITION) &&
		    (part != SCSI_DISK_VH_PARTITION)) 
			dk_add_hw_disk_alias(hwgraph_block_device_get(part_vhdl),
					     B_TRUE);
		dk_add_hw_disk_alias(hwgraph_char_device_get(part_vhdl),
				     B_FALSE);
	}
	hwgraph_vertex_unref(part_vhdl);
	hwgraph_vertex_unref(char_vhdl);
	hwgraph_vertex_unref(blk_vhdl);
	return part_vhdl;
}
/*----------------------------------------------------------------------*/
/* 
 * add a vertex corresponding to the scsi disk in the hardware 
 * graph and store the info associated with it
 * also create the vertices for the volume & volume header for this
 * disk and store the corr. info associated with them
 */
vertex_hdl_t
scsi_disk_update(	vertex_hdl_t lun_vhdl, int type) 
{
	vertex_hdl_t disk_vhdl;

	/* 
	 * check if the vertex already exists
	 */
	if (scsi_disk_vertex_exist(lun_vhdl, &disk_vhdl) == VERTEX_FOUND)
		return disk_vhdl;

	disk_vhdl = scsi_disk_vertex_add(lun_vhdl, type);
	dk_base_namespace_add(disk_vhdl, B_TRUE);

	return disk_vhdl;
}

vertex_hdl_t
scsi_dev_add(vertex_hdl_t lun_vhdl)
{
	vertex_hdl_t	scsi_vhdl;
	scsi_dev_info_t	*scsi_dev_info;
	/* REFERENCED */
	graph_error_t	rv;

	if (hwgraph_traverse(lun_vhdl,EDGE_LBL_SCSI,&scsi_vhdl)
	    != GRAPH_SUCCESS) {
		rv = hwgraph_char_device_add(lun_vhdl, 
					     EDGE_LBL_SCSI,
					     DS_PREFIX, 
					     &scsi_vhdl);
		ASSERT(rv == GRAPH_SUCCESS);

		if (!(hwgraph_fastinfo_get(scsi_vhdl))) {
			scsi_dev_info			= scsi_dev_info_init();
			SDEVI_DEV_VHDL(scsi_dev_info) 	= scsi_vhdl;
			SDEVI_LUN_INFO(scsi_dev_info) 	= scsi_lun_info_get(lun_vhdl);

			init_mutex(&(SDEVI_TDSLOCK(scsi_dev_info)),
				   MUTEX_DEFAULT,
				   "sdev", scsi_vhdl);
 
			scsi_dev_info_put(scsi_vhdl,scsi_dev_info);
		}
	} 
	hwgraph_vertex_unref(scsi_vhdl);
	return scsi_vhdl;
}

/*
 * scan hwgraph inventory to determine if inventory
 * already exists for this lun
 */
inventory_t *
scsi_find_inventory (vertex_hdl_t disk_vhdl, int class)
{
	invplace_t      place;
	inventory_t     *pinv;

	place = invplace_none;

        /*
	 * Fabric devices have a linked list (2 items) of inventory
	 * structures.  We need to check for the appropriate one.
	 */
	while (hwgraph_inventory_get_next(disk_vhdl, &place, &pinv) == GRAPH_SUCCESS) {
		if (pinv == NULL)
			break;
		if (pinv->inv_class == class)
			return pinv;
	}
	return NULL;
}

/*
 * creates a new vertex corr. to this scsi device in the 
 * hardware graph attached to the vertex corr. to the vhdl
 *
 */
void
scsi_device_update(u_char *inv, vertex_hdl_t lun_vhdl)
{
	char			*extra;
	char			*which;
	int			scsitype, disktype;
	int	 		eadap;
	char	              	scsi_dev_loc_str[LOC_STR_MAX_LEN]; /* we want to preserve *inv */
	vertex_hdl_t          	disk_vhdl, scsi_vhdl, known_vhdl, tmp_vhdl;
	int	 		israid = 0;
	scsi_lun_info_t		*lun_info;
	int			targ, lun;
	uint64_t		node, port;
	graph_error_t 		invrc = GRAPH_DUP;
	dev_t			spec_floppy;
	scsi_driver_reg_t	driver_info;
	vertex_hdl_t          	inv_vhdl = 0;
	int			inv_class;
	int			inv_type;
	int			inv_ctlr;
	int			inv_unit;
	int			inv_state;

	lun_info = scsi_lun_info_get(lun_vhdl);

	targ 	= SLI_TARG(lun_info);
	lun 	= SLI_LUN(lun_info);
	node	= SLI_NODE(lun_info);
	port	= SLI_PORT(lun_info);

	/*
	 * If LUN > 0 and the device is lying about multi-lunned support, bail out.
	 */
	if (lun > 0 && !scsi_device_is_multilun(inv))
		return;

	ASSERT(SLI_CTLR_INFO(lun_info));
	/*
	 * Set this to -1. ioconfig will call dsioctl to set it to the
	 * correct value at boot time.
	 */
	eadap 	=  -1;
	scsi_vhdl = scsi_dev_add(lun_vhdl);
	
	/*	
	 * Check if device is an SGI supported RAID
	 */
	if (((inv[0] & 0x1F) == 0) && raid_inquiry_test(inv))
	{
		israid = 1;
		if (lun == 0) {
			if (node != 0) {
				hwgraph_inventory_add(lun_vhdl, INV_FCNODE,
				      (uint32_t) (node >> 32), (uint32_t) node,
				      (uint32_t) (port >> 32), (uint32_t) port);
			}
		        invrc = hwgraph_inventory_add(lun_vhdl, 
					INV_SCSI, INV_RAIDCTLR,
					eadap, targ,
					inv[2] & INV_SCSI_MASK);
		}
	}

	fo_scsi_device_update(inv, lun_vhdl);

	/*
	 * Check the peripheral qualifier - non-zero means LUN is
	 * either not supported or is not currently attached. RAID
	 * provides a particular challenge in that following a
	 * failover, the LUNs will cease to be bound to a particular
	 * SP. In such a case, we want to remove the inventory
	 * entries, if they had previously existed. These hinv entries
	 * are attached to the "disk" vertices.
	 */
	if ((inv[0] & 0x60) && !(inv[0] & 0x80)) {
		invplace_t	place;
		inventory_t	*pinv;
		vertex_hdl_t	disk_vhdl;

		place.invplace_vhdl = GRAPH_VERTEX_NONE;

		if (hwgraph_traverse(lun_vhdl, EDGE_LBL_DISK, &disk_vhdl) == GRAPH_SUCCESS)
		{
		    	if (hwgraph_inventory_get_next(disk_vhdl, &place, &pinv) == GRAPH_SUCCESS)
				hwgraph_inventory_remove(disk_vhdl, -1, -1, -1, -1, -1);
			hwgraph_vertex_unref(disk_vhdl);		
		}
		return;
	}

	/*
	 * Get the registered driver info for this device
	 */
	driver_info = scsi_device_driver_info_get(inv);

	switch (inv[0] & 0x1F)
	{
	case 0: /* disk */
		extra = "disk";
		scsitype = 0;
		which = "";
		if (inv[1] & 0x80) {		/* removeable media */
			which = "removable ";
                	/*
                       	 *  This is used to distinguish PC Cards from other
                         *  removable media. Currently only Adtron is 
 			 *  supported. This is really quite ugly but 
			 *  since we don't want to treat PC Cards as floppies  
			 *  this is the only way...
                         */
                        if ( strncmp((char *)&inv[16], "SDDS", 4) == 0 ) {
				disktype = INV_PCCARD;
				scsitype = (lun << 8);
                        } else { 
				disktype = INV_SCSIFLOPPY;
				/* Check for TEAC, INSITE, IOMEGA, SyQuest floppy drives */
				scsitype = smfdtype(inv);
                        }
		} else {
			disktype = INV_SCSIDRIVE;
			scsitype = lun;
			if (israid) {
				extra = "RAID LUN";
				scsitype |= INV_RAID5_LUN;
			}
		}

		/*
		 * Check the type 0 registration to determine whether
		 * this device needs to be attached to a 3rd party
		 * driver. If not, then by default we'll attach it
		 * either the floppy driver (smfd.c) or the disk
		 * driver (dksc.c) depending on the value of disktype.
		 */
		if (driver_info) {
			disk_vhdl = scsi_device_register(lun_vhdl, inv, driver_info->sd_pathname_prefix);
			if (!scsi_device_is_attached(disk_vhdl))
				scsi_device_attach(disk_vhdl, driver_info->sd_driver_prefix);
		}
		else {
			inventory_t	*pinv;

			/*
			 * Check whether this is really a CDROM mascarading as a disk 
			 */
			if (((inv[0] & 0x1F) == 0) && cdrom_inquiry_test(&inv[8])) {
				inv[0] = INV_CDROM; 
				inv[1] = INV_REMOVE;
				goto cdrom;
			}

			/* 
			 * "real" floppy drives have a non-zero scsitype
			 * value.  Zip/Jaz/etc. drives have a disktype
			 * of INV_SCSIFLOPPY, but a scsitype that is zero.  
			 */
			if (disktype == INV_SCSIFLOPPY && scsitype != 0) {
				disk_vhdl = scsi_device_register(lun_vhdl, inv, EDGE_LBL_FLOPPY);
				if (hwgraph_traverse(disk_vhdl, 
						     SMFD_DEFAULT"/"EDGE_LBL_CHAR, 
						     &spec_floppy) != GRAPH_SUCCESS) {
					scsi_device_attach(disk_vhdl, "smfd");
				}
				if (scsi_device_is_multimode(inv))
					scsi_disk_update(lun_vhdl, disktype);
			} 
			else
				disk_vhdl = scsi_disk_update(lun_vhdl, disktype);
			pinv = scsi_find_inventory(disk_vhdl, INV_DISK);
			if (pinv == NULL) {
				inv_vhdl = disk_vhdl;
				inv_class = INV_DISK;
				inv_type = disktype;
				inv_ctlr = eadap;
				inv_unit = targ;
				inv_state = scsitype;
			}
			else if (pinv->inv_state&(INV_PRIMARY|INV_ALTERNATE))
				pinv->inv_state &= ~INV_FAILED;
		}
		break;

	case 1: /* sequential == tape */
		scsitype = tpsctapetype((ct_g0inq_data_t *)inv, 0);
		which = "tape";
		sprintf(extra=scsi_dev_loc_str, " type %d", scsitype);

		/*
		 * Check the type 1 registration to determine whether
		 * this device needs to be attached to a 3rd party
		 * driver. If not, then by default we'll attach it
		 * to the tape driver (tpsc.c).
		 */
		if (driver_info) {
			/*
		 	 * Mark this device as a tape
		 	 */
			device_driver_admin_info_set(driver_info->sd_driver_prefix, TAPE_IDENT, TAPE_IDENT);
			known_vhdl = scsi_device_register(lun_vhdl, inv, driver_info->sd_pathname_prefix);
			if (!scsi_device_is_attached(known_vhdl))
				scsi_device_attach(known_vhdl, driver_info->sd_driver_prefix);
		}	
		else {
			/* 
			 * add the vertex corresponding to the new
			 * scsi tape to the hardware graph 
			 */
			known_vhdl = scsi_device_register(lun_vhdl, inv, EDGE_LBL_TAPE);
			if (hwgraph_traverse(known_vhdl,
					     TPSC_DEFAULT"/"EDGE_LBL_CHAR,
					     &tmp_vhdl) != GRAPH_SUCCESS) {
				scsi_device_attach(known_vhdl, TPSC_PREFIX);
				inv_vhdl = known_vhdl;
				inv_class = INV_TAPE;
				inv_type = INV_SCSIQIC;
				inv_ctlr = eadap;
				inv_unit = targ;
				inv_state = scsitype;
			} else
				hwgraph_vertex_unref(tmp_vhdl);
		}
		break;

cdrom:
	case 5:	/* CDROM */
		which = "CDROM";
		extra = "";
		scsitype = (inv[1] & INV_REMOVE) | (inv[2] & INV_SCSI_MASK);

		/*
		 * Check the type 5 registration to determine whether
		 * this device needs to be attached to a 3rd party
		 * driver. If not, then by default we'll attach it
		 * to the disk driver (dksc.c).
		 */
		if (driver_info) {
			known_vhdl = scsi_device_register(lun_vhdl, inv, driver_info->sd_pathname_prefix);
			if (!scsi_device_is_attached(known_vhdl))
				scsi_device_attach(known_vhdl, driver_info->sd_driver_prefix);
		}
		else {
			/* 
			 * add the vertex corresponding to the new scsi disk to
			 * the hardware graph 
			 */
			disk_vhdl = scsi_disk_update(lun_vhdl,disktype); 
			if (!scsi_find_inventory(disk_vhdl, INV_SCSI)) {
				inv_vhdl = disk_vhdl;
				inv_class = INV_SCSI;
				inv_type = INV_CDROM;
				inv_ctlr = eadap;
				inv_unit = targ;
				inv_state = scsitype | (lun<<8);
			}
		}
		break;

	case 7: /* Optical Disk */
		which = "optical ";
		extra = "disk";
		scsitype = (inv[1] & INV_REMOVE) | (inv[2] & INV_SCSI_MASK);

		/*
		 * Check the type 7 registration to determine whether
		 * this device needs to be attached to a 3rd party
		 * driver. If not, then by default we'll attach it
		 * to the disk driver (dksc.c).
		 */
		if (driver_info) {
			known_vhdl = scsi_device_register(lun_vhdl, inv, driver_info->sd_pathname_prefix);
			if (!scsi_device_is_attached(known_vhdl))
				scsi_device_attach(known_vhdl, driver_info->sd_driver_prefix);
		}
		else {	
			disk_vhdl =  scsi_disk_update(lun_vhdl,disktype);
			if (!scsi_find_inventory(disk_vhdl, INV_SCSI)) {
				inv_vhdl = disk_vhdl;
				inv_class = INV_SCSI;
				inv_type = INV_OPTICAL;
				inv_ctlr = eadap;
				inv_unit = targ;
				inv_state = scsitype | (lun<<8);
			}
		}
		break;

	default:
		scsitype = (inv[1] & INV_REMOVE) | (inv[2] & INV_SCSI_MASK);
		inv_vhdl = scsi_vhdl;
		inv_class = INV_SCSI;
		inv_type = inv[0] & 0x1f;
		inv_ctlr = eadap;
		inv_unit = targ;
		inv_state = scsitype | (lun<<8);
		sprintf(which=scsi_dev_loc_str, " device type %u", inv[0]&0x1f);
		extra = "";

		/*
		 * Check the registration to determine whether this
		 * device needs to be attached to a 3rd party
		 * driver. If not, don't attach to any particular
		 * driver.  
		 */
		if (driver_info) {
			known_vhdl = scsi_device_register(lun_vhdl, inv, driver_info->sd_pathname_prefix);
			if (!scsi_device_is_attached(known_vhdl))
				scsi_device_attach(known_vhdl, driver_info->sd_driver_prefix);
		}
		break;
	}

	if (inv_vhdl) {
		if (node != 0) {
			hwgraph_inventory_add(inv_vhdl, INV_FCNODE,
					      (uint32_t) (node >> 32), (uint32_t) node,
					      (uint32_t) (port >> 32), (uint32_t) port);
		}
		invrc = hwgraph_inventory_add(inv_vhdl, inv_class, inv_type, 
					      inv_ctlr, inv_unit, inv_state);
	}

	if (showconfig && (invrc != GRAPH_DUP)) {
	    if (lun)
		cmn_err(CE_CONT,
			"%v: SCSI %s%s (targ = %d,%d)\n", 
			lun_vhdl,
			which,
			extra ? extra : "", 
			targ, lun);
	    else
		cmn_err(CE_CONT,
			"%v: SCSI %s%s (targ = %d)\n", 
			lun_vhdl,
			which,
			extra ? extra : "", 
			targ);
	}
}

/*
 * create a "bus" vertex under scsi_ctlr/#/ in the hardware graph.
 * used by the scsiha driver
 */
void
scsi_bus_create(vertex_hdl_t	ctlr_vhdl)
{
	/*REFERENCED*/
	graph_error_t		rv;
	vertex_hdl_t		bus_vhdl;

	rv = hwgraph_char_device_add(ctlr_vhdl, EDGE_LBL_BUS,
				     "scsiha_",&bus_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);
	
	scsi_ctlr_info_put(bus_vhdl,scsi_ctlr_info_get(ctlr_vhdl));
	
}

/*
 * given the controller vertex handle , target & the lun get the 
 * lun vertex handle
 */
vertex_hdl_t
scsi_lun_vhdl_get(vertex_hdl_t 	ctlr_vhdl,	
		  targ_t	targ,
		  lun_t		lun)
{	
	char		loc_str[LOC_STR_MAX_LEN];
	vertex_hdl_t	lun_vhdl;

	/*
	 * traverse the target/#/lun/# to get to the
	 * appropriate device vertex from the controller
	 * vertex
	 */
	sprintf(loc_str,TARG_LUN_FORMAT,targ,lun);
	if (hwgraph_traverse(ctlr_vhdl,
			     loc_str,
			     &lun_vhdl) == GRAPH_SUCCESS) {
		hwgraph_vertex_unref(lun_vhdl);
		return lun_vhdl;
	}
	return GRAPH_VERTEX_NONE;

}

/*
 * given the controller vertex handle, FC node & port, and the lun,
 * get the lun vertex handle
 */
vertex_hdl_t
scsi_fabric_lun_vhdl_get(vertex_hdl_t ctlr_vhdl, uint64_t node, uint64_t port, lun_t lun)
{	
	char		loc_str[LOC_STR_MAX_LEN];
	vertex_hdl_t	lun_vhdl;

	/*
	 * Traverse node/#/port/#/lun/# to get to the appropriate device
	 * vertex from the controller vertex.
	 */
	sprintf(loc_str, "node/%llx/port/%llx/lun/%d", node, port, lun);
	if (hwgraph_traverse(ctlr_vhdl, loc_str, &lun_vhdl) == GRAPH_SUCCESS) {
		hwgraph_vertex_unref(lun_vhdl);
		return lun_vhdl;
	}
	return GRAPH_VERTEX_NONE;
}

/*
 * Get the driver prefix this device is to be attached to. If zero is
 * returned, no explicit driver has been registered 
 */
scsi_driver_reg_t
scsi_device_driver_info_get(u_char *inq)
{
	int			scsi_type = inq[0] & 0x1f;
	scsi_driver_reg_t	sdr;
	char			*vid = (char *)inq+8;
	char                    *pid = (char *)inq+16;
	int			i;

	/*
	 * Find registration table for particular SCSI type
	 */
	for (i = 0; i < scsi_type && !scsi_drivers[i].st_last; ++i)
		;
	if (scsi_drivers[i].st_sdr == NULL)
		return(NULL);

	/*
	 * Find the driver info matching the inquiry string of the
	 * particular device.  
	 */
	sdr = scsi_drivers[scsi_type].st_sdr;
	for (i = 0; sdr[i].sd_vendor_id; ++i)
	{
		if ((strncmp(vid, sdr[i].sd_vendor_id, strlen(sdr[i].sd_vendor_id)) == 0) &&
		    (strncmp(pid, sdr[i].sd_product_id, strlen(sdr[i].sd_product_id)) == 0)) 
		{
			return(&sdr[i]);
		}
	}

	/*
	 * The last entry in the table is considered a
	 * wild-card. Attach the device to the that driver, if a driver
	 * is registered.
	 */
	if (sdr[i].sd_driver_prefix != NULL)
	{
		return(&sdr[i]);
	}

	return(NULL);
}

/*
 * We determine whether a device is already attached by checking
 * whether any part of its namespace is populated, i.e. edges
 * emminating from the vertex.
 */
int
scsi_device_is_attached(vertex_hdl_t vhdl)
{
	graph_edge_place_t	place = EDGE_PLACE_WANT_REAL_EDGES;
	int			rv;
	
        rv = hwgraph_edge_get_next(vhdl, NULL, NULL, &place);
	return((rv == GRAPH_HIT_LIMIT) ? 0 : 1);
}

/* Add a scsi lun device to the hwgraph
 *		ctlr_vhdl --> target/<targ>/lun/<lun>
 */
vertex_hdl_t
scsi_device_add(vertex_hdl_t ctlr_vhdl, targ_t targ, lun_t lun)
{
	vertex_hdl_t	targ_vhdl,lun_vhdl;

	/*
	 * add the target vertex
	 */
	targ_vhdl = scsi_targ_vertex_add(ctlr_vhdl,targ);
	/*
	 * add the lun vertex
	 */

	lun_vhdl = scsi_lun_vertex_add(targ_vhdl,lun);
	
	return(lun_vhdl);
}

/*
 * Remove the lun vertex for ctlr_vhdl --> target/<targ>/lun/<lun>
 * If <lun> is the last lun then "lun" directory gets removed.
 * If <targ> is the last target and the "lun" directory got removed
 * the "targ" directory gets removed.
 */
void
scsi_device_remove(vertex_hdl_t ctlr_vhdl,targ_t targ,lun_t lun)
{
	vertex_hdl_t	targ_vhdl;
	graph_error_t	rv;
	char		loc_str[LOC_STR_MAX_LEN];

	/* Get the target vertex handle */
 	sprintf(loc_str ,TARG_NUM_FORMAT,targ);
	rv = hwgraph_traverse(ctlr_vhdl,loc_str,&targ_vhdl);
	ASSERT(rv == GRAPH_SUCCESS);rv = rv;
	hwgraph_vertex_unref(targ_vhdl);

	/* Remove the lun vertex */
	if (scsi_lun_vertex_remove(targ_vhdl,lun) == GRAPH_VERTEX_NONE)
		/* If this was the last lun vertex remove the 
		 * target vertex.
		 */
		(void)scsi_targ_vertex_remove(ctlr_vhdl,targ);

}

uint64_t
fc_short_portname(uint64_t node, uint64_t port)
{
	/*
	 * If the Node name format is IEEE extended (type 2) and the
	 * port name is the node name with port number (sanity check, should
	 * always be the case), then we can use a short port name: the
	 * part of the port name that differs from the node name.
	 */
	switch (node >> 60)
	{
	case 1:
	case 2:
	    if (node == (port & ~((uint64_t) 0xFFF << 48)))
		return ((port >> 56) & 0xF) | ((port >> 44) & 0xFF0);
	    break;
	case 5:
	    if (node == (port & ~((uint64_t) 0x3 << 22)))
		return (port >> 22) & 0x3;
	    break;
	}
	return port;
}
