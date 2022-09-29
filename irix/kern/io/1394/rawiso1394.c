/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 */
#ident "$Revision: 1.9 $"

#if IP32
#include "rawiso1394_kern.h"

#define MAX_RAWISO1394_PORTS 5
#define START_PACKET 0
#define INTERIOR_PACKET 1
#define END_PACKET 2
#define NTSC_FRAME 0
#define PAL_FRAME 1

int rawiso1394devflag = D_MP;        /* required for dynamic loading */
char *rawiso1394mversion = M_VERSION;/* Required for dynamic loading */

int rawiso1394_timeout_id;
int rawiso1394_HeadP;		/* Head of the isoRcv queue */

/* Port information */

int rawiso1394_nopenedports;

mutex_t rawiso1394_port_lock;

typedef struct rawiso1394_port_s rawiso1394_port_t;

rawiso1394_port_t rawiso1394_ports[MAX_RAWISO1394_PORTS];

/*
 * function prototypes
 */
int	       rawiso1394init (struct edt *edtp);
int 	       rawiso1394ioctl(
		            dev_t dev,
		            int cmd,
		            void *arg,
		            int mode,
		            cred_t *crp,
		            int *rvalp);
int	       rawiso1394map(dev_t dev,vhandl_t *vt,off_t off,int len,int prot);
int	       rawiso1394unmap(dev_t dev, vhandl_t *vt);
int	       rawiso1394open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int	       rawiso1394close(dev_t dev, int oflag, int otyp, cred_t *crp);
int	       rawiso1394poll(dev_t dev,
	    		   register short events,
	    		   int anyyet,
	    		   short *reventsp,
	    		   struct pollhead **phpp,
			   unsigned int *genp);
static void    rawiso1394_start_timercallbacks(void);
static void    rawiso1394_stop_timercallbacks(void);
static void    rawiso1394_process_packet(int pktnum);
static void    rawiso1394_distribute_packet(int *headerp, int *datap,
					    int cyclecounter, int len);
static int     rawiso1394_hwregister(void);
static void    rawiso1394_do_timeout(void);

static void    rawiso1394_deliver_packet(int portnum, int *headerp,
                          		 int *datap, int cyclecounter, int len);
static void printports(int portnum);

/* ********************************************************************
 * Create /hw/1394/rawiso directory and char_device_add rawiso1394 in it.   *
 * Returns 0 for no error and -1 if error occured while adding vertex *
 **********************************************************************/

static int
rawiso1394_hwregister(void)
{
    int rc;
    vertex_hdl_t vertex, tmp_vertex;
    info_t *info;

    /*
     * create a new info_t and partially initialize it
     */

    info = (info_t*) kmem_zalloc(sizeof(info_t), 0);
    if (info == 0) {
        printf("rawiso1394 rawiso1394_hwregister() out of memory\n");
        return ENOMEM;
    }

    vertex = GRAPH_VERTEX_NONE;
    rc = hwgraph_path_add(hwgraph_root, "ieee1394/rawiso", &vertex);
    if (   (rc != GRAPH_SUCCESS)
        || (vertex == NULL)
        || (vertex == GRAPH_VERTEX_NONE)) {
        cmn_err(CE_ALERT, "rawiso1394_hwregister: can't add path /hw/ieee1394/rawiso");
        return(-1);
    }

    info->parent_vertex = vertex;
    info->app_no = -1;
    
    if (hwgraph_char_device_add(vertex, "rawiso1394", "rawiso1394", &tmp_vertex) !=
                        GRAPH_SUCCESS) {
        printf("rawiso1394_hwregister: hwgraph_char_device_add failed\n");
        return (-1);
    }
    /* By default grant READ, WRITE permissions to all */
    hwgraph_chmod(tmp_vertex, (mode_t) 0666);

    hwgraph_fastinfo_set(tmp_vertex, (arbitrary_info_t)info);
    return(0);
}


/******************************************************************************
 *  Device Driver Entry Points
 ******************************************************************************/

/*
 * rawiso1394init()
 *
 * Description:
 * Called once by init() at system startup time.
 */

int
rawiso1394init (struct edt *edtp)
{
    int m;
    int status;
  
    /* Create /hw/1394/rawiso directory and char_device_add rawiso1394
     * in it.
     */
    status=rawiso1394_hwregister();

    rawiso1394_nopenedports=0;
    MUTEX_INIT(&rawiso1394_port_lock, MUTEX_DEFAULT, "rawisoport_lock");
    for(m=0;m<MAX_RAWISO1394_PORTS;m++) {
	rawiso1394_ports[m].opened=0;
	rawiso1394_ports[m].mapped=0;
	rawiso1394_ports[m].channel=0;
	rawiso1394_ports[m].corrupted=0;
	rawiso1394_ports[m].packetcount=0;
	initpollhead(&rawiso1394_ports[m].pq);
    }
   
    return(0);
}


/* Note: Here devp is ptr to "rawiso1394" vertex" */
int
rawiso1394open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    int m,s,i;
    info_t *info; 
    info_t *rawiso1394info;
    vertex_hdl_t rawiso1394_vhdl;
    vertex_hdl_t cloned_vertex;
    graph_error_t err;

    for(m=0;m<MAX_RAWISO1394_PORTS;m++) {
	if(rawiso1394_ports[m].opened == 0)break;
    }
    if(m==MAX_RAWISO1394_PORTS) {
	return ENODEV;
    }
    rawiso1394_ports[m].opened=1;
    rawiso1394_ports[m].mapped=0;
    rawiso1394_ports[m].channel=0;
    rawiso1394_ports[m].corrupted=0;
    rawiso1394_ports[m].packetcount=0;
    rawiso1394_nopenedports++;

    /* Clone the vertex pointed to by "devp" which is rawiso1394. */
    rawiso1394_vhdl = dev_to_vhdl(*devp);
    if(rawiso1394_vhdl == GRAPH_VERTEX_NONE) {
        printf("rawiso1394open dev_to_vhdl failed\n");
        return ENODEV;
    }
    rawiso1394info  = (info_t*)hwgraph_fastinfo_get(rawiso1394_vhdl);
    if(rawiso1394info == 0) {
        printf("rawiso1394open hwgraph_fastinfo_get failed\n");
        return ENODEV;
    }

    info = (info_t*) kmem_zalloc(sizeof(info_t), 0);
    if (info == 0) {
        printf("rawiso1394 rawiso1394open() out of memory\n");
        return ENOMEM;
    }

    info->parent_vertex = rawiso1394info->parent_vertex;
    info->app_no = m;

    err = hwgraph_vertex_clone(rawiso1394_vhdl, &cloned_vertex);
    if (err != GRAPH_SUCCESS) {
        printf("RAWISO1394, rawiso1394open(): Error cloning the driver for application.\n");
        return (-1);
    }
    
    /* Return the new values of the structures in the hwgraph database */
    hwgraph_fastinfo_set(cloned_vertex, (arbitrary_info_t)info );

    *devp = vhdl_to_dev(cloned_vertex);

    if(rawiso1394_nopenedports==1){
	int tailp;
	int isorcv_numdatapackets;

	isorcv_numdatapackets=board_driver_get_isorcv_info(&tailp);
	if(tailp == -1) {
	    rawiso1394_HeadP=isorcv_numdatapackets-1;
	} else if(tailp==0) {
	    rawiso1394_HeadP=isorcv_numdatapackets-1;
	} else {
	    rawiso1394_HeadP=tailp-1;
	}
	rawiso1394_start_timercallbacks();
    } 
    return 0;
}

static void
rawiso1394_do_timeout(void)
{
    int tailp,tailp_tmp;
    int isorcv_numdatapackets;

    if(rawiso1394_nopenedports==0)return;
    isorcv_numdatapackets=board_driver_get_isorcv_info(&tailp);
    if(tailp == -1) {
	/* Do nothing */
    } else if(tailp == rawiso1394_HeadP) {
	/* Do nothing */
    } else {
	do {
	    int headp;

	    /* tailp points to the last filled slot */
	    /* headp points to the last consumed slot */
	    /* (tailp==headp) implies nothing to be consumed */
	    /* (tailp== -1) implies no packets have arrived yet */

	    headp=rawiso1394_HeadP+1;
	    if(headp>=isorcv_numdatapackets)headp=0;
	    rawiso1394_HeadP=headp;
	    rawiso1394_process_packet(rawiso1394_HeadP);
	} while (tailp != rawiso1394_HeadP);
    }
    rawiso1394_timeout_id=itimeout(rawiso1394_do_timeout, 0, 1, splhi);
}

int
rawiso1394close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    int m,s;
    int freelen;
    int *freeaddr;
    vertex_hdl_t vhdl;
    vertex_hdl_t dest_vertex;
    info_t *info;
    graph_error_t err;
    char vertex_name[10];

    /* Get the app_no of the application calling close */
    /*    m = getminor(dev); */
    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);
    m = info->app_no;

    ASSERT(m<MAX_RAWISO1394_PORTS);
    ASSERT(m>=0);
    ASSERT(rawiso1394_ports[m].opened);
    if(rawiso1394_ports[m].mapped) {
	freeaddr=rawiso1394_ports[m].bufpool_kernaddr;
	freelen=rawiso1394_ports[m].maplen;
	rawiso1394_ports[m].mapped=0;
        rawiso1394_ports[m].corrupted=0;
        rawiso1394_ports[m].packetcount=0;
    } else {
	freeaddr=0;
    }
    rawiso1394_ports[m].opened=0;
    rawiso1394_nopenedports--;
    if(freeaddr) {
        kvpfree(freeaddr,freelen);
    }
    
    if (info->app_no < 0) /* rawiso1394 vertex */ {
       sprintf(vertex_name, "%s", "rawiso1394");

       /* Remove the edge */
       err = hwgraph_edge_remove(info->parent_vertex, vertex_name, &dest_vertex);
       if (err != GRAPH_SUCCESS) {
          printf("RAWISO1394, rawiso1394close(): Cannot close the vertex, rawiso1394.\n");
          return (-1);
       }

       /* Free memory allocated for info_t */
       free(info);

       hwgraph_vertex_unref(dest_vertex);
       hwgraph_vertex_destroy(dest_vertex);
    } 
    else {
    /* For cloned vertices, hwgraph_vertex_destroy should return
     * GRAPH_IN_USE on success because all the reference counts for
     * rawiso1394 may not yet have been decremented. Make this cloned
     * vertex NULL, and decrease the reference count of the cloned
     * vertices at rawiso1394 vertex by 1 */

      /* Free memory allocated for info_t */
      free(info);

      device_info_set(vhdl, NULL);
      if (((err = hwgraph_vertex_destroy(vhdl)) != GRAPH_IN_USE) &&
        (err != GRAPH_SUCCESS))
       return(EBUSY);
    }

    return 0;
}

static void
rawiso1394_start_timercallbacks()
{
    rawiso1394_timeout_id=itimeout(rawiso1394_do_timeout, 0, 1, splhi);
}

int
rawiso1394ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp)
{
    int ret,m,newtail,s,t,nfilled;
    int *iarg;
    vertex_hdl_t vhdl;
    info_t *info;
    int status;
    /* XXX should not need this once rawiso1394 is better integrated to
	perform a more functionality */

    ret=0;

    /* Get the app_no of the application calling mmap */
    /*    m = getminor(dev); */
    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);
    m = info->app_no;

    switch(cmd) {
    case RAWISO1394_SET_CHANNEL:
	copyin(arg,&rawiso1394_ports[m].channel,4);
	break;
    default:
	ret=EINVAL;
	break;
    }
    return(ret);
}

int
rawiso1394map(dev_t dev, vhandl_t *vt, off_t off, int len, int prot)
{
    int m,err,s;
    int *kernaddr;
    vertex_hdl_t vhdl;
    info_t *info;

    /* Get the app_no of the application calling mmap */
    /*    m = getminor(dev); */

    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);

    m = info->app_no;
     

    ASSERT(m<MAX_RAWISO1394_PORTS);
    ASSERT(m>=0);
    ASSERT(rawiso1394_ports[m].opened);
    if(rawiso1394_ports[m].mapped)return(EEXIST);
    if(len < sizeof(rawiso1394_bufhdr_t))return(EFBIG);

#ifdef _VCE_AVOIDANCE
  if (vce_avoidance)
    kernaddr = kvpalloc(btoc(len),VM_VACOLOR|VM_NOSLEEP, colorof(v_getaddr(vt)));
  else
#endif /* _VCE_AVOIDANCE */
    kernaddr = kvpalloc(btoc(len),VM_NOSLEEP, 0);

    if(kernaddr == 0) {
	return(ENOMEM);
    }

    if(err= v_mapphys(vt, kernaddr, len))
    {
        kvpfree(kernaddr,btoc(len));
        return(err);
    }

    /* Initialize the shared variables: *headp, *tailp, *dropcountp */

    kernaddr[0]=0;	/* *headp */
    kernaddr[1]=0;	/* *tailp */
    kernaddr[2]=0;	/* *dropcountp */

    MUTEX_LOCK(&rawiso1394_port_lock,-1);
    rawiso1394_ports[m].bufpool_kernaddr=kernaddr;
    rawiso1394_ports[m].bufpool_vaddr=v_getaddr(vt);
    rawiso1394_ports[m].maplen=len;
    rawiso1394_ports[m].headp=kernaddr;
    rawiso1394_ports[m].tailp=kernaddr+1;
    rawiso1394_ports[m].dropcountp=kernaddr+2;
    rawiso1394_ports[m].bufstart=kernaddr+3;
    rawiso1394_ports[m].bufnquads=(len-12)/4;
    rawiso1394_ports[m].tail=0;
    rawiso1394_ports[m].mapped=1;
    rawiso1394_ports[m].corrupted=0;
    rawiso1394_ports[m].packetcount=0;
    MUTEX_UNLOCK(&rawiso1394_port_lock);

    return(0);
}

int
rawiso1394unmap(dev_t dev, vhandl_t *vt)
{
    int m;
    vertex_hdl_t vhdl;
    info_t *info;

    /* Get the app_no of the application calling mmap */
    /*    m = getminor(dev); */
    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);
    m = info->app_no;

    if(rawiso1394_ports[m].mapped) {
        rawiso1394_ports[m].mapped=0;
        rawiso1394_ports[m].corrupted=0;
        rawiso1394_ports[m].packetcount=0;
        kvpfree(rawiso1394_ports[m].bufpool_kernaddr,btoc(rawiso1394_ports[m].maplen));
    }
    return 0;
}

static void
rawiso1394_process_packet(int pktnum)
{
    int *headerp;
    int *cyclecounterp;
    int cyclecounter;
    int *lenp;
    int *datap;
    int xferlen;
    int sct, dseq;
    int s;

    s=board_driver_get_headerlist_info(ISORCV, pktnum, &headerp,
                        &cyclecounterp, &lenp, &datap);
    if (s) {
        printf("rawiso1394_process_packets: board_driver_get_headerlist_info failed\n");
        return;
    }
    cyclecounter=*cyclecounterp;

    /* Cache invalidate the data */

    xferlen = (*lenp) & 0x1fff;
    if(xferlen > 4) {
        dki_dcache_inval((caddr_t)datap,xferlen-4);
    }

    /* Distribute the packet to all the rawiso1394_ports */

    rawiso1394_distribute_packet(headerp,datap,cyclecounter,xferlen);
}

int
rawiso1394poll(dev_t dev,
	    register short events,
	    int anyyet,
	    short *reventsp,
	    struct pollhead **phpp,
	    unsigned int *genp)
{
    int m,s,ready,dont_wakeup;
    unsigned int gen;
    vertex_hdl_t vhdl;
    info_t *info;
    int head,tail;

    /* Get the app_no of the application calling mmap */
    /*    m = getminor(dev); */
    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);
    m = info->app_no;


    ready=events;
    ASSERT(m<MAX_RAWISO1394_PORTS);
    ASSERT(m>=0);
    ASSERT(rawiso1394_ports[m].opened);
    if(!rawiso1394_ports[m].mapped)goto out;
    if((ready & (POLLIN | POLLRDNORM))) {
        MUTEX_LOCK(&rawiso1394_port_lock,-1);
	head = *(rawiso1394_ports[m].headp);
	tail = rawiso1394_ports[m].tail;
	dont_wakeup= (head == tail );
        /* snapshot pollhead generation number while we hold the lock */
        gen = POLLGEN(&rawiso1394_ports[m].pq);
        MUTEX_UNLOCK(&rawiso1394_port_lock);
	if(dont_wakeup) {
	    ready &= ~(POLLIN | POLLRDNORM);
	    if(!anyyet){
		*phpp= &rawiso1394_ports[m].pq;
		*genp= gen;
	    }
	} else {
	}
    }
out:
    *reventsp = ready;
    return(0);
}

static void
rawiso1394_distribute_packet(int *headerp,
			     int *datap, int cyclecounter, int len)
{
    int m;
    int packet_channel;


    /* Pick up the channel number in the packet */

    packet_channel= ((*headerp)>>8)&0x3f;

    /* Deliver the packets to the rawiso1394_ports */

    for(m=0;m<MAX_RAWISO1394_PORTS;m++){
	if(!rawiso1394_ports[m].opened || !rawiso1394_ports[m].mapped)continue;
	if(rawiso1394_ports[m].corrupted)continue;
	/* Filter out packets that aren't for the requested channel */
	if(rawiso1394_ports[m].channel != packet_channel){
		continue;
	}
	rawiso1394_deliver_packet(m, headerp, datap, cyclecounter, len);
    }
}

static void
rawiso1394_deliver_packet(int portnum, int *headerp,
                          int *datap, int cyclecounter, int len)
{
    int head,tail,xferquads,quadsneeded;
    int overflow;
    int nneeded, nfreeatfront, nfreeatback, nfree;
    int i;
    int *hp, *dp, *tp;

    tail = rawiso1394_ports[portnum].tail;
    head = *(rawiso1394_ports[portnum].headp);
    if(head < 0 || head >rawiso1394_ports[portnum].bufnquads){
	printf("rawiso1394 Port %d corrupted head pointer\n",portnum);
	rawiso1394_ports[portnum].corrupted=1;
	return;
    }

    xferquads = (len+3)/4;

    /* Figure out where to put the incoming packet */

    overflow=0;
    if(tail >= head) {
	nfreeatback = rawiso1394_ports[portnum].bufnquads-tail;
	nneeded = 2*sizeof(rawiso1394_bufhdr_t)/sizeof(int)+xferquads;
	if(nfreeatback >= nneeded){
	    hp=rawiso1394_ports[portnum].bufstart+tail;
	    dp=hp+sizeof(rawiso1394_bufhdr_t)/sizeof(int);
	    tp=dp+xferquads;
	} else {
	     nneeded -= sizeof(rawiso1394_bufhdr_t)/sizeof(int);
	     nfreeatfront = head;
	     if(nfreeatfront >= nneeded) {
	        hp=rawiso1394_ports[portnum].bufstart+tail;
	        dp=rawiso1394_ports[portnum].bufstart;
	        tp=dp+xferquads;
	     } else {
		overflow=1;
	     }
	}
    } else {
	nfree=head-tail;
	nneeded=2*sizeof(rawiso1394_bufhdr_t)/sizeof(int)+xferquads;
	if(nfree >= nneeded) {
	    hp=rawiso1394_ports[portnum].bufstart+tail;
	    dp=hp+sizeof(rawiso1394_bufhdr_t)/sizeof(int);
	    tp=dp+xferquads;
	} else {
	    overflow=1;
	}
    }

    /* Put the packet into the buffer */

    if(overflow) {
	(*rawiso1394_ports[portnum].dropcountp)++;
    } else {

	if(hp <  rawiso1394_ports[portnum].bufstart ||
	   hp >= rawiso1394_ports[portnum].bufstart + rawiso1394_ports[portnum].bufnquads) {
	    printf("rawiso1394 Port %d corrupted hp\n",portnum);
	    rawiso1394_ports[portnum].corrupted=1;
	    return;
	}
	((rawiso1394_bufhdr_t *)hp)->packet_count=rawiso1394_ports[portnum].packetcount;
	((rawiso1394_bufhdr_t *)hp)->packet_length=xferquads;
	((rawiso1394_bufhdr_t *)hp)->cyclecounter=cyclecounter;
	((rawiso1394_bufhdr_t *)hp)->endflag=0;
	if(tp <  rawiso1394_ports[portnum].bufstart ||
	   tp >= rawiso1394_ports[portnum].bufstart + rawiso1394_ports[portnum].bufnquads) {
	    printf("rawiso1394 Port %d corrupted tp\n",portnum);
	    rawiso1394_ports[portnum].corrupted=1;
	    return;
	}
	((rawiso1394_bufhdr_t *)tp)->packet_count=0;
	((rawiso1394_bufhdr_t *)tp)->packet_length=0;
	((rawiso1394_bufhdr_t *)tp)->cyclecounter=0;
	((rawiso1394_bufhdr_t *)tp)->endflag=1;

	if(xferquads)dp[0]= *headerp;
        xferquads--;
	if(xferquads) bcopy((int *)datap,dp+1,xferquads*4);

	tail=tp-rawiso1394_ports[portnum].bufstart;
	rawiso1394_ports[portnum].tail=tail;
	*(rawiso1394_ports[portnum].tailp)=tail;
    }
    rawiso1394_ports[portnum].packetcount++;
}

static void
printports(int portnum)
{
        printf("opened            %d\n", rawiso1394_ports[portnum].opened);
        printf("mapped            %d\n", rawiso1394_ports[portnum].mapped);
        printf("channel           %d\n", rawiso1394_ports[portnum].channel);
        printf("maplen            %d\n", rawiso1394_ports[portnum].maplen);
        printf("corrupted         %d\n", rawiso1394_ports[portnum].corrupted);
        printf("packetcount       %d\n", rawiso1394_ports[portnum].packetcount);
        printf("headp             0x%x\n", rawiso1394_ports[portnum].headp);
        printf("tailp             0x%x\n", rawiso1394_ports[portnum].tailp);
        printf("bufstart          0x%x\n", rawiso1394_ports[portnum].bufstart);
        printf("bufnquads         %d\n", rawiso1394_ports[portnum].bufnquads);
        printf("tail              %d\n", rawiso1394_ports[portnum].tail);
        printf(" bufpool_kernaddr 0x%x\n", rawiso1394_ports[portnum].bufpool_kernaddr);
        printf(" bufpool_vaddr    0x%x\n", rawiso1394_ports[portnum].bufpool_vaddr);
}

#endif /* IP32 || IP22 */
