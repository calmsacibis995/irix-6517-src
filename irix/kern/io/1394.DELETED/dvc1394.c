/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 */
#ident "$Revision: 1.3 $"

#if IP32

#include "tifw_kern.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/edt.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/mman.h"
#include "sys/region.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/mbuf.h"
#include "sys/sbd.h"
#include "sys/socket.h"
#include "sys/cpu.h"
#include "sys/ddi.h"
#include <sys/cred.h>
#include "sys/ktime.h"
#include "sys/mload.h"
#include "sys/conf.h"
#include "sys/poll.h"
#include "dvc1394.h"
#include <sys/ksynch.h>
#include <sys/pfdat.h>

/* The following declarations should be defined in ksynch.h, but aren't.
   They should be removed when ksynch.h is updated to include them */
void ddi_sv_wait(sv_t *svp, void *lkp, int rv);
int ddi_sv_wait_sig(sv_t *svp, void *lkp, int rv);
/* End missing ksynch.h declarations */

#define WHOAMI "dvc1394"
#define MAX_DVC1394_PORTS 5
#define MAX_DVC1394_QUEUESIZE 30
#define START_PACKET 0
#define INTERIOR_PACKET 1
#define END_PACKET 2
#define NTSC_FRAME 0
#define PAL_FRAME 1

int dvc1394devflag = D_MP;        /* required for dynamic loading */
char *dvc1394mversion = M_VERSION;/* Required for dynamic loading */

int timeout_id;
extern tifw_state_t	tifw_state;
int HeadP;		/* Head of the isoRcv queue */

int dvc1394_channelguide_ntsc_tmp=0;
int dvc1394_channelguide_pal_tmp=0;
int dvc1394_channelguide_ntsc_tmp;
int dvc1394_channelguide_pal_tmp;

/* Per channel information */

typedef struct dvc1394_channel_status_s {
    int packet_offset;
    int packet_sequence;
    int frame_type;
    int packet_last_dseq;
    int packet_last_sequence;
    int ntsc;
}dvc1394_channel_status_t;

dvc1394_channel_status_t dvc1394chans[64];

/* Port information */

int nopenedports;

lock_t port_lock;
sv_t   port_sv;
typedef struct dvc1394_port_s {
    int opened;		/* 1 if opened */
    int mapped;		/* 1 if mmapped */
    int channel;	/* iso channel number */
    int maplen;		/* Amount of mmapped memory */
    int nbufs;		/* number of buffers */
    dvc1394_buf_t *curbufp;
    int head;
    int tail;
    int full;
    int waiting;
    dvc1394_buf_t *bufpool_kernaddr;
    caddr_t        bufpool_vaddr;
    struct pollhead pq; /* Poll head (for select, poll) */
}dvc1394_port_t;

dvc1394_port_t ports[MAX_DVC1394_PORTS];

/*
 * function prototypes
 */
int	       dvc1394edtinit (struct edt *edtp);
int 	       dvc1394ioctl(
		            dev_t dev,
		            int cmd,
		            void *arg,
		            int mode,
		            cred_t *crp,
		            int *rvalp);
int	       dvc1394map(dev_t dev,vhandl_t *vt,off_t off,int len,int prot);
int	       dvc1394open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int	       dvc1394close(dev_t dev, int oflag, int otyp, cred_t *crp);
int	       dvc1394poll(dev_t dev,
	    		   register short events,
	    		   int anyyet,
	    		   short *reventsp,
	    		   struct pollhead **phpp,
			   unsigned int *genp);
static void    dvc1394_start_timercallbacks(void);
static void    dvc1394_stop_timercallbacks(void);
static void    dvc1394_process_packet(volatile tifw_mem_layout_t *, int pktnum);
void	       dvc1394_distribute_packet(volatile HeaderListEntry_t *headerp,
			  		 volatile int *datap);
void
dvc1394_deliver_packet(int portnum,
		       volatile HeaderListEntry_t *headerp,
                       volatile int *datap,
		       dvc1394_channel_status_t *chan,
                       int packet_valid,
		       int packet_type);

/******************************************************************************
 *  Device Driver Entry Points
/******************************************************************************/

/*
 * dvc1394edtinit()
 *
 * Description:
 * Called once by edtinit() at system startup time.
 */

int
dvc1394edtinit (struct edt *edtp)
{
    int m;
    nopenedports=0;
    LOCK_INIT(&port_lock,(uchar_t)-1,plhi,(lkinfo_t*)-1);
    for(m=0;m<MAX_DVC1394_PORTS;m++) {
	ports[m].opened=0;
	ports[m].mapped=0;
	ports[m].channel=0;
	ports[m].curbufp=0;
	ports[m].head=0;
	ports[m].tail=0;
	ports[m].full=0;
	ports[m].waiting=0;
	initpollhead(&ports[m].pq);
    }
    SV_INIT(&port_sv,SV_DEFAULT,"port_sv");

    return(0);
}

int
dvc1394open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    int m,s,i;
    volatile tifw_mem_layout_t *tifw_mem;

    for(m=0;m<MAX_DVC1394_PORTS;m++) {
	if(ports[m].opened == 0)break;
    }
    if(m==MAX_DVC1394_PORTS) {
	return ENODEV;
    }
    ports[m].opened=1;
    ports[m].mapped=0;
    ports[m].channel=0;
    ports[m].curbufp=0;
    ports[m].head=0;
    ports[m].tail=0;
    ports[m].full=0;
    ports[m].waiting=0;
    nopenedports++;

    if(nopenedports==1){
	int tailp;
        tifw_mem = (volatile tifw_mem_layout_t *)tifw_state.mappages;
	dki_dcache_inval((caddr_t)&tifw_mem->isorcv.TailP,4);
        tailp=tifw_mem->isorcv.TailP;
	if(tailp == -1) {
	    HeadP=ISORCV_NUMDATAPACKETS-1;
	} else if(tailp==0) {
	    HeadP=ISORCV_NUMDATAPACKETS-1;
	} else {
	    HeadP=tailp-1;
	}
	for(i=0;i<64;i++) {
            dvc1394chans[i].packet_offset=0;
	}
	dvc1394_start_timercallbacks();
    } else {
    }
    /* clone-open: make this open be for a new minor device number. */
    /* use the index of the newly allocated entry in ports */
    *devp = makedev(emajor(*devp), m);

    return 0;
}

static void
dvc1394_do_timeout()
{
    volatile tifw_mem_layout_t *tifw_mem;
    int tailp,tailp_tmp;

    if(nopenedports==0)return;
    tifw_mem = (volatile tifw_mem_layout_t *)tifw_state.mappages;
    while(1) {
        dki_dcache_inval((caddr_t)&tifw_mem->isorcv.TailP,4);
        tailp=tifw_mem->isorcv.TailP;
        dki_dcache_inval((caddr_t)&tifw_mem->isorcv.TailP,4);
        tailp_tmp=tifw_mem->isorcv.TailP;
	if(tailp_tmp == tailp)break;
    }
    if(tailp == -1) {
	/* Do nothing */
    } else if(tailp == HeadP) {
	/* Do nothing */
    } else {
	do {
	    int headp;

	    /* tailp points to the last filled slot */
	    /* headp points to the last consumed slot */
	    /* (tailp==headp) implies nothing to be consumed */
	    /* (tailp== -1) implies no packets have arrived yet */

	    headp=HeadP+1;
	    if(headp>=ISORCV_NUMDATAPACKETS)headp=0;
	    HeadP=headp;
	    dvc1394_process_packet(tifw_mem,HeadP);
	} while (tailp != HeadP);
    }
    timeout_id=itimeout(dvc1394_do_timeout, 0, 1, splhi);
}

int
dvc1394close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    int m,s;
    int freelen;
    dvc1394_buf_t *freeaddr;

    m=minor(dev);
    ASSERT(m<MAX_DVC1394_PORTS);
    ASSERT(m>=0);
    ASSERT(ports[m].opened);
    if(ports[m].mapped) {
	freeaddr=ports[m].bufpool_kernaddr;
	freelen=ports[m].maplen;
	ports[m].mapped=0;
    } else {
	freeaddr=0;
    }
    ports[m].opened=0;
    nopenedports--;
    if(freeaddr) {
        kvpfree(freeaddr,freelen);
    }
    return 0;
}

static void
dvc1394_start_timercallbacks()
{
    timeout_id=itimeout(dvc1394_do_timeout, 0, 1, splhi);
}

int
dvc1394ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp)
{
    int ret,m,newtail,s,t,nfilled;
    ret=0;
    m=minor(dev);
    switch(cmd) {
    case DVC1394_SET_CHANNEL:
	copyin(arg,&ports[m].channel,4);
	break;
    case DVC1394_RELEASE_FRAME:
    	s=LOCK(&port_lock,splhi);
	if(ports[m].mapped) {
	    if(ports[m].tail != ports[m].head || ports[m].full) {
		ports[m].full=0;
		*rvalp=ports[m].head;
		ports[m].head++;
		if(ports[m].head==ports[m].nbufs)
		    ports[m].head=0;
	    } else {
		ret=EBUSY;
	    }
	} else {
	    ret=ENXIO;
	}
        UNLOCK(&port_lock,s);
	break;
    case DVC1394_GET_FRAME:
	if(ports[m].mapped) {
	    while(1) {
    		s=LOCK(&port_lock,splhi);
	        if(ports[m].tail != ports[m].head || ports[m].full) {
		    *rvalp=ports[m].head;
    		    UNLOCK(&port_lock,s);
		    break;
	        } else {
		    ports[m].waiting=1;
		    t=SV_WAIT_SIG(&port_sv,&port_lock,s);
		    if(!t) {
			ret=EINTR;
			break;
		    }
	        }
	    }
	} else {
	    ret=ENXIO;
	}
	break;
    case DVC1394_GET_FILLED:
	if(ports[m].mapped) {
	    s=LOCK(&port_lock,splhi);
	    if(ports[m].full){
		nfilled=ports[m].nbufs;
	    } else {
		nfilled=ports[m].tail-ports[m].head;
		if(nfilled<0)nfilled += ports[m].nbufs;
	    }
	    *rvalp=nfilled;
	    UNLOCK(&port_lock,s);
        } else {
	    *rvalp=0;
        }
	break;
    default:
	ret=EINVAL;
	break;
    }
    return(ret);
}

int
dvc1394map(dev_t dev, vhandl_t *vt, off_t off, int len, int prot)
{
    int m,err,s;
    dvc1394_buf_t *kernaddr;

    m = getminor(dev);

    ASSERT(m<MAX_DVC1394_PORTS);
    ASSERT(m>=0);
    ASSERT(ports[m].opened);
    if(ports[m].mapped)return(EEXIST);
    if(len < sizeof(dvc1394_buf_t))return(EFBIG);

#ifdef _VCE_AVOIDANCE
  if (vce_avoidance)
    kernaddr = kvpalloc(btoc(len),VM_VACOLOR, colorof(v_getaddr(vt)));
  else
#endif /* _VCE_AVOIDANCE */
    kernaddr = kvpalloc(btoc(len),0, 0);

    if(kernaddr == 0) {
	return(ENOMEM);
    }
    if(err= v_mapphys(vt, kernaddr, len))
    {
        kvpfree(kernaddr,btoc(len));
        return(err);
    }
    s=LOCK(&port_lock,splhi);
    ports[m].bufpool_kernaddr=kernaddr;
    ports[m].bufpool_vaddr=v_getaddr(vt);
    ports[m].maplen=len;
    ports[m].nbufs=len/sizeof(dvc1394_buf_t);
    ports[m].curbufp= 0;
    ports[m].head=0;
    ports[m].tail=0;
    ports[m].full=0;
    ports[m].waiting=0;
    ports[m].mapped=1;
    UNLOCK(&port_lock,s);

    return(0);
}

int
dvc1394unmap(dev_t dev, vhandl_t vt)
{
    int m;
    m=getminor(dev);
    if(ports[m].mapped) {
        ports[m].mapped=0;
        kvpfree(ports[m].bufpool_kernaddr,btoc(ports[m].maplen));
    }
    return 0;
}

void
dvc1394_process_packet(volatile tifw_mem_layout_t *tifw_mem, int pktnum)
{
    volatile HeaderListEntry_t *headerp;
    volatile int *datap;
    int xferlen;
    int sct, dseq;

    /* Cache invalidate the header */

    headerp = &(tifw_mem->isorcv.headerlist[pktnum]);
    dki_dcache_inval((caddr_t)headerp,16);

    /* Ignore packets that are not 492 bytes long */

    xferlen = headerp->len & 0x1fff;
    if(xferlen!=492) return;

    /* Cache invalidate the date */

    datap = &tifw_mem->isorcv.datapages[pktnum*512];
    dki_dcache_inval((caddr_t)datap,xferlen-4);

    /* Distribute the packet to all the ports */

    dvc1394_distribute_packet(headerp,datap);

}

int
dvc1394poll(dev_t dev,
	    register short events,
	    int anyyet,
	    short *reventsp,
	    struct pollhead **phpp,
	    unsigned int *genp)
{
    int m,s,ready,dont_wakeup;
    unsigned int gen;

    ready=events;
    m=getminor(dev);
    ASSERT(m<MAX_DVC1394_PORTS);
    ASSERT(m>=0);
    ASSERT(ports[m].opened);
    if(!ports[m].mapped)goto out;
    if((ready & (POLLIN | POLLRDNORM))) {
        s=LOCK(&port_lock,splhi);
	dont_wakeup= !(ports[m].head != ports[m].tail || ports[m].full);
	/* snapshot pollhead generation number while we hold the lock */
	gen = POLLGEN(&ports[m].pq);
        UNLOCK(&port_lock,s);
	if(dont_wakeup) {
	    ready &= ~(POLLIN | POLLRDNORM);
	    if(!anyyet){
		*phpp= &ports[m].pq;
		*genp= gen;
	    }
	} else {
	}
    }
out:
    *reventsp = ready;
    return(0);
}

void
dvc1394_distribute_packet(volatile HeaderListEntry_t *headerp,
			  volatile int *datap)
{
    int sct, dseq, dbn;
    int m;
    int packet_type,packet_valid, packet_channel;
    dvc1394_channel_status_t *chan;


    /* Pick up the fields of the first DIF block in the packet */

    sct = (datap[2]>>29) & 0x7;
    dseq = (datap[2]>>20) & 0xf;
    dbn=(datap[2]>>8)&0xff;

    /* Pick up the channel number in the packet */

    packet_channel= ((headerp->hdr)>>8)&0x3f;
    chan = &(dvc1394chans[packet_channel]);

    /* Determine whether we are at the start of a new frame or the end
       of a frame or neither */

    packet_valid=1;
    if(sct==0 && dseq==0 && dbn==0) {
	packet_type=START_PACKET;
	chan->packet_offset=0;
	chan->packet_sequence=0;
	chan->ntsc=((datap[1]&0x00800000)!=0x00800000);
	if(chan->ntsc) {
	    chan->packet_last_sequence=249;
	    chan->packet_last_dseq=9;
	    dvc1394_channelguide_ntsc_tmp |= 1<<packet_channel;
	} else {
	    chan->packet_last_sequence=299;
	    chan->packet_last_dseq=11;
	    dvc1394_channelguide_pal_tmp |= 1<<packet_channel;
	}
    } else if(sct==4 && dseq==chan->packet_last_dseq && dbn==129){
	if(chan->packet_sequence !=
	   chan->packet_last_sequence){
	    packet_valid=0;
	}
	packet_type=END_PACKET;
    } else {
	if(chan->packet_sequence <= 0 ||
               chan->packet_sequence >=
               chan->packet_last_sequence){
	    packet_valid=0;
	}
        packet_type=INTERIOR_PACKET;
    }

    /* Deliver the packets to the ports */

    for(m=0;m<MAX_DVC1394_PORTS;m++){
	if(!ports[m].opened || !ports[m].mapped)continue;
	/* Filter out packets that aren't for the requested channel */
	if(ports[m].channel != packet_channel){
		continue;
	}
	dvc1394_deliver_packet(m,
			       headerp,
			       datap,
			       chan,
			       packet_valid,
			       packet_type);
    }

    chan->packet_offset += 60;
    chan->packet_sequence++;
}

void
dvc1394_deliver_packet(int portnum,
		       volatile HeaderListEntry_t *headerp,
                       volatile int *datap,
		       dvc1394_channel_status_t *chan,
                       int packet_valid,
		       int packet_type)
{
    int s,packet_offset;

    packet_offset=chan->packet_offset;

    if(packet_type==START_PACKET) {

	/* If a dvc_1394_buf is being filled, mark it invalid and
	   close it out */

	if(ports[portnum].curbufp) {
	    ports[portnum].curbufp->valid=0;
	    ports[portnum].curbufp->ntsc=chan->ntsc;
	    ports[portnum].curbufp=0;
    	    s=LOCK(&port_lock,splhi);
	    ports[portnum].tail++;
	    if(ports[portnum].tail >= ports[portnum].nbufs)
		ports[portnum].tail=0;
	    if(ports[portnum].tail == ports[portnum].head)
		ports[portnum].full=1;
	    if(ports[portnum].waiting){
		ports[portnum].waiting=0;
		SV_BROADCAST(&port_sv);
	    }
	    pollwakeup(&ports[portnum].pq,POLLIN | POLLOUT | POLLRDNORM);
            UNLOCK(&port_lock,s);
	}

	/* If a new buf is available, open it */

	if(!ports[portnum].full) {
	    int tail;
	    tail=ports[portnum].tail;
	    ports[portnum].curbufp = &(ports[portnum].bufpool_kernaddr[tail]);
	    ports[portnum].curbufp->valid=1;
	}
    }

    /* Transfer the packet into the buffer */

    if(ports[portnum].curbufp) {
        if(packet_valid) {
            bcopy(
	        (void *)&datap[2],
		&((ports[portnum].curbufp->frame)[packet_offset]),
		480);
        } else {
	    ports[portnum].curbufp->valid=0;
        }
    }

    if(packet_type==END_PACKET) {
	if(ports[portnum].curbufp) {
	    ports[portnum].curbufp->ntsc=chan->ntsc;
	    ports[portnum].curbufp=0;
    	    s=LOCK(&port_lock,splhi);
	    ports[portnum].tail++;
	    if(ports[portnum].tail >= ports[portnum].nbufs)
		ports[portnum].tail=0;
	    if(ports[portnum].tail == ports[portnum].head)
		ports[portnum].full=1;
	    if(ports[portnum].waiting){
		ports[portnum].waiting=0;
		SV_BROADCAST(&port_sv);
	    }
	    pollwakeup(&ports[portnum].pq,POLLIN | POLLOUT | POLLRDNORM);
            UNLOCK(&port_lock,s);
	}
    }
}
#endif /* IP32 || IP22 */
