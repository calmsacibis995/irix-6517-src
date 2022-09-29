/*
 * Copyright 1996, Silicon Graphics, Inc.
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


/*
 * hubspc.c - Hub Memory Space Management Driver
 * This driver implements the managers for the following
 * memory resources:
 * 1) reference counters
 */

#if SN0

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/immu.h>		/* VM Specific stuff 	*/
#include <sys/conf.h>		/* D_MP 		*/
#include <sys/cred.h>
#include <sys/debug.h>		/* Assert,  etc 	*/
#include <sys/page.h>		/* cnodeid().. 		*/
#include <sys/pda.h>		/* cnodeid().. 		*/
#include <sys/errno.h>		/* Error number values	*/
#include <sys/sysmacros.h>	/* btoc & friends	*/
#include <sys/capability.h>	/* _CAP_ Stuff 		*/
#include <sys/kmem.h>		/* kmem_ stuff		*/
#include <ksys/ddmap.h>		/* vhandl ..		*/
#include <sys/ddi.h>		/* Device driver intfc	*/
#include <sys/cmn_err.h>	/* If cmn_err is needed	*/
#include <sys/atomic_ops.h>	/* Atomic ops		*/
#include <sys/SN/agent.h>	/* SN0 Address		*/
#include <sys/SN/addrs.h>	/* SN0 Address		*/
#include <sys/SN/SN0/hubdev.h>   /* Hub Device Registration */
#include <sys/SN/SN0/ip27config.h> 
#include <sys/hubspc.h>
#include <sys/iograph.h>
#include <sys/invent.h>
#include <sys/hwgraph.h>
#include <ksys/mem_refcnt.h>

/* Uncomment the following line for tracing */
/* #define HUBSPC_DEBUG 1 */

int hubspc_devflag = D_MP;


/***********************************************************************/
/* CPU Prom Space 						       */
/***********************************************************************/

typedef struct cpuprom_info {
	dev_t	prom_dev;
	dev_t	nodevrtx;
	struct	cpuprom_info *next;
}cpuprom_info_t;

static cpuprom_info_t	*cpuprom_head;
lock_t	cpuprom_spinlock;
#define	PROM_LOCK()	mutex_spinlock(&cpuprom_spinlock)
#define	PROM_UNLOCK(s)	mutex_spinunlock(&cpuprom_spinlock, (s))

/*
 * Add prominfo to the linked list maintained.
 */
void
prominfo_add(dev_t hub, dev_t prom)
{
	cpuprom_info_t	*info;
	int	s;

	info = kmem_alloc(sizeof(cpuprom_info_t), KM_SLEEP);
	ASSERT(info);
	info->prom_dev = prom;
	info->nodevrtx = hub;


	s = PROM_LOCK();
	info->next = cpuprom_head;
	cpuprom_head = info;
	PROM_UNLOCK(s);
}

void
prominfo_del(dev_t prom)
{
	int	s;
	cpuprom_info_t	*info;
	cpuprom_info_t	**prev;

	s = PROM_LOCK();
	prev = &cpuprom_head;
	while (info = *prev) {
		if (info->prom_dev == prom) {
			*prev = info->next;
			PROM_UNLOCK(s);
			return;
		}
		
		prev = &info->next;
	}
	PROM_UNLOCK(s);
	ASSERT(0);
}

dev_t
prominfo_nodeget(dev_t prom)
{
	int	s;
	cpuprom_info_t	*info;

	s = PROM_LOCK();
	info = cpuprom_head;
	while (info) {
		if(info->prom_dev == prom) {
			PROM_UNLOCK(s);
			return info->nodevrtx;
		}
		info = info->next;
	}
	return 0;
}

/* Add "detailed" labelled inventory information to the
 * prom vertex 
 */
void
cpuprom_detailed_inventory_info_add(dev_t prom_dev,dev_t node)
{
	invent_miscinfo_t 	*cpuprom_inventory_info;
	extern invent_generic_t *klhwg_invent_alloc(cnodeid_t cnode, 
						     int class, int size);
	cnodeid_t		cnode = hubdev_cnodeid_get(node);

	/* Allocate memory for the extra inventory information
	 * for the  prom
	 */
	cpuprom_inventory_info = (invent_miscinfo_t *) 
		klhwg_invent_alloc(cnode, INV_PROM, sizeof(invent_miscinfo_t));

	ASSERT(cpuprom_inventory_info);

	/* Set the enabled flag so that the hinv interprets this
	 * information
	 */
	cpuprom_inventory_info->im_gen.ig_flag = INVENT_ENABLED;
	cpuprom_inventory_info->im_type = INV_IP27PROM;
	/* Store prom revision into inventory information */
	cpuprom_inventory_info->im_rev = IP27CONFIG.pvers_rev;
	cpuprom_inventory_info->im_version = IP27CONFIG.pvers_vers;


	/* Store this info as labelled information hanging off the
	 * prom device vertex
	 */
	hwgraph_info_add_LBL(prom_dev, INFO_LBL_DETAIL_INVENT, 
			     (arbitrary_info_t) cpuprom_inventory_info);
	/* Export this information so that user programs can get to
	 * this by using attr_get()
	 */
        hwgraph_info_export_LBL(prom_dev, INFO_LBL_DETAIL_INVENT,
				sizeof(invent_miscinfo_t));
}
int
cpuprom_attach(dev_t node)
{
        vertex_hdl_t prom_dev;
        
        hwgraph_char_device_add(node, EDGE_LBL_PROM, "hubspc_", &prom_dev);
#ifdef	HUBSPC_DEBUG
	printf("hubspc: prom_attach hub: 0x%x prom: 0x%x\n", node, prom_dev);
#endif	/* HUBSPC_DEBUG */
	device_inventory_add(prom_dev, INV_PROM, INV_IP27PROM, NULL, NULL, NULL);
	/* Add additional inventory info about the cpu prom like
	 * revision & version numbers etc.
	 */
	cpuprom_detailed_inventory_info_add(prom_dev,node);
        device_info_set(prom_dev, (void*)(ulong)HUBSPC_PROM);
	prominfo_add(node, prom_dev);

        return (0);
}

/*ARGSUSED*/
int
cpuprom_map(dev_t dev, vhandl_t *vt, off_t addr, size_t len)
{
        int 		errcode;
	caddr_t 	kvaddr;
	dev_t		node;
	cnodeid_t 	cnode;

	node = prominfo_nodeget(dev);

	if (!node)
		return EIO;
        

	kvaddr = hubdev_prombase_get(node);
	cnode  = hubdev_cnodeid_get(node);
#ifdef	HUBSPC_DEBUG
	printf("cpuprom_map: hubnode %d kvaddr 0x%x\n", node, kvaddr);
#endif

	if (len > RBOOT_SIZE)
		len = RBOOT_SIZE;
        /*
         * Map in the prom space
         */
	errcode = v_mapphys(vt, kvaddr, len);

	if (errcode == 0 ){
		/*
		 * Set the MD configuration registers suitably.
		 */
		nasid_t		nasid;
		__uint64_t	value;
		volatile hubreg_t	*regaddr;

		nasid = COMPACT_TO_NASID_NODEID(cnode);
		regaddr = REMOTE_HUB_ADDR(nasid, MD_MEMORY_CONFIG);
		value = HUB_L(regaddr);
		value &= ~(MMC_FPROM_CYC_MASK | MMC_FPROM_WR_MASK);

		if (private.p_sn00) {
			value |= ((0xfL << MMC_FPROM_CYC_SHFT) | 
				  (0x4L << MMC_FPROM_WR_SHFT));
		}
		else {
			value |= ((0x08L << MMC_FPROM_CYC_SHFT) | 
				  (0x01L << MMC_FPROM_WR_SHFT));
		}
		HUB_S(regaddr, value);

	}
        return (errcode);
}

/*ARGSUSED*/
int
cpuprom_unmap(dev_t dev, vhandl_t *vt)
{
        return 0;
}

/***********************************************************************/
/* Base Hub Space Driver                                               */
/***********************************************************************/

/*
 * hubspc_init
 * Registration of the hubspc devices with the hub manager
 */
void
hubspc_init(void)
{
        /*
         * Register with the hub manager
         */

        /* The reference counters */
        hubdev_register(mem_refcnt_attach);

	/* Prom space */
	hubdev_register(cpuprom_attach);
	
#ifdef	HUBSPC_DEBUG
	printf("hubspc_init: Completed\n");
#endif	/* HUBSPC_DEBUG */
	/* Initialize spinlocks */
	spinlock_init(&cpuprom_spinlock, "promlist");
}

/* ARGSUSED */
int
hubspc_open(dev_t *devp, mode_t oflag, int otyp, cred_t *crp)
{
        int errcode = 0;
        
        switch ((hubspc_subdevice_t)(ulong)device_info_get(*devp)) {
        case HUBSPC_REFCOUNTERS:
                errcode = mem_refcnt_open(devp, oflag, otyp, crp);
                break;

        case HUBSPC_PROM:
		/* Check if the user has proper access rights to 
		 * read/write the prom space.
		 */
                if (!cap_able(CAP_DEVICE_MGT)) {
                        errcode = EPERM;
                }                
                break;

        default:
                errcode = ENODEV;
        }

#ifdef	HUBSPC_DEBUG
	printf("hubspc_open: Completed open for type %d\n",
               (hubspc_subdevice_t)(ulong)device_info_get(*devp));
#endif	/* HUBSPC_DEBUG */

        return (errcode);
}


/* ARGSUSED */
int
hubspc_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
        int errcode = 0;
        
        switch ((hubspc_subdevice_t)(ulong)device_info_get(dev)) {
        case HUBSPC_REFCOUNTERS:
                errcode = mem_refcnt_close(dev, oflag, otyp, crp);
                break;

        case HUBSPC_PROM:
                break;
        default:
                errcode = ENODEV;
        }

#ifdef	HUBSPC_DEBUG
	printf("hubspc_close: Completed close for type %d\n",
               (hubspc_subdevice_t)(ulong)device_info_get(dev));
#endif	/* HUBSPC_DEBUG */

        return (errcode);
}

/* ARGSUSED */
hubspc_map(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
	/*REFERENCED*/
        hubspc_subdevice_t subdevice;
        int errcode = 0;

	/* check validity of request */
	if( len == 0 ) {
		return ENXIO;
        }

        subdevice = (hubspc_subdevice_t)(ulong)device_info_get(dev);

#ifdef	HUBSPC_DEBUG
	printf("hubspc_map: subdevice: %d vaddr: 0x%x phyaddr: 0x%x len: 0x%x\n",
	       subdevice, v_getaddr(vt), off, len);
#endif /* HUBSPC_DEBUG */

        switch ((hubspc_subdevice_t)(ulong)device_info_get(dev)) {
        case HUBSPC_REFCOUNTERS:
                errcode = mem_refcnt_mmap(dev, vt, off, len, prot);
                break;

        case HUBSPC_PROM:
		errcode = cpuprom_map(dev, vt, off, len);
                break;
        default:
                errcode = ENODEV;
        }

#ifdef	HUBSPC_DEBUG
	printf("hubspc_map finished: spctype: %d vaddr: 0x%x len: 0x%x\n",
	       (hubspc_subdevice_t)(ulong)device_info_get(dev), v_getaddr(vt), len);
#endif /* HUBSPC_DEBUG */

	return errcode;
}

/* ARGSUSED */
hubspc_unmap(dev_t dev, vhandl_t *vt)
{
        int errcode = 0;
        
        switch ((hubspc_subdevice_t)(ulong)device_info_get(dev)) {
        case HUBSPC_REFCOUNTERS:
                errcode = mem_refcnt_unmap(dev, vt);
                break;

        case HUBSPC_PROM:
                errcode = cpuprom_unmap(dev, vt);
                break;

        default:
                errcode = ENODEV;
        }
	return errcode;

}

/* ARGSUSED */
int
hubspc_ioctl(dev_t dev,
             int cmd,
             void *arg,
             int mode,
             cred_t *cred_p,
             int *rvalp)
{
        int errcode = 0;
        
        switch ((hubspc_subdevice_t)(ulong)device_info_get(dev)) {
        case HUBSPC_REFCOUNTERS:
                errcode = mem_refcnt_ioctl(dev, cmd, arg, mode, cred_p, rvalp);
                break;

        case HUBSPC_PROM:
                break;

        default:
                errcode = ENODEV;
        }
	return errcode;

}


        
#endif /* SN0 */

