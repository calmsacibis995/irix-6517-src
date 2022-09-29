/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 */
#ident "$Revision: 1.23 $"

#define XMITCHAN 63

#include "dvc1394_kern.h"

/* XXX the following functions are direct calls into ti_lynx.c
       that's not really allowed.  They should be indirect calls
       through ieee1394.c */
extern void ti_lynx_setup_isoxmt(void);
extern void ti_lynx_start_isoxmt(void);
extern void ti_lynx_stop_isoxmt(void);
extern void ti_lynx_sendpacket(int slotnum,char *buf,int len,int chan);
extern void ti_lynx_getslotcycle(int slot, int* cyc);

/* debug_level = 0; no debug messages 
	       = 1; debug error messages
	       = 2; debug error messages & info
	       = 3; all messages
*/
int debug_level = 0;

int dvc1394devflag = D_MP;        /* required for dynamic loading */
char *dvc1394mversion = M_VERSION;/* Required for dynamic loading */

int dvc1394_timeout_id;
int dvc1394_RcvHeadP;		/* Head of the isorcv queue */
int dvc1394_XmtTailP;		/* Tail of the isoxmt queue */
int nullciphdr[2]= {0x01780000,0x8080ffff};

long long dvc1394_channelguide_ntsc_tmp=0;
long long dvc1394_channelguide_ntsc    =0;
long long dvc1394_channelguide_pal_tmp =0;
long long dvc1394_channelguide_pal     =0;
int 	  dvc1394_channelguide_count   =0;

dvc1394_channel_status_t dvc1394chans[64];

/* Port information */

int dvc1394_nopenedports;
int dvc1394_transmitport;
mutex_t dvc1394_port_lock;
sv_t   dvc1394_port_sv;
dvc1394_port_t dvc1394_ports[MAX_DVC1394_PORTS];

/*
 * function prototypes
 */
int	       dvc1394init (struct edt *edtp);
int 	       dvc1394ioctl(
		            dev_t dev,
		            int cmd,
		            void *arg,
		            int mode,
		            cred_t *crp,
		            int *rvalp);
int	       dvc1394map(dev_t dev,vhandl_t *vt,off_t off,int len,int prot);
int	       dvc1394unmap(dev_t dev, vhandl_t *vt);
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
static void    dvc1394_process_packet(int pktnum);
static void    dvc1394_distribute_packet(int *headerp, int *datap);
static int     dvc1394_hwregister(void);
static void    dvc1394_do_timeout(void);
static void    dvc1394_do_timeout_outputstuff(void);
static void    dvc1394_send_next_packet(int slotnum);
static void    dvc1394_do_timeout_inputstuff(void);
static int     dvc1394_handle_event(void *arg);
static void    dvc1394_send_frame_slice(int slotnum, int m);
static void    dvc1394_send_still_slice(int slotnum, int m);

static void
dvc1394_deliver_packet(int portnum,
                       volatile int *datap,
		       dvc1394_channel_status_t *chan,
                       int packet_valid,
		       int packet_type);

int ieee1394_gain_interest(long long Vendor_ID, long long Device_ID, 
		int app_id, long long begaddr, long long endaddr, fun_t fp);
int ieee1394_lose_interest(long long Vendor_ID, long long Device_ID, 
		int app_id, long long begaddr, long long endaddr, fun_t fp);


/* ********************************************************************
 * Create /hw/1394/dvc directory and char_device_add dvc1394 in it.   *
 * Returns 0 for no error and -1 if error occured while adding vertex *
 **********************************************************************/

static int
dvc1394_hwregister(void)
{
    int rc;
    vertex_hdl_t vertex, tmp_vertex;
    info_t *info;

    /*
     * create a new info_t and partially initialize it
     */

    info = (info_t*) kmem_zalloc(sizeof(info_t), 0);
    if (info == 0) {
        if (debug_level) printf("DVC1394:dvc1394_hwregister() out of memory\n");
        return ENOMEM;
    }

    vertex = GRAPH_VERTEX_NONE;
    rc = hwgraph_path_add(hwgraph_root, "ieee1394/dvc", &vertex);
    if (   (rc != GRAPH_SUCCESS)
        || (vertex == NULL)
        || (vertex == GRAPH_VERTEX_NONE)) {
        cmn_err(CE_ALERT, "dvc1394_hwregister: can't add path /hw/ieee1394/dvc");
        return(-1);
    }

    info->parent_vertex = vertex;
    info->app_no = -1;
    
    if (hwgraph_char_device_add(vertex, "dvc1394", "dvc1394", &tmp_vertex) !=
                        GRAPH_SUCCESS) {
        if (debug_level)
	   printf("dvc1394_hwregister: hwgraph_char_device_add failed\n");
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
 * dvc1394init()
 *
 * Description:
 * Called once by init() at system startup time.
 */

int
dvc1394init (struct edt *edtp)
{
    int m;
    int status;
  
    /* Create /hw/1394/dvc directory and char_device_add dvc1394
     * in it.
     */
    status=dvc1394_hwregister();

    dvc1394_nopenedports=0;
    dvc1394_transmitport=-1;
    MUTEX_INIT(&dvc1394_port_lock, MUTEX_DEFAULT, "dvcport_lock");
    for(m=0;m<MAX_DVC1394_PORTS;m++) {
	dvc1394_ports[m].opened=0;
	dvc1394_ports[m].mapped=0;
	dvc1394_ports[m].channel=0;
	dvc1394_ports[m].curbufp=0;
	dvc1394_ports[m].head=0;
	dvc1394_ports[m].tail=0;
	dvc1394_ports[m].full=0;
	dvc1394_ports[m].waiting=0;
	dvc1394_ports[m].direction=0;
	dvc1394_ports[m].compressedbuf=0;
	dvc1394_ports[m].transmitbuf=0;
	dvc1394_ports[m].transmitbufp=0;
	initpollhead(&dvc1394_ports[m].pq);
    }
    SV_INIT(&dvc1394_port_sv,SV_DEFAULT,"port_sv");
   
    return(0);
}


/* Note: Here devp is ptr to "dvc1394" vertex" */
int
dvc1394open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    int m,s,i;
    info_t *info; 
    info_t *dvc1394info;
    vertex_hdl_t dvc1394_vhdl;
    vertex_hdl_t cloned_vertex;
    graph_error_t err;

    for(m=0;m<MAX_DVC1394_PORTS;m++) {
	if(dvc1394_ports[m].opened == 0)break;
    }
    if(m==MAX_DVC1394_PORTS) {
	return ENODEV;
    }
    dvc1394_ports[m].opened=1;
    dvc1394_ports[m].mapped=0;
    dvc1394_ports[m].channel=0;
    dvc1394_ports[m].curbufp=0;
    dvc1394_ports[m].head=0;
    dvc1394_ports[m].tail=0;
    dvc1394_ports[m].full=0;
    dvc1394_ports[m].waiting=0;
    dvc1394_ports[m].direction=0;
    dvc1394_ports[m].compressedbuf=0;
    dvc1394_ports[m].transmitbuf=0;
    dvc1394_ports[m].transmitbufp=0;
    dvc1394_nopenedports++;

    /* Clone the vertex pointed to by "devp" which is dvc1394. */
    dvc1394_vhdl = dev_to_vhdl(*devp);
    if(dvc1394_vhdl == GRAPH_VERTEX_NONE) {
	if (debug_level)
           printf("dvc1394open dev_to_vhdl failed\n");
        return ENODEV;
    }
    dvc1394info  = (info_t*)hwgraph_fastinfo_get(dvc1394_vhdl);
    if(dvc1394info == 0) {
	if (debug_level)
          printf("dvc1394open hwgraph_fastinfo_get failed\n");
        return ENODEV;
    }

    info = (info_t*) kmem_zalloc(sizeof(info_t), 0);
    if (info == 0) {
	if (debug_level)
          printf("DVC1394:dvc1394open() out of memory\n");
        return ENOMEM;
    }

    info->parent_vertex = dvc1394info->parent_vertex;
    info->app_no = m;

    err = hwgraph_vertex_clone(dvc1394_vhdl, &cloned_vertex);
    if (err != GRAPH_SUCCESS) {
	if (debug_level)
          printf("DVC1394, dvc1394open(): Error cloning the driver for application.\n");
        return (-1);
    }
    
    if (debug_level>=2) printf("App no: %d.\n", m);

    /* Return the new values of the structures in the hwgraph database */
    hwgraph_fastinfo_set(cloned_vertex, (arbitrary_info_t)info );

    *devp = vhdl_to_dev(cloned_vertex);

    if(dvc1394_nopenedports==1){
	int tailp;
	int isorcv_numdatapackets;

	isorcv_numdatapackets=board_driver_get_isorcv_info(&tailp);
	if(tailp == -1) {
	    dvc1394_RcvHeadP=isorcv_numdatapackets-1;
	} else if(tailp==0) {
	    dvc1394_RcvHeadP=isorcv_numdatapackets-1;
	} else {
	    dvc1394_RcvHeadP=tailp-1;
	}
	for(i=0;i<64;i++) {
            dvc1394chans[i].packet_offset=0;
	}
	dvc1394_start_timercallbacks();
    } 
    return 0;
}

static void
dvc1394_do_timeout(void)
{

    /* If there aren't any open ports, there's no work to be done. */

    if(dvc1394_nopenedports==0)return;

    /* Process received isorcv packets */

    dvc1394_do_timeout_inputstuff();

    /* Setup isoxmt packets */

    dvc1394_do_timeout_outputstuff();
}

#ifdef LATER
static void
dvc1394_do_timeout_outputstuff(void)
{
    int headp,headp_tmp;
    int isoxmt_numdatapackets;

    /* Get a reliable reading of the headp of the transmit ring buffer */

    while(1) {
	isoxmt_numdatapackets=board_driver_get_isoxmt_info(&headp);
	isoxmt_numdatapackets=board_driver_get_isoxmt_info(&headp_tmp);
        if(headp_tmp == headp)break;
    }

#define ISOXMT_FILLPT 450
#define ISOXMT_MINFILLED 40
    if(headp == -1) {
	/* transmit ring buffer not running so do nothing */
    } else if(headp == dvc1394_XmtTailP) {
	/* transmit ring buffer hasn't moved since last time so do nothing */
    } else {
        int nfilled,ntofill,slot,i;
	nfilled=dvc1394_XmtTailP-headp;
	if(nfilled<0)nfilled+=isoxmt_numdatapackets;
	/* Check for overrun */
	if(nfilled > ISOXMT_FILLPT || nfilled < ISOXMT_MINFILLED) {
		/* XXX Should have better recovery */
	        ti_lynx_stop_isoxmt();
		printf("Overrun nfilled = %d\n",nfilled);
		dvc1394_XmtTailP=headp+ISOXMT_MINFILLED+1;
	}
	ntofill=ISOXMT_FILLPT-nfilled;
	/* We've transmitted some packets so get more to transmit */
	slot=dvc1394_XmtTailP;
	for(i=0;i<ntofill;i++) {
	    dvc1394_send_next_packet(slot);
	    slot++;
	    if(slot>=isoxmt_numdatapackets)slot=0;
	}
	dvc1394_XmtTailP=slot;
    }
}
#endif

#define ISOXMT_FILLPT 450
#define ISOXMT_MINFILLED 40

static void
dvc1394_do_timeout_outputstuff(void)
{
    int headp,headp_tmp;
    int isoxmt_numdatapackets;
    int nfilled,ntofill,slot,i;

    /* Get a reliable reading of the headp of the transmit ring buffer */

    while(1) {
	isoxmt_numdatapackets=board_driver_get_isoxmt_info(&headp);
	isoxmt_numdatapackets=board_driver_get_isoxmt_info(&headp_tmp);
        if(headp_tmp == headp)break;
    }

    if(headp == -1) {
	/* transmit ring buffer not running so do nothing */
	return;
    } else if(headp == dvc1394_XmtTailP) {
	/* transmit ring buffer hasn't moved since last time so do nothing */
	return;
    }

    /* Check for overrun */

    nfilled=dvc1394_XmtTailP-headp;
    if(nfilled<0)nfilled+=isoxmt_numdatapackets;
    if(nfilled > ISOXMT_FILLPT || nfilled < ISOXMT_MINFILLED) {
        /* XXX Should have better recovery */
        ti_lynx_stop_isoxmt();
        printf("Overrun nfilled = %d\n",nfilled);
        dvc1394_XmtTailP=headp+ISOXMT_MINFILLED+1;
    }

    /* We've transmitted some packets so get more to transmit */

    ntofill=ISOXMT_FILLPT-nfilled;
    slot=dvc1394_XmtTailP;
    for(i=0;i<ntofill;i++) {
        dvc1394_send_next_packet(slot);
        slot++;
        if(slot>=isoxmt_numdatapackets)slot=0;
    }
    dvc1394_XmtTailP=slot;
}


int seqnum;
int pkt[2+120];

static void
dvc1394_send_next_packet(int slotnum)
{
    int m;
    int can_start_a_still, can_start_a_frame;

    m=dvc1394_transmitport;

    if(m== -1){
	printf("dvc1394_send_next_packet no transmit port\n");
	return;
    }

    /* If there's a frame transmission in progress, send the next slice */

    if(dvc1394_ports[m].frame_in_progress) {
	return;
    }

    /* If there's a still transmission in progress, send the next slice */

    if(dvc1394_ports[m].still_in_progress) {
        dvc1394_send_still_slice(slotnum, m);
	return;
    }

    /* If we can start a new frame, start it and send the first slice */

    can_start_a_frame = ((dvc1394_ports[m].full) || dvc1394_ports[m].tail != dvc1394_ports[m].head);
    if(can_start_a_frame) {
	dvc1394_ports[m].frame_in_progress=1;
	dvc1394_ports[m].transmitbuf=(char *)&(dvc1394_ports[m].bufpool_kernaddr[dvc1394_ports[m].head].frame);
	dvc1394_ports[m].transmitbufp=dvc1394_ports[m].transmitbuf;
        dvc1394_send_still_slice(slotnum, m);
	return;
    } 

    /* If we can start a new still, start it and send the first slice */

    can_start_a_still = (dvc1394_ports[m].compressedbuf != 0);
    if(can_start_a_still) {
	dvc1394_ports[m].still_in_progress=1;
	dvc1394_ports[m].transmitbuf=dvc1394_ports[m].compressedbuf;
	dvc1394_ports[m].transmitbufp=dvc1394_ports[m].transmitbuf;
        dvc1394_send_still_slice(slotnum, m);
	dvc1394_ports[m].head++;
	dvc1394_ports[m].full=0;
	return;
    }

    /* Otherwise, send a null cip header */
	ti_lynx_sendpacket(slotnum,(char *)nullciphdr,8,XMITCHAN);
	return;

}

static void
dvc1394_send_frame_slice(int slotnum, int m)
{
}

static void
dvc1394_send_still_slice(int slotnum, int m)
{
    char *buf,*outp;
    buf=dvc1394_ports[m].transmitbuf;
    outp=dvc1394_ports[m].transmitbufp;
    dvc1394_ports[m].num+=dvc1394_ports[m].inc;
    if(dvc1394_ports[m].num>=dvc1394_ports[m].denom) {
        dvc1394_ports[m].num-=dvc1394_ports[m].denom;
        seqnum++;
        seqnum &= 0xff;
        pkt[0]=0x01780000+seqnum;
        pkt[1]=0x8080ffff;
        if(buf==outp){
            int cycle;
            ti_lynx_getslotcycle(slotnum,&cycle);  
            pkt[1]=0x80800000|((cycle+0x2000)&0xf000);
        }
        bcopy(outp,&(pkt[2]),480);
        outp+=480;
        if(outp==buf+144000){
	    dvc1394_ports[m].still_in_progress=0;
	    dvc1394_ports[m].frame_in_progress=0;
	}
        ti_lynx_sendpacket(slotnum,(char *)pkt,488,XMITCHAN);
        dvc1394_ports[m].transmitbufp=outp;
        } else {
        pkt[0]=0x01780000+seqnum;
        pkt[1]=0x8080ffff;
        ti_lynx_sendpacket(slotnum,(char *)pkt,8,XMITCHAN);
    }
}

#ifdef LATER
    }
#endif
    

static void
dvc1394_do_timeout_inputstuff(void)
{
    int tailp,tailp_tmp;
    int isorcv_numdatapackets;

    /* Get a reliable reading of the tailp of the receive ring buffer */

    while(1) {
	isorcv_numdatapackets=board_driver_get_isorcv_info(&tailp);
	isorcv_numdatapackets=board_driver_get_isorcv_info(&tailp_tmp);
	if(tailp_tmp == tailp)break;
    }
    if(tailp == -1) {
	/* receive ring buffer not running so do nothing */
    } else if(tailp == dvc1394_RcvHeadP) {
	/* receive ring buffer hasn't moved since last time so do nothing */
    } else {
	/* We've received some packets so pass them on for processing */
	do {
	    int headp;

	    /* tailp points to the last filled slot */
	    /* headp points to the last consumed slot */
	    /* (tailp==headp) implies nothing to be consumed */
	    /* (tailp== -1) implies no packets have arrived yet */

	    headp=dvc1394_RcvHeadP+1;
	    if(headp>=isorcv_numdatapackets)headp=0;
	    dvc1394_RcvHeadP=headp;
	    dvc1394_process_packet(dvc1394_RcvHeadP);
	} while (tailp != dvc1394_RcvHeadP);
    }

    /* Update the channel guide */

    dvc1394_channelguide_count++;
    if(dvc1394_channelguide_count==10){
        dvc1394_channelguide_count=0;
        dvc1394_channelguide_ntsc=dvc1394_channelguide_ntsc_tmp;
        dvc1394_channelguide_pal=dvc1394_channelguide_pal_tmp;
        dvc1394_channelguide_ntsc_tmp=0;
        dvc1394_channelguide_pal_tmp=0;
    }

    /* Schedule another timeout */

    dvc1394_timeout_id=itimeout(dvc1394_do_timeout, 0, 1, splhi);
}

int
dvc1394close(dev_t dev, int oflag, int otyp, cred_t *crp)
{
    int m,s;
    int freelen;
    dvc1394_buf_t *freeaddr;
    vertex_hdl_t vhdl;
    vertex_hdl_t dest_vertex;
    info_t *info;
    graph_error_t err;
    char vertex_name[10];

    /* Get the app_no of the application calling close */
    /*    m = getminor(dev); */
    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);
    if (debug_level >= 2)
      printf("close: Got app_no: %d.\n", info->app_no);
    m = info->app_no;

    ASSERT(m<MAX_DVC1394_PORTS);
    ASSERT(m>=0);
    ASSERT(dvc1394_ports[m].opened);

    if(dvc1394_ports[m].direction==2) {
	ti_lynx_stop_isoxmt();
	dvc1394_transmitport= -1;
    }
    if(dvc1394_ports[m].mapped) {
	freeaddr=dvc1394_ports[m].bufpool_kernaddr;
	freelen=dvc1394_ports[m].maplen;
	dvc1394_ports[m].mapped=0;
    } else {
	freeaddr=0;
    }
    dvc1394_ports[m].opened=0;
    dvc1394_ports[m].direction=0;
    if(dvc1394_ports[m].compressedbuf)
	kern_free(dvc1394_ports[m].compressedbuf);
    dvc1394_ports[m].compressedbuf=0;
    dvc1394_nopenedports--;
    if(freeaddr) {
        kvpfree(freeaddr,freelen);
    }
    
    if (info->app_no < 0) /* dvc1394 vertex */ {
       sprintf(vertex_name, "%s", "dvc1394");

       /* Remove the edge */
       err = hwgraph_edge_remove(info->parent_vertex, vertex_name, &dest_vertex);
       if (err != GRAPH_SUCCESS) {
	  if (debug_level)
            printf("DVC1394, dvc1394close(): Cannot close the vertex, dvc1394.\n");
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
     * dvc1394 may not yet have been decremented. Make this cloned
     * vertex NULL, and decrease the reference count of the cloned
     * vertices at dvc1394 vertex by 1 */

      /* Free memory allocated for info_t */
      free(info);

      device_info_set(vhdl, NULL);
      if (((err = hwgraph_vertex_destroy(vhdl)) != GRAPH_IN_USE) &&
        (err != GRAPH_SUCCESS))
       return(EBUSY);
    }

    return 0;
}

static int
dvc1394_handle_event(void *arg)
{
   callback_t *callback_pktp;
   int i;
 
  callback_pktp=(callback_t*) arg;
  if (debug_level >= 2) {

   printf("dvc1394_handle_event, callback_code: %d\n", callback_pktp->callback_code);
   if (callback_pktp->callback_code==-1) {
       printf("Device Disappeared, app_id: %d\n", callback_pktp->app_id);
   } 
   else {
     printf("app_id: %d\n", callback_pktp->app_id);
     printf("source_id: %d\n", callback_pktp->source_id);
     printf("destination_id: %d\n", callback_pktp->destination_id);
     printf("destination_offset: 0x%llx\n", callback_pktp->destination_offset);
     printf("tlabel: %d\n", callback_pktp->tlabel);
     printf("data_length: %d\n", callback_pktp->data_length);
     printf("extended_tcode: %d\n", callback_pktp->extended_tcode);
     for (i=0; i<callback_pktp->data_length; i+=4) 
       printf("payload[i/4]: 0x%x\n", callback_pktp->payload[i/4]);
   }
  }
   return 0;
}

static void
dvc1394_start_timercallbacks()
{
    dvc1394_timeout_id=itimeout(dvc1394_do_timeout, 0, 1, splhi);
}

int
dvc1394ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp)
{
    int ret,m,newtail,s,t,nfilled,i,isoxmt_numdatapackets;
    int *iarg;
    caddr_t carg;
    int direction;
    vertex_hdl_t vhdl;
    info_t *info;
    int status, recv_debug_level;

    /* XXX should not need this once dvc1394 is better integrated to
	perform a more functionality */
    dvc1394_gain_lose_interest_t interest;

    ret=0;

    /* Get the app_no of the application calling mmap */
    /*    m = getminor(dev); */
    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);
    if (debug_level>=2)
      printf("ioctl: Got app_no: %d.\n", info->app_no);
    m = info->app_no;

    switch(cmd) {
    case DVC1394_SET_DEBUG_LEVEL:
	copyin(arg, &recv_debug_level, 4);
	if ((recv_debug_level >=0) && (recv_debug_level <=3))
	    debug_level=recv_debug_level;
	break;

    case DVC1394_SET_CHANNEL:
	copyin(arg,&dvc1394_ports[m].channel,4);
	break;

    /* XXX get rid of this ioctl in the future */
    case DVC1394_GAIN_INTEREST:
	copyin(arg,&interest,sizeof(dvc1394_gain_lose_interest_t));
	status=ieee1394_gain_interest(interest.Vendor_ID, interest.Device_ID, m,
		     interest.begaddr, interest.endaddr, dvc1394_handle_event);
	if ((status!=0) && (debug_level)) {
	   printf("ieee1394_gain_interest failed: %d\n", status);
	}
	ret=status;
        break;
    /* XXX get rid of this ioctl in the future */
    case DVC1394_LOSE_INTEREST:
	copyin(arg,&interest,sizeof(dvc1394_gain_lose_interest_t));
	status=ieee1394_lose_interest(interest.Vendor_ID, interest.Device_ID, m,
                     interest.begaddr, interest.endaddr, dvc1394_handle_event);
        if ((status!=0) && (debug_level)) {
           printf("ieee1394_lose_interest failed: %d\n", status);
        }
	ret=status;
        break;

    case DVC1394_RELEASE_FRAME:
    	MUTEX_LOCK(&dvc1394_port_lock,-1);
	if(dvc1394_ports[m].mapped) {
	    if(dvc1394_ports[m].tail != dvc1394_ports[m].head || dvc1394_ports[m].full) {
		dvc1394_ports[m].full=0;
		*rvalp=dvc1394_ports[m].head;
		dvc1394_ports[m].head++;
		if(dvc1394_ports[m].head==dvc1394_ports[m].nbufs)
		    dvc1394_ports[m].head=0;
	    } else {
		ret=EBUSY;
	    }
	} else {
	    ret=ENXIO;
	}
        MUTEX_UNLOCK(&dvc1394_port_lock);
	break;
    case DVC1394_GET_FRAME:
	if(dvc1394_ports[m].mapped) {
	    while(1) {
    		MUTEX_LOCK(&dvc1394_port_lock,-1);
	        if(dvc1394_ports[m].tail != dvc1394_ports[m].head || dvc1394_ports[m].full) {
		    *rvalp=dvc1394_ports[m].head;
    		    MUTEX_UNLOCK(&dvc1394_port_lock);
		    break;
	        } else {
		    dvc1394_ports[m].waiting=1;
		    t=SV_WAIT_SIG(&dvc1394_port_sv,&dvc1394_port_lock,0);
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
    case DVC1394_PUT_FRAME:
	if(dvc1394_ports[m].mapped) {
	    while(1) {
                MUTEX_LOCK(&dvc1394_port_lock,-1);
                if(!dvc1394_ports[m].full) {
		    copyin(arg,&(dvc1394_ports[m].bufpool_kernaddr[dvc1394_ports[m].tail].frame),144000);
		    dvc1394_ports[m].tail++;
		    if(dvc1394_ports[m].tail >= dvc1394_ports[m].nbufs)dvc1394_ports[m].tail=0;
		    if(dvc1394_ports[m].tail == dvc1394_ports[m].head)dvc1394_ports[m].full=1;
                    MUTEX_UNLOCK(&dvc1394_port_lock);
                    break;
		} else {
                    dvc1394_ports[m].waiting=1;
                    t=SV_WAIT_SIG(&dvc1394_port_sv,&dvc1394_port_lock,0);
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
	if(dvc1394_ports[m].mapped) {
	    MUTEX_LOCK(&dvc1394_port_lock,-1);
	    if(dvc1394_ports[m].full){
		nfilled=dvc1394_ports[m].nbufs;
	    } else {
		nfilled=dvc1394_ports[m].tail-dvc1394_ports[m].head;
		if(nfilled<0)nfilled += dvc1394_ports[m].nbufs;
	    }
	    *rvalp=nfilled;
	    MUTEX_UNLOCK(&dvc1394_port_lock);
        } else {
	    *rvalp=0;
        }
	break;
    case DVC1394_GET_CHANNELGUIDE:
	iarg=(int*)arg;
        copyout((caddr_t)&dvc1394_channelguide_ntsc,iarg,8);
        copyout((caddr_t)&dvc1394_channelguide_pal,iarg+2,8);
	break;
    case DVC1394_SET_DIRECTION:
	copyin(arg,&direction,sizeof(int));
	if((direction==1 || direction==2) && dvc1394_ports[m].direction==0) {
	    dvc1394_ports[m].direction=direction;
	    if(direction==2) {
	        ti_lynx_stop_isoxmt();
	        dvc1394_XmtTailP=ISOXMT_FILLPT;
	        ti_lynx_setup_isoxmt();
	        isoxmt_numdatapackets=board_driver_get_isoxmt_info(&i);
	        for(i=0;i<isoxmt_numdatapackets;i++){
		    ti_lynx_sendpacket(i,(char *)nullciphdr,8,XMITCHAN);
		}
	        dvc1394_ports[m].num=0;
	        dvc1394_ports[m].denom=320;
	        dvc1394_ports[m].inc=300;
		dvc1394_transmitport=m;
	        ti_lynx_start_isoxmt();
	    }
	} else {
	    ret=EINVAL;
	}
	break;
    case DVC1394_LOAD_FRAME:
	if(dvc1394_ports[m].direction != 2) {
	    ret=EINVAL;
	} else {
	    if(dvc1394_ports[m].compressedbuf==0){
		dvc1394_ports[m].compressedbuf=(char *)kern_malloc(144000);
	    }
	    if(dvc1394_ports[m].compressedbuf==0) {
		ret=ENOMEM;
	    } else {
	        carg=(caddr_t)arg;
		copyin(carg,dvc1394_ports[m].compressedbuf,144000);
	    }
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
    vertex_hdl_t vhdl;
    info_t *info;

    /* Get the app_no of the application calling mmap */
    /*    m = getminor(dev); */
    if (debug_level >= 2)
      printf("Came to dvc1394map\n");

    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);

    if (debug_level>=2)
      printf("map: Got app_no: %d.\n", info->app_no);
    m = info->app_no;
     

    ASSERT(m<MAX_DVC1394_PORTS);
    ASSERT(m>=0);
    ASSERT(dvc1394_ports[m].opened);
    if(dvc1394_ports[m].mapped)return(EEXIST);
    if(len < sizeof(dvc1394_buf_t))return(EFBIG);

#ifdef _VCE_AVOIDANCE
  if (vce_avoidance)
    kernaddr = kvpalloc(btoc(len),VM_VACOLOR|VM_NOSLEEP, colorof(v_getaddr(vt)));
  else
    kernaddr = kvpalloc(btoc(len),VM_NOSLEEP, 0);
#endif /* _VCE_AVOIDANCE */

    if(kernaddr == 0) {
	return(ENOMEM);
    }
    if(err= v_mapphys(vt, kernaddr, len))
    {
        kvpfree(kernaddr,btoc(len));
        return(err);
    }
    MUTEX_LOCK(&dvc1394_port_lock,-1);
    dvc1394_ports[m].bufpool_kernaddr=kernaddr;
    dvc1394_ports[m].bufpool_vaddr=v_getaddr(vt);
    dvc1394_ports[m].maplen=len;
    dvc1394_ports[m].nbufs=len/sizeof(dvc1394_buf_t);
    dvc1394_ports[m].curbufp= 0;
    dvc1394_ports[m].head=0;
    dvc1394_ports[m].tail=0;
    dvc1394_ports[m].full=0;
    dvc1394_ports[m].waiting=0;
    dvc1394_ports[m].mapped=1;
    MUTEX_UNLOCK(&dvc1394_port_lock);

    return(0);
}

int
dvc1394unmap(dev_t dev, vhandl_t *vt)
{
    int m;
    vertex_hdl_t vhdl;
    info_t *info;

    /* Get the app_no of the application calling mmap */
    /*    m = getminor(dev); */
    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);
    if (debug_level>=2)
      printf("unmap: Got app_no: %d.\n", info->app_no);
    m = info->app_no;

    if(dvc1394_ports[m].mapped) {
        dvc1394_ports[m].mapped=0;
        kvpfree(dvc1394_ports[m].bufpool_kernaddr,btoc(dvc1394_ports[m].maplen));
    }
    return 0;
}

static void
dvc1394_process_packet(int pktnum)
{
    int *headerp;
    int *cyclecounterp;
    int *lenp;
    int *datap;
    int xferlen;
    int sct, dseq;
    int s;

    s=board_driver_get_headerlist_info(ISORCV, pktnum, &headerp, 
			&cyclecounterp, &lenp, &datap);
    if ((s) && (debug_level)) {
	printf("dvc1394_process_packets: board_driver_get_headerlist_info failed\n");
	return;
    }

    /* Ignore packets that are not 492 bytes long */
    xferlen = (*lenp) & 0x1fff;
    if(xferlen!=492) return;

    /* Cache invalidate the data */
    dki_dcache_inval((caddr_t)datap,xferlen-4);

    /* Distribute the packet to all the dvc1394_ports */
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
    vertex_hdl_t vhdl;
    info_t *info;

    /* Get the app_no of the application calling mmap */
    /*    m = getminor(dev); */
    vhdl = dev_to_vhdl(dev);
    info = (info_t*) hwgraph_fastinfo_get(vhdl);
    if (debug_level>=2)
      printf("poll: Got app_no: %d.\n", info->app_no);
    m = info->app_no;


    ready=events;
    ASSERT(m<MAX_DVC1394_PORTS);
    ASSERT(m>=0);
    ASSERT(dvc1394_ports[m].opened);
    if(!dvc1394_ports[m].mapped)goto out;
    if((ready & (POLLIN | POLLRDNORM))) {
        MUTEX_LOCK(&dvc1394_port_lock,-1);
	dont_wakeup= !(dvc1394_ports[m].head != dvc1394_ports[m].tail || dvc1394_ports[m].full);
        /* snapshot pollhead generation number while we hold the lock */
        gen = POLLGEN(&dvc1394_ports[m].pq);
        MUTEX_UNLOCK(&dvc1394_port_lock);
	if(dont_wakeup) {
	    ready &= ~(POLLIN | POLLRDNORM);
	    if(!anyyet){
		*phpp= &dvc1394_ports[m].pq;
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
dvc1394_distribute_packet(int *headerp, int *datap)
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

    packet_channel= ((*headerp)>>8)&0x3f;
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
	    dvc1394_channelguide_ntsc_tmp |= 1ll<<packet_channel;
	} else {
	    chan->packet_last_sequence=299;
	    chan->packet_last_dseq=11;
	    dvc1394_channelguide_pal_tmp |= 1ll<<packet_channel;
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


    /* Deliver the packets to the dvc1394_ports */

    for(m=0;m<MAX_DVC1394_PORTS;m++){
	if(!(dvc1394_ports[m].direction==1)||!dvc1394_ports[m].mapped)continue;
	/* Filter out packets that aren't for the requested channel */
	if(dvc1394_ports[m].channel != packet_channel){
		continue;
	}
	dvc1394_deliver_packet(m,
			       datap,
			       chan,
			       packet_valid,
			       packet_type);
    }

    chan->packet_offset += 60;
    chan->packet_sequence++;
}

static void
dvc1394_deliver_packet(int portnum,
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

	if(dvc1394_ports[portnum].curbufp) {
	    dvc1394_ports[portnum].curbufp->valid=0;
	    dvc1394_ports[portnum].curbufp->ntsc=chan->ntsc;
	    dvc1394_ports[portnum].curbufp=0;
    	    MUTEX_LOCK(&dvc1394_port_lock,-1);
	    dvc1394_ports[portnum].tail++;
	    if(dvc1394_ports[portnum].tail >= dvc1394_ports[portnum].nbufs)
		dvc1394_ports[portnum].tail=0;
	    if(dvc1394_ports[portnum].tail == dvc1394_ports[portnum].head)
		dvc1394_ports[portnum].full=1;
	    if(dvc1394_ports[portnum].waiting){
		dvc1394_ports[portnum].waiting=0;
		SV_BROADCAST(&dvc1394_port_sv);
	    }
	    ASSERT(portnum >=0 && portnum <MAX_DVC1394_PORTS);
	    pollwakeup(&dvc1394_ports[portnum].pq,POLLIN | POLLOUT | POLLRDNORM);
            MUTEX_UNLOCK(&dvc1394_port_lock);
	}

	/* If a new buf is available, open it */

	if(!dvc1394_ports[portnum].full) {
	    int tail;
	    tail=dvc1394_ports[portnum].tail;
	    dvc1394_ports[portnum].curbufp = &(dvc1394_ports[portnum].bufpool_kernaddr[tail]);
	    dvc1394_ports[portnum].curbufp->valid=1;
	}
    }

    /* Transfer the packet into the buffer */

    if(dvc1394_ports[portnum].curbufp) {
        if(packet_valid) {
            bcopy(
	        (void *)&datap[2],
		&((dvc1394_ports[portnum].curbufp->frame)[packet_offset]),
		480);
        } else {
	    dvc1394_ports[portnum].curbufp->valid=0;
        }
    }

    if(packet_type==END_PACKET) {
	if(dvc1394_ports[portnum].curbufp) {
	    dvc1394_ports[portnum].curbufp->ntsc=chan->ntsc;
	    dvc1394_ports[portnum].curbufp=0;
    	    MUTEX_LOCK(&dvc1394_port_lock,-1);
	    dvc1394_ports[portnum].tail++;
	    if(dvc1394_ports[portnum].tail >= dvc1394_ports[portnum].nbufs)
		dvc1394_ports[portnum].tail=0;
	    if(dvc1394_ports[portnum].tail == dvc1394_ports[portnum].head)
		dvc1394_ports[portnum].full=1;
	    if(dvc1394_ports[portnum].waiting){
		dvc1394_ports[portnum].waiting=0;
		SV_BROADCAST(&dvc1394_port_sv);
	    }
	    ASSERT(portnum >=0 && portnum <=MAX_DVC1394_PORTS);
	    pollwakeup(&dvc1394_ports[portnum].pq,POLLIN | POLLOUT | POLLRDNORM);
            MUTEX_UNLOCK(&dvc1394_port_lock);
	}
    }
}
