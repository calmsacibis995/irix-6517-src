/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1999 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if !defined(IP20)

/*
 * usrbus.c - /dev user level bus access driver
 *
 *	This driver has intimate knowlege of EVEREST vme data structures.
 */

#ident	"$Revision: 1.47 $"

#include "sys/usrbus.h"
#include "sys/types.h"
#include "sys/cmn_err.h"
#include "sys/conf.h"
#include "sys/cred.h"
#include "sys/ddi.h"
#include "sys/debug.h"
#include "sys/edt.h"
#include "sys/errno.h"
#include "sys/giobus.h"
#include "sys/invent.h"
#include "sys/kabi.h"
#include "sys/kmem.h"
#include "sys/param.h"
#include "sys/par.h"
#include "sys/pio.h"
#include "ksys/vproc.h"
#include "sys/proc.h"
#include "sys/signal.h"
#include "sys/ksignal.h"
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/uio.h"
#include "sys/vmereg.h"
#include "sys/vme/usrvme.h"
#if ULI && EVEREST
#include "sys/uli.h"
#include "ksys/uli.h"
#endif
#include "ksys/ddmap.h"

int ubusdevflag = D_MP;

/*
 * The minor device number for a /dev/{bustype} device
 * is currently allocated as:
 *
 * bits 0-7	: the bus adapter's number
 * bits 8-12	: bus dependent
 * bits 13-17	: bus adapter type (ADAP_*)
 *
 * /dev/vme allocates bits 8-13 as:
 * bits 8-11	: The VME space that's being addressed
 * bit 12	: The mode of the space (Supervisor == 1/Non-priv == 0)
 * bit 13	: ADAP_VME == 1
 *
 * /dev/eisa allocates bits 8-15 as:
 * bit 8	: IO space or MEM space
 * bits 13-15	: ADAP_EISA == 6
 *
 * /dev/gio
 * bits 0-7	: bus adapter number
 * bits 8-9	: bus type (GIO32/GIO64)
 * bits 10-11	: GIO slot number (0,1,gfx)
 * bits 13-17	: GIO adapter type (ADAP_GIO)
 *
 * NOTE: the bit layout was originally chosen to make it easy to
 * read the minor number as a hex digit.  This layout was selected
 * just to be upward compatible with the existing one.  It probably
 * wouldn't hurt to take a couple of bits from the adapter number
 * and add them to the bus dependent field in the future.
 * 
 * Note : Everest systems may still need all the 8 bits in 'adapter'
 *	  field, since adapter number is evaluated as (slot*padap)
 *	  with slot being from 1-15 and padap being 1-7
 */

#define UBUS_TYPE(minor)	(((minor) >> 13) & 0x1f)
#define UBUS_ADAP(minor)	((minor) & 0xff)

/*
 * VME specific defines
 */
#define	A16SPACE	0
#define A24SPACE	1
#define A32SPACE	2
#define A64SPACE	3	/* Not supported yet. */
#define VAL_VMESPACE	3	/* Anything less than this is valid */

#define UVME_SPACE(minor)	(((minor) >> 8) & 0xf)
#define UVME_SUP(minor)		((minor) & 0x1000)

/*
 * EISA specific defines
 */
#define UEISA_SPACE(minor)	(((minor) >> 8) & 0x1)
#define VAL_EISASPACE		(PIOMAP_EISA_MEM+1)


/*
 * GIO Specific Macros
 */

#define	GIOBUS_SHFT		8
#define	GIOBUS_MASK		3

#define	GIOSLOT_SHFT		10
#define	GIOSLOT_MASK		3

#define	UGIO_SPACE(minor)	(((minor) >> 1) & 3)
#define	UGIO_SLOT(minor)	(((minor) >> GIOSLOT_SHFT) & GIOSLOT_MASK)
#define	UGIO_BUSTYPE(minor)	(((minor) >> GIOBUS_SHFT) & GIOBUS_MASK)

#define	VAL_GIOSLOT		(GIO_SLOT_GFX) 
#define	VAL_GIOBUS		(PIOMAP_GIO64)

/* Protect our list of maps */
static lock_t maplstlock;

#define	UV_ACTIVE	1
#define	UV_TIMEOUT	2
#define	UV_VMEBERR	4

struct uvmap_s {
	struct uvmap_s *uv_next, *uv_last;
	__psunsigned_t	uv_id;
	uint		uv_state;
	piomap_t       	*uv_map;
	uthread_t *	uv_ut;
};
typedef struct uvmap_s uvmap_t;

static uvmap_t maplst;

#if defined(EVEREST)

/* Prototypes for Everest only */
extern int ubuserrhandlr(piomap_t *, iopaddr_t, int);

#endif /* EVEREST */

void
ubusinit()
{
	spinlock_init(&maplstlock, "maplstlock");
	maplst.uv_next = maplst.uv_last = &maplst;

}


/* ARGSUSED */
int
ubusopen(dev_t *devp, int oflag, int otyp, cred_t *crp)
{ 
	int minor;
	register uint_t busadapters;


	minor = geteminor(*devp);
	busadapters = readadapters(UBUS_TYPE(minor));

	/* check for valid space */
	switch (UBUS_TYPE(minor)) {
		case ADAP_VME: {
			int adap;
			
			adap = VMEADAP_XTOI(UBUS_ADAP(minor));
			if( (adap < 0) || (adap >= busadapters) ||
			    (UVME_SPACE(minor) >= VAL_VMESPACE) )
				return ENODEV;
			break;
		}

		case ADAP_EISA:
			if( (UBUS_ADAP(minor) >= busadapters) ||
			    (UEISA_SPACE(minor) >= VAL_EISASPACE) )
				return ENODEV;
			break;

		case ADAP_GIO: {
			int adap;

			adap = UBUS_ADAP(minor);
			if ((adap < 0) || (adap >= busadapters) ||
			    (UGIO_SLOT(minor) > VAL_GIOSLOT) || 
			    (UGIO_BUSTYPE(minor) > VAL_GIOBUS))
				return	ENODEV;
			break;
		}

		default:
			return ENODEV;
	}

	return 0;
}

/* ARGSUSED */
int
ubusclose(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
	/* maps can span opens and closes so don't free
	 * anything up.  Even if we could, we wouldn't
	 * know what to free up.
	 */
	return 0;
}

/* ARGSUSED */
int
ubusmap(dev_t dev, vhandl_t *vt, off_t addr, size_t len, uint_t prot)
{
	uvmap_t		*up;
	int 		 minor, space, adap, priv, type, slot;
	int 		 err = 0;
	iospace_t 	 ios;
	void		*kvaddr;
	piomap_t	*pmap;
	int		s;

	/* check validity of request */
	if( len == 0 )
		return ENXIO;

	minor = geteminor(dev);
	type  = UBUS_TYPE(minor);
	adap  = UBUS_ADAP(minor);

	switch (type) {
	case ADAP_VME:
	    priv  = UVME_SUP(minor);
	    space = UVME_SPACE(minor);
	    switch( space ) {
		case A16SPACE:
			ios.ios_type = (priv) ? PIOMAP_A16S : PIOMAP_A16N;
			break;
		case A24SPACE:
			ios.ios_type = (priv) ? PIOMAP_A24S : PIOMAP_A24N;
			break;
		case A32SPACE:
			ios.ios_type = (priv) ? PIOMAP_A32S : PIOMAP_A32N;
			break;
		default:
			/* We shouldn't be here */
			ASSERT(0);
			return ENXIO;
	    }
	    break;

	case ADAP_EISA:
	    space = UEISA_SPACE(minor);
	    ios.ios_type = space;
	    break;

	case ADAP_GIO:
	    slot  = UGIO_SLOT(minor);
	    space = UGIO_BUSTYPE(minor);

	    switch(space){
		case PIOMAP_GIO32: ios.ios_type = PIOMAP_GIO32; break;
		case PIOMAP_GIO64: ios.ios_type = PIOMAP_GIO64; break;
		default		 : return (ENXIO);
	    }

	    switch(slot){
		/* Validate address and len against slot number */
		case GIO_SLOT_0: 
			if ((uint)addr < LIO_ADDR+LIO_GFX_SIZE || 
			    (((uint)addr + len) > (LIO_ADDR+LIO_GIO_SIZE)))
			    return ENXIO;
			break;

		/* Size of GIO Slot 1 */
#define	LIO_GIO1_SIZE	(LIO_GIO_SIZE + (LIO_GIO_SIZE-LIO_GFX_SIZE))

		case GIO_SLOT_1:
			if (((uint)addr < LIO_ADDR+LIO_GIO_SIZE) ||
			     (uint)addr+len > (LIO_ADDR+LIO_GIO1_SIZE))
			    return ENXIO;
			break;
#undef	LIO_GIO1_SIZE
		case GIO_SLOT_GFX:
			if ((uint)addr < LIO_ADDR || 
			    (((uint)addr + len) > (LIO_ADDR+LIO_GFX_SIZE)))
			    return ENXIO;
			break;
		default	:
			return ENXIO;
	    }

	    break;

	default:
	    return ENXIO;
	}

	ios.ios_iopaddr = (iopaddr_t)addr;
	ios.ios_size = len;
	
	/* allocate the map */
	pmap = pio_mapalloc(type,adap,&ios,PIOMAP_FIXED,"usrbus");
	if( pmap == NULL )
		return EINVAL;

	/* map in the address */
	kvaddr = pio_mapaddr(pmap,addr);

	/* allocate a record to store the information for
	 * later freeing
	 */
	if( (up = kmem_alloc(sizeof(uvmap_t),KM_SLEEP)) == NULL ) {
		pio_mapfree(pmap);
		return ENOSPC;
	}

	/* now map it into the user's address space 
	 * do this after the alloc since, we can't undo it if the
	 * alloc were to fail
	 */
	if( err = v_mapphys(vt,kvaddr,len) ) {
		pio_mapfree(pmap);
		kmem_free(up,sizeof(uvmap_t));
		return err;
	}

	up->uv_id = v_gethandle(vt);
	up->uv_map = pmap;

#if	EVEREST
	/* Register error handler */
	pio_seterrf(pmap, (void(*)())ubuserrhandlr);
	up->uv_ut  = curuthread;
	up->uv_state  = UV_ACTIVE;

	if ( type == ADAP_VME ) {
		CURVPROC_SET_PROXY(VSETP_USERVME, 0, 0);
	}
#endif

	/* Add it to the list */
	s = io_splock(maplstlock);

	up->uv_next = maplst.uv_next;
	up->uv_last = &maplst;
	up->uv_next->uv_last = up->uv_last->uv_next = up;
	
	io_spunlock(maplstlock, s);

	return 0;
}

void
ubustimeout(uvmap_t	*up)
{
	int	s;

	s = io_splock(maplstlock);
	/* Free from the list */
	up->uv_next->uv_last = up->uv_last;
	up->uv_last->uv_next = up->uv_next;
	io_spunlock(maplstlock, s);

	/* Free associated resources */
	pio_mapfree(up->uv_map);
	kmem_free((void *)up,sizeof(uvmap_t));
}

/* ARGSUSED */
int
ubusunmap(dev_t dev, vhandl_t *vt)
{
	__psunsigned_t 	 id;
	uvmap_t		*up;
	int		s;

	id = v_gethandle(vt);

	s = io_splock(maplstlock);

	for( up = maplst.uv_next ; up != &maplst ; up = up->uv_next ){
		if( up->uv_id != id ) 
			continue;
#if	EVEREST
		/* If we came to ubusunmap due to VME Bus error+SIGBUS, 
		 * we dont need to do all these. 
		 */
		if ((up->uv_map->pio_bus == ADAP_VME) && 
		    !(up->uv_state & UV_VMEBERR)){ /* Not yet flagged */
			/* Free resources after timeout */
			/* Gives a chance to catch any errors */
			timeout(ubustimeout, up, 1);
			up->uv_state |= UV_TIMEOUT;
			io_spunlock(maplstlock, s);
			return 0;
		}
#endif
		up->uv_next->uv_last = up->uv_last;
		up->uv_last->uv_next = up->uv_next;
		break;
	}
	io_spunlock(maplstlock, s);

	if( up == &maplst )
		return 0;

	/* NOTE: Any other resources freed here should also be done in
	 * ubustimeout for EVEREST VME case
	 */
	/* free up the necessary resources */
	if (!(up->uv_state & UV_TIMEOUT)){
		pio_mapfree(up->uv_map);
		kmem_free((void *)up,sizeof(uvmap_t));
	}

	return 0;
}


/*
 * ubuserrhandlr :  VME Bus write Error handler for Challenge/Onyx.
 * Gets invoked when system recognizes a bus error in VME write
 * using the pio map in usr vme address space.
 * Either send a signal to the relevant process or send a message to
 * syslog
 * Return: 1 If signal sent or found the right uvmap. 0 Otherwise.
 */
int
ubuserrhandlr(piomap_t *pio, iopaddr_t erraddr, int erradap)
{
        uvmap_t         *up;
        int             s, noerr=0;

        if (pio->pio_bus != ADAP_VME)	/* Just in case */
		return 0;

        s = io_splock(maplstlock);

        for (up = maplst.uv_next; up != &maplst; up = up->uv_next)
                if (up->uv_map == pio)
                        break;

        if ( up == &maplst )
		goto	errhand_end;

	if (up->uv_state & UV_TIMEOUT){
		/* Send a message to SYSLOG. */
		cmn_err(CE_WARN,
		"!User VME Write error- Adap %d Address 0x%x - User process has exited\n",
		erradap, erraddr);
		noerr = 1;
	} else {
		sigtouthread(up->uv_ut, SIGBUS, (k_siginfo_t *)NULL);
		up->uv_state |= UV_VMEBERR;
		noerr = 1;
	}

errhand_end:
        io_spunlock(maplstlock, s);

	return noerr;

}


#if ULI && EVEREST
/* eprintf: report on errors */

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

/* dprintf: general info */

#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

void
vmeuli_teardown(struct uli *uli)
{
    /* free the vector if it was dynamically allocated */
    if (uli->teardownarg3 != -1)
	vme_ivec_free((int)uli->teardownarg1, (int)uli->teardownarg3);
    else
	/* change the arg for the vector back to -1 */
	vmeuli_change_ivec((int)uli->teardownarg1, (int)uli->teardownarg2, -1);
}

/* ARGSUSED */
int
ubusioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *cr, int rvp)
{
    extern int vme_ivec_set_ext(int, int, int (*)(int), int, int);
    int minor = geteminor(dev);

    if (UBUS_TYPE(minor) != ADAP_VME) {
	eprintf(("ubusioctl: minor dev not VME\n"));
	return(EINVAL);
    }

    switch(cmd) {
      case UVIOCREGISTERULI: {

	  /* register a user level interrupt to handle a VME device intr */
	  int adap = UBUS_ADAP(minor), err;
	  struct uliargs args;
	  struct uli *uli;
	  extern int vme_ipl_to_cpu(int);
	  extern int (*vmeuliintrp)(int);

	  if (vmeuliintrp == 0) {
	      eprintf(("vmeuli device not configured\n"));
	      return(ENODEV);
	  }

	  if (COPYIN_XLATE((void*)arg, &args, sizeof(args),
			   irix5_to_uliargs, get_current_abi(), 1)) {
	      eprintf(("ubusioctl: bad copyin\n"));
	      return(EFAULT);
	  }

	  dprintf(("ubusioctl args pc 0x%x sp 0x%x funcarg 0x%x ipl %d vec %d\n",
		   args.pc, args.sp, args.funcarg, args.dd.vme.ipl,
		   args.dd.vme.vec));

	  if (vmeuli_validate_vector(adap, args.dd.vme.ipl, args.dd.vme.vec)) {
	      eprintf(("validate failed\n"));
	      return(EINVAL);
	  }

	  if (err = new_uli(&args, &uli, vme_ipl_to_cpu(args.dd.vme.ipl))) {
	      eprintf(("ubusioctl: new_uli failed\n"));
	      return(err);
	  }
	      
	  if (args.dd.vme.vec == -1) {
	      
	      /* no vector specified, allocate one dynamically */
	      if ((args.dd.vme.vec = vme_ivec_alloc(adap)) == -1) {
		  free_uli(uli);
		  eprintf(("can't allocate vector\n"));
		  return(EBUSY);
	      }
	      if (vme_ivec_set_ext(adap, args.dd.vme.vec,
				   vmeuliintrp, uli->index, EXEC_MODE_ISTK)) {
		  /* teardown has not been set up yet, so we need to free
		   * the vector manually
		   */
		  vme_ivec_free(adap, args.dd.vme.vec);
		  
		  free_uli(uli);
		  eprintf(("bad ivec set\n"));
		  return(EINVAL);
	      }
	      uli->teardownarg3 = args.dd.vme.vec;
	  }
	  else {
	      if (err = vmeuli_change_ivec(adap, args.dd.vme.vec,
					   uli->index)) {
		  free_uli(uli);
		  eprintf(("bad ivec reset\n"));
		  return(err);
	      }
	      uli->teardownarg3 = -1;
	  }

	  uli->teardown = vmeuli_teardown;
	  uli->teardownarg1 = adap;
	  uli->teardownarg2 = args.dd.vme.vec;

	  args.id = uli->index;
	  if (XLATE_COPYOUT(&args, (void*)arg, sizeof(args),
			    uliargs_to_irix5, get_current_abi(), 1)) {
	      eprintf(("ubus_ioctl: bad copyout\n"));
	      free_uli(uli);
	      return(EFAULT);
	  }
	  dprintf(("ubus_ioctl: return normal\n"));
	  return(0);
      }
    }

    eprintf(("bad cmd %d\n", cmd));
    return(EINVAL);
}

#endif /* ULI */

#endif /* !IP20 */
