/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or dudlicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


/*
 * usrdma.c - generic user level DMA engine support
 *
 *	This file currently only supports the DMA engine on the IO4 VMECC.
 */

#ident	"$Revision: 1.14 $"

#include "sys/types.h"
#include "sys/cred.h"
#include "sys/conf.h"
#include "sys/ddi.h"
#include "sys/debug.h"
#include "sys/dmamap.h"
#include "sys/errno.h"
#include "sys/kmem.h"
#include "sys/immu.h"
#include "sys/param.h"
#include "sys/signal.h"
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/uio.h"
#include "sys/usrdma.h"
#include "sys/var.h"
#include "sys/vmereg.h"
#include "ksys/ddmap.h"


/* minor number looks like:
 * bits 0-3, bus type
 * rest are bus dependent
 */

#define UDMA_BUS(x)		((x)&0xf)
#define UDMA_ADAP(x)		(((x)>>4)&0xff)

int udmadevflag = D_MP;

static void err_init(void);
static int err_mmap(dev_t, vhandl_t*, off_t, size_t); 
static int err_unmap(dev_t, vhandl_t*); 
static int err_open(dev_t);
static int err_close(dev_t);

#if EVEREST
static void vme_init(void);
static int vme_mmap(dev_t, vhandl_t*, off_t, size_t); 
static int vme_unmap(dev_t, vhandl_t*); 
static int vme_open(dev_t);
static int vme_close(dev_t);
#endif /* EVEREST */

struct udma_entry_s {
	void	(*udma_init)(void);
	int	(*udma_open)(dev_t);
	int	(*udma_close)(dev_t);
	int	(*udma_mmap)(dev_t, vhandl_t *, off_t, size_t);
	int	(*udma_unmap)(dev_t,vhandl_t *);
};
typedef struct udma_entry_s udma_entry_t;

/* These are based on ADAP types from sys/edt.h */
udma_entry_t udma_bus[] = {
	{err_init, err_open, err_close, err_mmap, err_unmap},
#if EVEREST
	{vme_init, vme_open, vme_close, vme_mmap, vme_unmap}
#else /* !EVEREST */
	{err_init, err_open, err_close, err_mmap, err_unmap}
#endif /* !EVEREST */
};
#define UDMA_MAXBUS	(sizeof(udma_bus)/sizeof(udma_bus[0]))


void
udmainit(void)
{
	int i;

	for( i = 0 ; i < UDMA_MAXBUS ; i++ )
		(*udma_bus[i].udma_init)();
}

/* ARGSUSED */
int
udmaopen(dev_t *devp, int oflag, int otyp, cred_t *crp)
{ 
	int bus;

	bus = UDMA_BUS(geteminor(*devp));

	if( bus > UDMA_MAXBUS )
		return EINVAL;

	return (*udma_bus[bus].udma_open)(*devp);
}

/* ARGSUSED */
int
udmaclose(dev_t dev, int oflag, int otyp, cred_t *crp)
{
	int bus;

	bus = UDMA_BUS(geteminor(dev));

	if( bus > UDMA_MAXBUS )
		return EINVAL;

	return (*udma_bus[bus].udma_close)(dev);
}

/* what get's mapped in is based on the address passed in... 
 * 0 means the VMECC DMA engine regs
 * !0 means allocate memory and dma map it.
 */

/* ARGSUSED */
int
udmamap(dev_t dev, vhandl_t *vt, off_t addr, size_t len, uint_t prot)
{
	int bus;

	bus = UDMA_BUS(geteminor(dev));

	if( bus > UDMA_MAXBUS )
		return EINVAL;

	return (*udma_bus[bus].udma_mmap)(dev,vt,addr,len);
}

int
/* ARGSUSED */
udmaunmap(dev_t dev, vhandl_t *vt)
{
	int bus;

	bus = UDMA_BUS(geteminor(dev));

	if( bus > UDMA_MAXBUS )
		return EINVAL;

	return (*udma_bus[bus].udma_unmap)(dev,vt);
}

/* stub routines for non supported bus types */

static void
err_init(void)
{
}
/* ARGSUSED */
static int
err_open(dev_t dev)
{
	return ENODEV;
}
/* ARGSUSED */
static int
err_close(dev_t dev)
{
	return ENODEV;
}

/* ARGSUSED */
static int
err_mmap(dev_t dev, vhandl_t *vt, off_t addr, size_t len)
{
	return ENODEV;
}
/* ARGSUSED */
static int
err_unmap(dev_t dev, vhandl_t *vt)
{
	return ENODEV;
}

#if EVEREST
/* VME routines follow */

#include <sys/EVEREST/everest.h>

static mutex_t vme_sem;

struct vme_maplst_s {
	struct vme_maplst_s	*vm_next, *vm_last;
	__psunsigned_t		 vm_id;
	dmamap_t		*vm_map;
	void			*vm_kvaddr;
	unsigned int		 vm_dmaaddr;
	int			 vm_pgcnt;
	int			 vm_type;
};
typedef struct vme_maplst_s vme_maplst_t;

static unsigned char vme_state[EV_MAX_VMEADAPS];
#define VME_INUSE	0x1
#define VME_REGS	0x2
static vme_maplst_t vmehd;

extern void *vmecc_getregs(int);
extern int vmecc_allocdma(int);
extern void vmecc_freedma(int);


static void
vme_init(void)
{
	mutex_init(&vme_sem, MUTEX_DEFAULT, "vmedma");
	vmehd.vm_next = vmehd.vm_last = &vmehd;

	/* XXX If this assert fails, you need to make VME_REG_SIZE
	 * larger and rebuild libudma.
	 */
	ASSERT( VME_REG_SIZE >= NBPP );
}


static int
vme_open(dev_t dev)
{
	int intadap;


	/* check for valid adapter */
	if( (intadap = VMEADAP_XTOI(UDMA_ADAP(geteminor(dev)))) == -1 )
		return ENODEV;

	mutex_lock(&vme_sem,PRIBIO);
	/* check to see if engine's inuse or mapped */
	if( vme_state[intadap] || !vmecc_allocdma(intadap) ) {
		mutex_unlock(&vme_sem);
		return EBUSY;
	}

	vme_state[intadap] |= VME_INUSE;
	mutex_unlock(&vme_sem);

	return 0;
}

static int
vme_close(dev_t dev)
{
	int intadap;


	/* check for valid adapter */
	if( (intadap = VMEADAP_XTOI(UDMA_ADAP(geteminor(dev)))) == -1 )
		return ENODEV;

	mutex_lock(&vme_sem,PRIBIO);
	vme_state[intadap] &= ~VME_INUSE;
	mutex_unlock(&vme_sem);

	vmecc_freedma(intadap);

	return 0;
}

static int
vme_mmap(dev_t dev, vhandl_t *vt, off_t addr, size_t len)
{
	int		 err;
	void		*kvaddr;
	vme_maplst_t	*vm;
	int		 intadap;
	int		 xadap;


	/* check validity of request */
	if( len == 0 )
		return EINVAL;

	/* check for valid adapter */
	xadap = UDMA_ADAP(geteminor(dev));
	if( (intadap = VMEADAP_XTOI(xadap)) == -1 )
		return ENODEV;

	/* allocate a record for this mapping */
	if( (vm = kmem_alloc(sizeof(vme_maplst_t),KM_SLEEP)) == NULL )
		return ENOSPC;

	/* convert the addr into the page offset which is used
	 * to determine what function to provide, map registers
	 * or allocate a data buffer.
	 */
	addr = btoc(addr);

	vm->vm_type = addr;
	vm->vm_id = v_gethandle(vt);

	/* The address determines what to map */
	if( addr == VME_MAP_REGS ) {
		kvaddr = vmecc_getregs(intadap);
		if( len != VME_REG_SIZE ) {
			kmem_free(vm,sizeof(vme_maplst_t));
			return EINVAL;
		}
		vme_state[intadap] |= VME_REGS;
	}
	else if( addr == VME_MAP_BUF ) {
		int		 pgcnt;

		pgcnt = btoc(len);

		/* limit the amount which can be mapped */
		if( pgcnt > v.v_maxdmasz ) {
			kmem_free(vm,sizeof(vme_maplst_t));
			return ENOSPC;
		}

		/* allocate the buffer */
		if( (kvaddr = kvpalloc(pgcnt,0,0)) == NULL ) {
			kmem_free(vm,sizeof(vme_maplst_t));
			return ENOSPC;
		}

		/* clear memory before mapping into the user space */
		bzero(kvaddr,ctob(pgcnt));

		/* allocate the dma map
		 * NOTE: we can use an A32 map for any VME address space
		 * since this is the address the DMA engine creates, not
		 * the VME board.
		 */
		vm->vm_map = dma_mapalloc(DMA_A32VME,xadap,
						io_btoc(ctob(pgcnt)),0);
		if( vm->vm_map == NULL ) {
			kmem_free(vm,sizeof(vme_maplst_t));
			kvpfree(kvaddr,pgcnt);
			return ENOSPC;
		}

		(void)dma_map(vm->vm_map,kvaddr,len);
		vm->vm_dmaaddr = dma_mapaddr(vm->vm_map,kvaddr);
		*(unsigned int *)kvaddr = vm->vm_dmaaddr;
			
		vm->vm_kvaddr = kvaddr;
		vm->vm_pgcnt = pgcnt;
	}
	else {
		kmem_free(vm,sizeof(vme_maplst_t));
		return EINVAL;
	}

	/* add it to the list */
	mutex_lock(&vme_sem,PRIBIO);
	vm->vm_next = vmehd.vm_next;
	vm->vm_last = &vmehd;
	vm->vm_next->vm_last = vm->vm_last->vm_next = vm;
	mutex_unlock(&vme_sem);

	/* map it in. */
	err = v_mapphys(vt,kvaddr,len);

	/* clean up */
	if( err )
		(void)vme_unmap(dev,vt);

	return err;
}


static int
vme_unmap(dev_t dev, vhandl_t *vt)
{
	vme_maplst_t	*vm;
	__psunsigned_t	 id;
	int		 intadap;


	/* check for valid adapter */
	if( (intadap = VMEADAP_XTOI(UDMA_ADAP(geteminor(dev)))) == -1 )
		return ENODEV;

	id = v_gethandle(vt);

	mutex_lock(&vme_sem,PRIBIO);
	for( vm = vmehd.vm_next ; vm != &vmehd ; vm = vm->vm_next )
		if( vm->vm_id == id ) {
			vm->vm_next->vm_last = vm->vm_last;
			vm->vm_last->vm_next = vm->vm_next;
			break;
		}

	mutex_unlock(&vme_sem);

	/* unknown element */
	if( vm == &vmehd )
		return EINVAL;

	if( vm->vm_type == VME_MAP_REGS ) {
		mutex_lock(&vme_sem,PRIBIO);
		vme_state[intadap] &= ~VME_REGS;
		mutex_unlock(&vme_sem);
	}
	else {
		dma_mapfree(vm->vm_map);
		kvpfree(vm->vm_kvaddr,vm->vm_pgcnt);
	}
	kmem_free(vm,sizeof(vme_maplst_t));

	return 0;
}
#endif /* EVEREST */
