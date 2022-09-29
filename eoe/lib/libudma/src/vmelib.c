
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"libdisk/diskheader.c $Revision: 1.5 $ of $Date: 1995/12/05 07:37:46 $"

#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/usrdma.h>

#include "udmalib.h"
#include "udmadefs.h"

struct dmareg_s {
	char 		 d_dummy[0x1100];
	int		 d_dum0;
	unsigned int 	 d_vbus;
	int		 d_dum1;
	unsigned int	 d_ebus;
	int		 d_dum2;
	unsigned int	 d_bcnt;
	int		 d_dum3;
	unsigned int	 d_parms;
};
typedef struct dmareg_s dmareg_t;


struct iobuf_s {
	struct iobuf_s	*i_next;
	struct iobuf_s	*i_last;
	unsigned int	 i_ebus;
	__psunsigned_t	 i_bp;
	int		 i_size;
};
typedef struct iobuf_s iobuf_t;

struct vmeinfo_s {
	int	 v_fd;
	void	*v_regs;
	int	 v_bufcnt;
	iobuf_t	 v_bufhd;

};
typedef struct vmeinfo_s vmeinfo_t;

struct dmaparms_s {
	unsigned int	d_parms;
	unsigned int	d_ebus;
	unsigned int	d_size;
};
typedef struct dmaparms_s dmaparms_t;

/* NOTE: These routines are specific to IP19, IO4 VMECC
 * If future VME DMA engines appear, this will need to be made
 * more modular, once again.
 */

int
vme_open(dmaid_t *dp)
{
	vmeinfo_t 	*vp;
	char		 devnm[PATH_MAX];


	/* This is a problem that needs to be watched. */
	if( VME_REG_SIZE < getpagesize() )
		return 1;
	
	if( (vp = malloc(sizeof(vmeinfo_t))) == NULL )
		return 1;

	(void)sprintf(devnm,"/dev/vme/dma%d",dp->d_adap);

	if( (vp->v_fd = open(devnm,O_RDWR)) < 1 ) {
		(void)free(vp);
		return 1;
	}

	/* map in the vmecc regs */
	if( (vp->v_regs = 
		mmap(NULL,VME_REG_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE,
		vp->v_fd, getpagesize()*VME_MAP_REGS)) == (void*)-1L ) {
		(void)close(vp->v_fd);
		(void)free(vp);
		return 1;
	}

	vp->v_bufhd.i_next = vp->v_bufhd.i_last = &vp->v_bufhd;
	vp->v_bufcnt = 0;
	
	dp->d_businfo = vp;

	return 0;
}

int
vme_close(dmaid_t *dp)
{
	vmeinfo_t 	*vp;


	vp = dp->d_businfo;

	/* XXX may want to deallocate all the users buffers 
	 * associated with this DMA engine at this point.
	 */
	if( vp->v_bufcnt )
		return 1;

	if( munmap(vp->v_regs,getpagesize()) == -1L )
		return 1;

	(void)close(vp->v_fd);
	(void)free(vp);

	dp->d_businfo = NULL;

	return 0;
}

void *
vme_allocbuf(dmaid_t *dp, int size)
{
	vmeinfo_t 	*vp;
	iobuf_t		*ip;
	unsigned int	*iobuf;


	vp = dp->d_businfo;
	
	if( (ip = malloc(sizeof(iobuf_t))) == NULL )
		return NULL;
	
	/* allocate dma buffer space */
	if( (iobuf = 
		mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE, vp->v_fd,
			getpagesize()*VME_MAP_BUF)) == (unsigned int *)-1 ) {
		(void)free(ip);
		return NULL;
	}
#ifdef	DEBUG
	printf("allocbuf: iobuf 0x%x size %d i_ebus: %x\n",iobuf, size, *iobuf);
#endif	/* DEBUG */

	/* pluck out Ebus DMA address */
	ip->i_ebus = *iobuf;
	ip->i_size = size;
	ip->i_bp = (__psunsigned_t)iobuf;

	/* add to list */
	ip->i_next = vp->v_bufhd.i_next;
	ip->i_last = &vp->v_bufhd;
	ip->i_next->i_last = ip->i_last->i_next = ip;

	vp->v_bufcnt++;

	return (void *)iobuf;
}

int 
vme_freebuf(dmaid_t *dp, void *bp)
{
	iobuf_t *ip;
	vmeinfo_t 	*vp;


	vp = dp->d_businfo;

	for( ip = vp->v_bufhd.i_next ; ip != &vp->v_bufhd ; ip = ip->i_next )
		if( ip->i_bp == (__psunsigned_t)bp ) 
			break;

	if( ip == &vp->v_bufhd )
		return 1;
		
	if( munmap(bp,ip->i_size) == -1 )
		return 1;

	ip->i_next->i_last = ip->i_last;
	ip->i_last->i_next = ip->i_next;
	vp->v_bufcnt--;

	return 0;
}


#define VMECC_DS(ds)	((ds)<<7)
#define VMECC_BLOCK	(1<<6)
#define VMECC_WRITE	(1<<9)
#define VMECC_ROR	(1<<10)
#define VMECC_THROT_2048	(1<<11)
#define VMECC_ERR	(3<<29)
#define VMECC_GO	(1<<31)

static unsigned int
vme_ebusaddr(dmaid_t *dp, __psunsigned_t uabuf, int size)
{
	iobuf_t		*ip;
	vmeinfo_t	*vp;


	vp = dp->d_businfo;

	for( ip = vp->v_bufhd.i_next ; ip != &vp->v_bufhd ; ip = ip->i_next ) {
		if( (uabuf >= ip->i_bp) && 
		    (uabuf + size <= ip->i_bp + ip->i_size) )
		    return ip->i_ebus + (unsigned int)(uabuf - ip->i_bp);
	}

	return 0;
}

/*
 * Convert VME accesible ebus address to cpu accessible ebus address.
 */
__psunsigned_t
vme_pvaddr(dmaid_t *dp, int ebus)
{
	iobuf_t		*ip;
	vmeinfo_t	*vp;

	vp = dp->d_businfo;

	for (ip = vp->v_bufhd.i_next; ip != &vp->v_bufhd; ip = ip->i_next) {
		if ((ip->i_ebus <= ebus) && (ip->i_ebus + ip->i_size))
			return ip->i_bp + (ebus - ip->i_ebus);
	}
	return 0;
}

/* ARGSUSED */
udmaprm_t *
vme_mkparms(dmaid_t *dp, void *dinfo, void *iobuf, int size)
{
	vmeparms_t *vp;
	unsigned int parms;
	unsigned int ebus;
	dmaparms_t	*dmap;


	vp = dinfo;

	/* check out the address modifier */
	if( vp->vp_addrmod & 0xc0 )
		return NULL;

	parms = vp->vp_addrmod | VMECC_GO;

	/* check out datum size */
	if( vp->vp_datumsz > VME_DS_DBLWORD )
		return NULL;

	parms |= VMECC_DS(vp->vp_datumsz);

	/* check block mode */
	if( vp->vp_block )
		parms |= VMECC_BLOCK;

	/* check out the direction */
	if( vp->vp_dir )
		parms |= VMECC_WRITE;

	/* check out release */
	if( vp->vp_release == VME_REL_ROR )
		parms |= VMECC_ROR;

	/* check out throttle */

	/* Throttle is set to 2048 by default for double word transfers.
	 * This is done in order to workaround a bug in VMECC double
	 * word operation mode.
	 */
	if( (vp->vp_throt == VME_THROT_2048 ) ||
	    (vp->vp_datumsz  == VME_DS_DBLWORD) )
		parms |= VMECC_THROT_2048;

	/* pull out ebus address and perform range check */
	if( (ebus = vme_ebusaddr(dp,(__psunsigned_t)iobuf,size)) == 0 )
		return NULL;
	
	if( (dmap = malloc(sizeof(dmaparms_t))) == NULL )
		return NULL;

	dmap->d_parms = parms;
	dmap->d_ebus = ebus;
	dmap->d_size = size;
	
	return (udmaprm_t *)dmap;
}

/* ARGSUSED */
int
vme_freeparms(dmaid_t *dp, udmaprm_t *dmap)
{
	(void)free(dmap);

	return 0;
}


int 
vme_start(dmaid_t *dp, void *busaddr, udmaprm_t *parms)
{
	volatile dmareg_t 	*dreg;
	unsigned int 		 parm;
	vmeinfo_t		*vp;
	int			errcode = 0;
	volatile __psunsigned_t	startvaddr, endvaddr;
	dmaparms_t		*dparm;
	


	startvaddr = endvaddr = 0;

	vp = dp->d_businfo;
	dreg = vp->v_regs;
	dparm = parms;

	/* set up parms and start io */
	dreg->d_bcnt = dparm->d_size;
	dreg->d_ebus = dparm->d_ebus;
	dreg->d_vbus = (unsigned int)busaddr;
	dreg->d_parms = dparm->d_parms;

	/* If the transfer is really large, sleep wait instead of busy wait */

	/*
	 * NAP_XFER gives the number of bytes we can transfer 
	 * During a tick, assuming transfer width of 8 bytes.
	 * This is a rough estimate of throughput capacity
	 */
#define	NAP_XFER	(256*1024)	

#define	NEED_SLEEP(bytes)  (long)((bytes)/(NAP_XFER))
			
	if (NEED_SLEEP(dparm->d_size)){
		sginap(NEED_SLEEP(dparm->d_size));
	}

	/* wait for it to complete */
	do {
		parm = dreg->d_parms;
		if( parm & VMECC_ERR ){
#ifdef	DEBUG
			printf("vme_start: error in xfer\n");
#endif
			errcode = 1;
			break;
		}

	} while( parm & VMECC_GO );

	/* 
	 * IO4 IA bug workaround.
	 * Due to a bug in IO4 IA chip, it's necessary not to leave any
	 * partial cachelines in IO4 IA's cache. So, if the transfer
	 * is not cache aligned, just touch the last cache line.
	 *
	 * Ideally, this work around is needed only in systems with
	 * multiple IO4s. There is no easy way to check that in this 
	 * code. So, we just touch the last line.
	 *
	 * Note: This code also assumes the "Secondary cache line"
	 * size is 128 bytes.
	 */ 
	startvaddr = vme_pvaddr(dp, dparm->d_ebus);

	if (startvaddr) {
		endvaddr   = startvaddr + dparm->d_size;
	}

#ifdef	DEBUG
	printf("d_ebus: %x d_size: %d, Virtaddr: <%x:%x> \n",
		dparm->d_ebus, dparm->d_size, startvaddr, endvaddr);
#endif

	if (startvaddr & 0x7f) {
		*(volatile __psunsigned_t *)startvaddr;
		
	}
	if (endvaddr & 0x7f) {
		*(volatile __psunsigned_t *)endvaddr;
	}

	return errcode;
}
