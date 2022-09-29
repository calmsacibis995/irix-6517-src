/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990-1993, Silicon Graphics, Inc           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "io/usrpci.c: $Revision: 1.23 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/cred.h>
#include <ksys/ddmap.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/pio.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/ddi.h>
#include <sys/kabi.h>
#include <sys/signal.h>
#include <sys/kthread.h>
#include <sys/uthread.h>

#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/usrpci.h>
#include <sys/PCI/bridge.h>

#include <sys/PCI/pcibr.h>

#include <sys/PCI/pcibr_private.h>

#if ULI
#include <sys/uli.h>
#include <ksys/uli.h>
#endif

#define	NEW(ptr)	(ptr = kmem_alloc(sizeof (*(ptr)), KM_SLEEP))
#define	DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

int	usrpci_devflag = D_MP;


int	usrpci_debug = 0;

#define	usrpci_printf(x)	if (usrpci_debug) printf x

#define	EDGE_LBL_USRPCI		"usrpci"
#define	EDGE_USRPCI_MEM		"mem"
#define	EDGE_USRPCI_MEM32	"mem32"
#define	EDGE_USRPCI_MEM64	"mem64"
#define	EDGE_USRPCI_IO		"io"
#define	EDGE_USRPCI_CONFIG	"config"
#define	EDGE_USRPCI_DEFAULT	"default"
#define EDGE_USRPCI_BAR         "base"

char *usrpci_spacename[] = 
	{ "None", "", EDGE_USRPCI_IO, "", EDGE_USRPCI_MEM,
	   EDGE_USRPCI_MEM32, EDGE_USRPCI_MEM64, EDGE_USRPCI_CONFIG ,
	   EDGE_USRPCI_DEFAULT};

/*
 * Per vertex data structure.
 * Data structure attached to pcibus/<slot>/usrpci vertex of each PCI slot.
 * All the minor devices created under the usrpci vertex will be 
 * associated with this data structure. 
 */
typedef struct usrpci_drvinfo_s {
	vertex_hdl_t	vertex;		/* back pointer to Vertex 	*/
	vertex_hdl_t	mem32_cv;	/* Memory space char dev vertex */
	vertex_hdl_t	mem64_cv;	/* Memory space char dev vertex */
	vertex_hdl_t	io_cv;		/* IO space char dev vertex	*/
	vertex_hdl_t	cfg_cv;		/* Config space vertex		*/
	vertex_hdl_t	default_cv;	/* Defaults to what this slot has */
	vertex_hdl_t    bars[PCI_CFG_BASE_ADDRS];  /* Base addr reg vertices */
	vertex_hdl_t	connectpt;	/* Where we are connected to	*/
	int		slot;		/* PCI Bus slot			*/
} *usrpci_drvinfo_t;

/*
 * Per slot data structure for keeping track of the  mapping done by user 
 * level PCI drivers. This will be used to provide error recovery.
 */

typedef struct usrpci_usrinfo_s {
	struct usrpci_usrinfo_s	*next;
	__psunsigned_t		handle;	
	uthread_t		*uthread; /* uthread, for sigtouthread */
	pciio_piomap_t		piomap;	/* PCI PIO map if allocated	*/
	caddr_t			vaddr;	/* User virtual address 	*/
	caddr_t			kaddr;	/* Kernel address of mapping	*/
	off_t			start;	/* Start offset within PCI space */
	size_t			size;	/* Size in PCI Space		*/
	iopaddr_t		pciaddr;/* PCI Address if allocated 	*/
	pciio_space_t		space;	/* PCI space used for mapping	*/
	char			pio_alloc; /* PIO Space allocated	*/
} *usrpci_usrinfo_t;

/*
 * Lock to serialize various usrpci operations.
 */
lock_t			usrpci_lock;
#define	usrpci_lockslot(slot)		mutex_spinlock(&usrpci_lock)
#define	usrpci_unlockslot(slot, s)	mutex_spinunlock(&usrpci_lock, s)

#define	MAX_PCISLOT	8
/*
 * Linked list of mapping done for each slot.
 * Slot number (MAX_PCISLOT) corresponds to the
 * list of users who map in non-slot specific space.
 */
usrpci_usrinfo_t	usrpci_maplist[MAX_PCISLOT+1]; 	/* list of mapping */

#define	USRPCI_PREFIX		"usrpci_"

#define	USRPCI_SLOT		1

/* =====================================================================
 *		FUNCTION TABLE OF CONTENTS
 */

extern void	usrpci_init(void);

extern int	usrpci_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
extern int	usrpci_close(dev_t dev, int oflag, int otyp, cred_t *crp);

extern int	usrpci_ioctl(dev_t dev, int cmd, void *arg,
			    int mode, cred_t *crp, int *rvalp);

extern int	usrpci_map(dev_t dev, vhandl_t *vt,
			  off_t off, size_t len, uint_t prot);
extern int	usrpci_unmap(dev_t dev, vhandl_t *vt);

extern int	pciio_slot_inuse(vertex_hdl_t);

static pciio_space_t 	usrpci_space(vertex_hdl_t, usrpci_drvinfo_t, int *);
static usrpci_drvinfo_t usrpci_info_get(vertex_hdl_t);

/* =====================================================================
 *			Driver Initialization
 */

/* 
 *	usrpci_init: called once during system startup or
 *	when a loadable driver is loaded.
 */
void
usrpci_init(void)
{
	spinlock_init(&usrpci_lock, "usrpci");

}

/*
 *	usrpci_open: called when a user level program tries to open
 *	any path specific to usrpci interface.
 */

/* ARGSUSED */
int
usrpci_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
	vertex_hdl_t		vhdl = dev_to_vhdl(*devp);
	usrpci_drvinfo_t	usrpci_info = usrpci_info_get(vhdl);
	pciio_space_t		type;
	int                     bar;

	if (!usrpci_info){
		usrpci_printf(("usrpci_open: usrpci_info not found for 0x%x\n",
					vhdl));
		return ENODEV;
	}

	type = usrpci_space(vhdl, usrpci_info, &bar);
	if (type == PCIIO_SPACE_NONE) {
		usrpci_printf(("usrpci_open: Invald Space type %d to open\n",
				type));
		return EIO;
	}

	usrpci_printf(("usrpci_open: %s info 0x%x\n", 
			usrpci_spacename[type], usrpci_info));

	if (!pciio_slot_inuse(usrpci_info->connectpt))
		return EIO;

	return 0;

}

/*
 *	usrpci_close: called when a device special file
 *	is closed by a process and no other processes
 *	still have it open ("last close").
 */

/* ARGSUSED */
int
usrpci_close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
	vertex_hdl_t		vhdl = dev_to_vhdl(dev);
	/*REFERENCED*/
	usrpci_drvinfo_t	usrpci_info = usrpci_info_get(vhdl);

	ASSERT(usrpci_info->vertex == vhdl);
	return 0;
}

/*
 *	usrpci_ioctl: a user has made an ioctl request
 *	for an open character device.
 *
 *	cmd and arg are as specified by the user; arg is
 *	probably a pointer to something in the user's
 *	address space, so you need to use copyin() to
 *	read through it and copyout() to write through it.
 */

#if ULI
static void
pciuli_teardown(struct uli *uli)
{
    pciio_intr_disconnect((pciio_intr_t)uli->teardownarg1);
    pciio_intr_free((pciio_intr_t)uli->teardownarg1);
}

static void
pciuli_intr(intr_arg_t arg)
{
    int ulinum = (int)(__psint_t)arg;

    if (ulinum >= 0 && ulinum < MAX_ULIS)
	uli_callup(ulinum);
}

#endif

/* ARGSUSED */
int
usrpci_ioctl(dev_t dev, int cmd, void *arg,
	    int mode, cred_t *crp, int *rvalp)
{
    switch(cmd) {
#if ULI
      case UPIOCREGISTERULI: {
	  vertex_hdl_t vhdl = dev_to_vhdl(dev);
	  usrpci_drvinfo_t usrpci_info = usrpci_info_get(vhdl);
	  int err;
	  struct uliargs args;
	  struct uli *uli;
	  device_desc_t desc;
	  pciio_intr_t intr;
	  cpuid_t intrcpu;
	  
	  if (copyin(arg, (void*)&args, sizeof(args)))
	      return(EFAULT);

	  desc = device_desc_dup(usrpci_info->connectpt);
	  device_desc_flags_set(desc, (device_desc_flags_get(desc) |
				       D_INTR_NOTHREAD));
	  device_desc_intr_swlevel_set(desc, INTR_SWLEVEL_NOTHREAD_DEFAULT);
	  device_desc_intr_name_set(desc, "PCIULI");
	  device_desc_default_set(usrpci_info->connectpt, desc);

	  intr = pciio_intr_alloc(usrpci_info->connectpt,
				  desc,
				  PCIIO_INTR_LINE_A,
				  usrpci_info->connectpt);

	  if (intr == 0)
	      return(EINVAL);

	  intrcpu = cpuvertex_to_cpuid(pciio_intr_cpu_get(intr));

	  if (err = new_uli(&args, &uli, intrcpu)) {
	      pciio_intr_free(intr);
	      return(err);
	  }

	  uli->teardownarg1 = (__psint_t)intr;

	  pciio_intr_connect(intr,
			     pciuli_intr,
			     (intr_arg_t)(__psint_t)uli->index,
			     (void*)0);

	  /* don't set teardown function until intr is set so we don't
	   * try to disconnect an interrupt which isn't connected
	   */
	  uli->teardown = pciuli_teardown;

	  args.id = uli->index;
	  if (copyout((void*)&args, arg, sizeof(args))) {
	      free_uli(uli);
	      return(EFAULT);
	  }
	  return(0);
      }
#endif
      default:
	*rvalp = -1;
	return EIO;	/* TeleType® is a registered trademark. */
    }
}

/* ARGSUSED */
int
usrpci_map(dev_t dev, vhandl_t *vt,
	  off_t off, size_t len, uint_t prot)
{
	vertex_hdl_t		vhdl = dev_to_vhdl(dev);
	usrpci_drvinfo_t	usrpci_info = usrpci_info_get(vhdl);
	caddr_t			kaddr;
	pciio_space_t		type;
	int			error;
	int			piosp_alloc_done = 0;
	usrpci_usrinfo_t	info;
	iopaddr_t		pciaddr = 0;
	pciio_piomap_t		piomap  = 0;
	int                     bar = -1;


	ASSERT(usrpci_info);

	type = usrpci_space(vhdl, usrpci_info, &bar);

	if (type == PCIIO_SPACE_NONE)	
		return EINVAL;

	/* v_getlen is a multiple of the page size which 
	 * is always greater than the device config space
	 * size. This causes pciio_piomap calls to fail
	 * for accesses to config space. So this new
	 * check has been added to prevent the above 
	 * from happening.
	 */
	if (type != PCIIO_SPACE_CFG)
		len =  v_getlen(vt) > len ? v_getlen(vt) : len;

	if (len == 0)
		return EINVAL;

	switch(type) {

	case PCIIO_SPACE_CFG:
	case PCIIO_SPACE_WIN0:

		/*
		 * We now know the PCI slot and the type of 
		 * mapping needed by user. 
		 *
		 * Find out the address at which the device in 
		 * this slot is mapped, and request a mapping 
		 * for the same address.
		 */

		ASSERT(pciio_slot_inuse(usrpci_info->connectpt));
		kaddr = (caddr_t) pciio_piotrans_addr(
						      usrpci_info->connectpt, 0, type, off, len, 0);
		/*
		 * If kaddr is not page aligned, prime it, and
		 * bump the length of mapping.
		 */
		if (poff(kaddr)) {
			caddr_t	kaddr_aligned;
			kaddr_aligned = kaddr - poff(kaddr);
			len += (kaddr - kaddr_aligned);
			kaddr = kaddr_aligned;
		}
		pciaddr = (caddr_t)pciio_piotrans_addr(
						       usrpci_info->connectpt, 0, type, 0, 1, 0) - kaddr;
		break;

	case PCIIO_SPACE_IO:
		if (off == 0) {
			int win;
			int slot;
			pciio_info_t pciio_info;
			pcibr_soft_t pcibr_soft;
			
			pciio_info = pciio_info_get(usrpci_info->connectpt);
			pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get
				(pciio_info);
			slot = usrpci_info->slot;

			pciaddr = 0;

			if (bar == -1) {
				for (win = 0; win < 6; win++) {
					if (pcibr_soft->bs_slot[slot].
					    bss_window[win].bssw_space == type) {
						if (btoc(pcibr_soft->bs_slot[slot].
					    bss_window[win].bssw_size) < 
					    btoc(len)) {
						return(ENXIO);
					}
					usrpci_printf(("using BAR #%d\n", 
						       win));
					pciaddr = pcibr_soft->bs_slot
						[slot].bss_window[win].
						bssw_base;
					break;
				}
			}
			}
			else {
				ASSERT(pcibr_soft->bs_slot[slot].
				       bss_window[bar].bssw_space == type);
				if (btoc(pcibr_soft->bs_slot[slot].
					 bss_window[bar].bssw_size) < 
				    btoc(len)) {
					return(ENXIO);
				}

				usrpci_printf(("using BAR #%d\n", 
					       win));
				pciaddr = pcibr_soft->bs_slot
					[slot].bss_window[bar].
					bssw_base;
			}

			if (pciaddr == 0) {
				return(EINVAL);
			}
		}
		else {
			pciaddr = (iopaddr_t)off;
		}
			
		piomap = pciio_piomap_alloc(
					    usrpci_info->connectpt, 0, type, 
					    pciaddr, len, len, PIOMAP_FIXED);
		kaddr = pciio_piomap_addr(piomap, pciaddr, len);
		usrpci_printf(("usrpci_map (IO): pciaddr 0x%x kvaddr 0x%x\n", 
			       pciaddr, kaddr));

		break;
	case PCIIO_SPACE_MEM:
	case PCIIO_SPACE_MEM32:
	case PCIIO_SPACE_MEM64:

		/* 
		 * mmap for the /hw/../pci/usrpci/<space_type> 
		 * In this mode, we need to go via pciio_piospace_alloc
		 * interface to get the required PCI space, and use
		 * that address to the pio translation.
		 */
		if (off == 0){
			int win;
			int slot;
			pciio_info_t pciio_info;
			pcibr_soft_t pcibr_soft;
			
			pciio_info = pciio_info_get(usrpci_info->connectpt);
			pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get
				(pciio_info);
									
			pciaddr = 0;

			/*
			  pciaddr = pciio_piospace_alloc(
			  usrpci_info->connectpt, 0, type, len, NBPP);
			  if (pciaddr == 0) 
			  return ENOSPC;
			  piosp_alloc_done = 1;
			  */


			slot = usrpci_info->slot;

			if (bar == -1) {
				for (win = 0; win < 6; win++) {
					if (pcibr_soft->bs_slot[slot].
					    bss_window[win].bssw_space == type) {
						if (btoc(pcibr_soft->bs_slot[slot].
							 bss_window[win].bssw_size) < 
						    btoc(len)) {
						return(ENXIO);
					}
					usrpci_printf(("using BAR #%d\n", 
						       win));
					pciaddr = pcibr_soft->bs_slot
						[slot].bss_window[win].
						bssw_base;
					break;
				}
			}
			}
			
			else {
				ASSERT(pcibr_soft->bs_slot[slot].
				       bss_window[bar].bssw_space == type);
				if (btoc(pcibr_soft->bs_slot[slot].
					 bss_window[bar].bssw_size) < 
				    btoc(len)) {
					return(ENXIO);
				}
				pciaddr = pcibr_soft->bs_slot
					[slot].bss_window[bar].
					bssw_base;
			}


			if (pciaddr == 0) {
				return(EINVAL);
			}
			
		} else {
			/* XXX In this case, we should not be doing 
			 * pciio_piospace_free. got to keep track of
			 * this situation..
			 */
			pciaddr = (iopaddr_t)off;
		}
		usrpci_printf(("usrpci_map: pciaddr : 0x%x\n", pciaddr));
		piomap = pciio_piomap_alloc(
					    usrpci_info->connectpt, 0, type, 
					    pciaddr, len, len, PIOMAP_FIXED);
		if (!piomap && piosp_alloc_done) {
			pciio_piospace_free(
					    usrpci_info->connectpt, type, pciaddr, len);
			return ENOSPC;
		}
		kaddr = pciio_piomap_addr(piomap, pciaddr, len);
		usrpci_printf(("usrpci_map: piomap_addr : 0x%x\n", kaddr));

		ASSERT(kaddr);
		break;

	default:
		return EINVAL;
	}

	/*
	 * This is an explicit check. Yeah, we know that PCI bus 
	 * address 0 cannot be used.. 
	 */
	if (kaddr == NULL)
		return EINVAL;

	error = v_mapphys(vt, kaddr, len);

	if (!error) {
		int	s;
		/*
		 * Mmap done. Note down relavent information
		 * and add this info to per slot list.
		 */
		NEW(info);

		info->uthread = curuthread;
		info->handle = v_gethandle(vt);
		info->space  = type;
		info->vaddr  = v_getaddr(vt);
		info->kaddr  = kaddr;
		info->pciaddr= pciaddr;
		info->start  = off;
		info->size   = len;
		info->piomap = piomap;
		info->pio_alloc = piosp_alloc_done;

		/*
		 * tack it to to start of list for this slot.
		 */
		s = usrpci_lockslot(usrpci_info->slot);
		if (usrpci_maplist[usrpci_info->slot]) {
			info->next = usrpci_maplist[usrpci_info->slot];
		} else 	info->next = 0;
		usrpci_maplist[usrpci_info->slot] = info;
		usrpci_unlockslot(usrpci_info->slot, s);
		usrpci_printf(
			("usrpci_map: Proc %s Virtual: 0x%x Kvaddr 0x%x\n",
			get_current_name(), v_getaddr(vt), kaddr));
	} else {
		if (piomap) {
			pciio_piomap_done(piomap);
			pciio_piomap_free(piomap);
			ASSERT(pciaddr);
			if (piosp_alloc_done)
				pciio_piospace_free(vhdl, type, pciaddr, len);
		}
	}

	return error;
}

int
usrpci_unmap(dev_t dev, vhandl_t *vt)
{
	vertex_hdl_t		vhdl = dev_to_vhdl(dev);
	usrpci_drvinfo_t	usrpci_info = usrpci_info_get(vhdl);
	usrpci_usrinfo_t	info;
	usrpci_usrinfo_t	*previnfo; 
	int			s;
	pciio_space_t		type;
	int                     bar;

	/*
	 * Look up the per slot list for the info inserted for this device
	 * and free it.
	 */
	
	type = usrpci_space(vhdl, usrpci_info, &bar);

	ASSERT(type != PCIIO_SPACE_NONE);

	s = usrpci_lockslot(usrpci_info->slot);

	previnfo = &usrpci_maplist[usrpci_info->slot];
	while (info = *previnfo) {
		if (info->handle == v_gethandle(vt)){
			break;
		}
		previnfo = &info->next;
	}

	if (!info) {
		usrpci_printf(
		("usrpci_unmap: No info structure for vertex 0x%x vaddr 0x%x\n",
				vhdl, v_getaddr(vt)));
		usrpci_unlockslot(usrpci_info->slot, s);
		return ENODEV;
	}

	/*
	 * Pull info out of the linked list.
	 */
	*previnfo = info->next;
	usrpci_unlockslot(usrpci_info->slot, s);

	/*
	 * Free up all the stuff that was allocated as part of the mapping.
	 */
	if (info->piomap) {
		pciio_piomap_done(info->piomap);
		pciio_piomap_free(info->piomap);
		ASSERT(info->pciaddr);
		if (info->pio_alloc) 
			pciio_piospace_free(usrpci_info->connectpt, type, 
				info->pciaddr, info->size);
	}

	DEL(info);
		
	return(0);
}

/*
 * Error handling support routines.
 */
/*
 * usrpci_sendsig
 *	Find out all the processes that map the address 'pciaddr'
 * 	in slot 'slot' and send SIGBUS to them
 *	For small window addresses,pciaddr is sufficient to findout
 * 	the slot to which the address belongs to. For large window
 *	addresses, we need explicit info.
 */
static int
usrpci_sendsig(pciio_slot_t slot, iopaddr_t pciaddr)
{
	usrpci_usrinfo_t	info;
	int			sigcount = 0;

	info = usrpci_maplist[slot];

	while (info) {
		if ((pciaddr >= info->pciaddr) && 
		    (pciaddr < (info->pciaddr + info->size))){

			sigtouthread(info->uthread, SIGBUS,
					(k_siginfo_t *)NULL);
			sigcount++;
		}
		info   = info->next;
	}
	return sigcount;
}

/*
 * usrpci_cfgspace_error
 *	Handle an error in the config space. 
 *	Handling PCI bus config space errors are trikcy, since the
 * 	config space for each PCI slot does not start on a page 
 * 	boundary for systems with 16k page size. 
 *	Config space for each slot is 4k, and if system page size is
 *	16k, 4 slots worth of config space is mapped to each users.
 *	In case of an error in the config space, we may need to 
 * 	send a SIGBUS to many processes.
 */

#define	CFGSPACE_PERPAGE	(NBPP/BRIDGE_CONFIG_SLOT_SIZE)
#define	PCIIO_CFGSPACE_ADDR(x)	\
		(((x) > BRIDGE_CONFIG_BASE) && ((x) < BRIDGE_CONFIG_END))

static int
usrpci_cfgspace_error(
	usrpci_drvinfo_t 	usrpci_info, 
	iopaddr_t 		pciaddr)
{
	int		slot;
	int		lastslot;
	int		sigcount = 0;

	ASSERT(PCIIO_CFGSPACE_ADDR(pciaddr));

	/* Get the slot number as per the PCI info. */
	slot = usrpci_info->slot;
	lastslot = slot + CFGSPACE_PERPAGE;

	for (; slot < lastslot; slot++) {
		if (usrpci_sendsig(slot, pciaddr))
			sigcount++;
	}

	return sigcount;
}

/*
 * User level PCI Error handler.
 *	This would be invoked by the upper level erorr handling routines
 *	to handle a PCI bus error.
 */
/*ARGSUSED*/
int
usrpci_error_handler(vertex_hdl_t vhdl, int error_code, iopaddr_t erraddr)
{
	usrpci_drvinfo_t	usrpci_info = usrpci_info_get(vhdl);
	int			slot;
	int			retval = IOERROR_INVALIDADDR;

#if DEBUG && ERROR_DEBUG
	cmn_err(CE_CONT, "%v: usrpci_error_handler\n", vhdl);
#endif

	ASSERT(usrpci_info);
	slot = usrpci_info->slot;

	ASSERT(error_code & IOECODE_PIO);

	/*
	 * In this situation, we need to figure out which is
	 * the process mapping address <erraddr> and send SIGBUS
	 * to that process. 
	 * There are a couple of problems in doing this.
	 * <erraddr> is the absolute PCI address. 
	 * Look through the mappings done for each slot, and
	 * send SIGBUS to all the processes that has the mapping
	 * in the range that caused a fault.
	 */

	if (PCIIO_CFGSPACE_ADDR(erraddr)){
		if (usrpci_cfgspace_error(usrpci_info, erraddr))
			retval = IOERROR_HANDLED;
	} else if (usrpci_sendsig(slot, erraddr)) {
		retval = IOERROR_HANDLED;
	}
	if (retval == IOERROR_HANDLED)
		pciio_error_devenable(usrpci_info->connectpt, error_code);

	return retval;
}

/* 
 *		SUPPORT ENTRY POINTS
 */
static void
usrpci_info_set(vertex_hdl_t usrpci_v, usrpci_drvinfo_t usrpci_info)
{
	hwgraph_fastinfo_set(usrpci_v, (arbitrary_info_t)usrpci_info);
}

static usrpci_drvinfo_t
usrpci_info_get(vertex_hdl_t usrpci_v)
{
	usrpci_drvinfo_t	info;

	info = (usrpci_drvinfo_t)hwgraph_fastinfo_get(usrpci_v);
	ASSERT(info);
	ASSERT(info->vertex = usrpci_v);

	return info;
}

static graph_error_t
usrpci_hwgraph_add(vertex_hdl_t connectpt, int slot)
{
	graph_error_t		rc;
	usrpci_drvinfo_t	usrpci_info;
	int                     i;
	pciio_info_t            pciio_info;
	pcibr_soft_t            pcibr_soft;
	vertex_hdl_t            bar;
	char                    label[3];
	
	
	pciio_info = pciio_info_get(connectpt);
	pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);

	NEW(usrpci_info);

	rc = hwgraph_path_add(connectpt, EDGE_LBL_USRPCI, &usrpci_info->vertex);
	
	if (rc != GRAPH_SUCCESS) 
		return rc;

	/*
	 * Create mem, io and config character device entry points
	 * under usrpci. These are the entry points used by the processes
	 * to access either the memory or the IO space in a specific
	 * PCI slot.
	 */
	
	rc = hwgraph_char_device_add(usrpci_info->vertex, EDGE_USRPCI_MEM32, 
			USRPCI_PREFIX, &usrpci_info->mem32_cv);
	ASSERT(rc == GRAPH_SUCCESS);

	rc = hwgraph_char_device_add(usrpci_info->vertex, EDGE_USRPCI_MEM64, 
			USRPCI_PREFIX, &usrpci_info->mem64_cv);
	ASSERT(rc == GRAPH_SUCCESS);

	rc = hwgraph_char_device_add(usrpci_info->vertex, EDGE_USRPCI_IO, 
			USRPCI_PREFIX, &usrpci_info->io_cv);
	ASSERT(rc == GRAPH_SUCCESS);

	rc = hwgraph_char_device_add(usrpci_info->vertex, EDGE_USRPCI_CONFIG,
			USRPCI_PREFIX, &usrpci_info->cfg_cv);
	ASSERT(rc == GRAPH_SUCCESS);

	rc = hwgraph_char_device_add(usrpci_info->vertex, EDGE_USRPCI_DEFAULT,
			USRPCI_PREFIX, &usrpci_info->default_cv);
	ASSERT(rc == GRAPH_SUCCESS);

	rc = hwgraph_path_add(usrpci_info->vertex, 
			      EDGE_USRPCI_BAR,
			      &bar);
	ASSERT(rc == GRAPH_SUCCESS);

	for (i = 0; i < PCI_CFG_BASE_ADDRS; i++) {
		usrpci_info->bars[i] = GRAPH_VERTEX_NONE;
	}
	
	for (i = 0; i < PCI_CFG_BASE_ADDRS; i++) {
		if (pcibr_soft->bs_slot[slot].bss_window[i].bssw_space !=
		    PCIIO_SPACE_NONE) {
			sprintf(label, "%d\0", i);
			rc = hwgraph_char_device_add(bar,
						     label,
						     USRPCI_PREFIX, 
						     &usrpci_info->bars[i]);
			ASSERT(rc == GRAPH_SUCCESS);
		}
	}

	/*
	 * Create a private structure and associate that structure
	 * with this vertex. This provides the mechanism to access 
	 * required information when needed.
	 */

	usrpci_info->slot      = slot;
	usrpci_info->connectpt = connectpt;

	usrpci_printf(("usrpci: pci %x def %x io %x cfg %x mem32 %x mem64 %x\n",
		usrpci_info->vertex, usrpci_info->default_cv, 
		usrpci_info->io_cv, usrpci_info->cfg_cv,
		usrpci_info->mem32_cv,  usrpci_info->mem64_cv));

	usrpci_info_set(usrpci_info->vertex, usrpci_info);
	usrpci_info_set(usrpci_info->mem32_cv, usrpci_info);
	usrpci_info_set(usrpci_info->mem64_cv, usrpci_info);
	usrpci_info_set(usrpci_info->default_cv, usrpci_info);
	usrpci_info_set(usrpci_info->io_cv, usrpci_info);
	usrpci_info_set(usrpci_info->cfg_cv, usrpci_info);
	
	for (i = 0; i < PCI_CFG_BASE_ADDRS; i++) {
		if (usrpci_info->bars[i] != GRAPH_VERTEX_NONE) {
			usrpci_info_set(usrpci_info->bars[i], usrpci_info);
		}
	}

	return rc;
}

/*
 * Graph setup entry points.
 * usrpci_device_register()
 *	Create and setup usrpci entry specific edges.
 *	This path would be used by user processes to map PCI space
 *	to its address space.
 */

void
usrpci_device_register(
	vertex_hdl_t	connectpt,	/* Vertex /hw/../pcibus/<slot>/	*/
	vertex_hdl_t	provider,	/* Vertex /hw/.../pcibus/ 	*/
	int		slot)

{
	char		vertex_name[256];
	graph_error_t	rc;

	hwgraph_vertex_name_get(connectpt, vertex_name, 256);
	usrpci_printf(("usrpci connect point: %s\n", vertex_name));
	hwgraph_vertex_name_get(provider, vertex_name, 256);
	usrpci_printf(("usrpci provider: %s\n", vertex_name));

	rc = usrpci_hwgraph_add(connectpt, slot);

	if (rc != GRAPH_SUCCESS) {
		 hwgraph_vertex_name_get(connectpt, vertex_name, 256);
		 cmn_err(CE_WARN, 
			"usrpci: Unable to add new vertices under %s\n", 
			vertex_name);
	}

	return;
}

/*
 * usrpci_space
 *	Given a vertex handle, and the usrpci_info structure,
 * 	find out which of the spaces, the vertex handle maps to.
 *	We have this routine, since same data structure is attached to
 *	all the vertices for a specific slot usrpci devices. 
 */
static pciio_space_t 
usrpci_space(vertex_hdl_t vh, usrpci_drvinfo_t ui, int *bar)
{
	int                  i;
	int                  slot;
	pciio_info_t         pciio_info;
	pcibr_soft_t         pcibr_soft;
	usrpci_drvinfo_t     usrpci_info;

	usrpci_info = usrpci_info_get(vh);
	slot = usrpci_info->slot;
	pciio_info = pciio_info_get(usrpci_info->connectpt);
	pcibr_soft = (pcibr_soft_t) pciio_info_mfast_get(pciio_info);
	
	
	if (vh == ui->io_cv)
		return PCIIO_SPACE_IO;
	if (vh == ui->default_cv)
		return PCIIO_SPACE_WIN0;
	if (vh == ui->mem32_cv)
		return PCIIO_SPACE_MEM32;
	if (vh == ui->mem64_cv)
		return PCIIO_SPACE_MEM64;
	if (vh == ui->cfg_cv)
		return PCIIO_SPACE_CFG;
	
	for (i = 0; i <= PCI_CFG_BASE_ADDRS; i++) {
		if (vh == ui->bars[i]) {
			*bar = i;
			return(pcibr_soft->bs_slot[slot].bss_window[i].bssw_space);
		}
	}
	
	return PCIIO_SPACE_NONE;

}
