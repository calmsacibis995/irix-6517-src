    char *boardbuf=0;
/* This file is the ieee1394 lower-level driver file that supports
   the TI-LYNX chipset. */

#include "ti_lynx.h"

/* Function prototypes */

/* PCI infra structure interface functions */
__int32_t ti_lynx_attach (vertex_hdl_t vertex);
__int32_t ti_lynx_detach ( vertex_hdl_t vertex);
static error_handler_f ti_lynx_pcierror;

void    ti_lynx_intr(eframe_t *, void *);
int     ti_lynx_edtinit (void);
int     ti_lynx_reg(void);
void    ti_lynx_unreg(void);
int     ti_lynx_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
void    ti_lynx_halt(void);
int     ti_lynx_ioctl(dev_t dev, int cmd, void *arg, int mode, 
		      cred_t *crp, int *rvalp);
int     ti_lynx_map(dev_t dev, vhandl_t *vt, off_t off, int len, int prot);

static int  ti_lynx_setup(void);
static int  ti_lynx_kvtopci(caddr_t vaddr,int nbytes,int *pciaddrs,int step);
static void ti_lynx_setup_isorcv(void);
void ti_lynx_setup_isoxmt(void);		/* XXX Should be static */
static void ti_lynx_setup_sidrcv(void);
static void ti_lynx_setup_asyrcv(void);
static void ti_lynx_setup_asyxmt(int xferlen);
static void ti_lynx_start_isorcv(void);
void ti_lynx_start_isoxmt(void);		/* XXX Should be static */
static void ti_lynx_start_sidrcv(void);
static void ti_lynx_start_asyrcv(void);
static void ti_lynx_start_asyxmt(void);
static void ti_lynx_stop_isorcv(void);
void ti_lynx_stop_isoxmt(void);			/* XXX Should be static */
void ti_lynx_sendpacket(int slot, char *pkt, int len, int chan); /* XXX Should be static */
void ti_lynx_getslotcycle(int slot, int* cyc);  /* XXX Should be static */
static void ti_lynx_stop_sidrcv(void);
static void ti_lynx_stop_asyrcv(void);
static void ti_lynx_stop_asyxmt(void);
static void ti_lynx_init_lynx_regs(void);
static void ti_lynx_start_ll(void);
static void ti_lynx_stop_ll(void);
static int  ti_lynx_ReadPhyReg(int regnum);
static void ti_lynx_WritePhyReg(int regnum, int val);
static void ti_lynx_monitor_interrupts(void);
void 	    ti_lynx_link_request(ieee1394_transaction_t *t);
int  	    ti_lynx_get_headerlist_info(int op, int index, int **hdr, 
		int **cyclecounter, int **len, int **datap);
static int  ti_lynx_get_isorcv_info(int *tailp);
static int  ti_lynx_get_isoxmt_info(int *headp);
void 	    ti_lynx_ReadOurSelfIDInfo(int *phy_ID,int *gap_cnt,int *sp,int *c,
                                      signed char port_status[27]);
void 	    ieee1394_shared_init(void);
void 	    ieee1394_interrupt(int op, int *param, void (*funp)(int param));
static void ti_lynx_post_interrupt(int op);


/* XXX: Functions that can be removed later on. Only here
   	for debugging purposes. */
int  ti_lynx_readreg(int offset);
void ti_lynx_writereg(int offset, int val);
int  ti_lynx_get_mmapsize(void);
void ti_lynx_ioctl_test_isoxmt(void *arg);

/* XXX these shouldn't be global */
int referenceslot;
int referencecycle;



/******************************************************************************
   Device Driver Entry Points
******************************************************************************/

/*
 * ti_lynx_edtinit()
 *
 * Description:
 * Called at system startup time.
 * It must probe for all working TI PCI LYNX ieee1394 boards and set up the
 * necessary data structures.
 * Only supports one adapter at this point.
 */


int
ti_lynx_edtinit (void)
{
    int ret;

    /* Tell the system what boards I am responsible for */
    ret=pciio_driver_register(TI_LYNX_VNDID, TI_LYNX_DEVID, "ti_lynx_", 0);
    if(ret) {
        cmn_err(CE_WARN,"error %d registering devid %d",ret,TI_LYNX_DEVID);
        return ret;
    }
    return ret;
}

static int
ti_lynx_setup(void)
{
    volatile ti_lynx_t *ti_lynx;
    int i, j;

    initnsema(&ti_lynx_state_ptr->Physema,1,"Physema");
    
    /* Make sure that the shared variables are initialized */
    ieee1394_shared_init();

    /* Get the list of PCI addresses for the mmapped pages */
    i = ti_lynx_kvtopci(ti_lynx_state_ptr->mappages, NMAPPAGES*4096, 
			ti_lynx_state_ptr->pciaddrs, 4);
    /* XXX handle failure case, i<=0 */

    /* Workaround as per "Texas Instruments TSB12LV21PGF PCILynx Errata,
       Revision 6, Last Updated 9/18/96", errata #3 */
    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx->MiscControl=0xc0;
    /* End Workaround */

    /* Set up the lynx registers - ie get them into a known quiescent state */
    ti_lynx_init_lynx_regs();
{
        volatile uint *pci_config_reg = (uint *) PHYS_TO_K1(PCI_CONTROL);
        *pci_config_reg = PCI_CONFIG_BITS;
}

    /* Start up the receive dmas */

    ti_lynx_setup_isorcv(); ti_lynx_start_isorcv();
    ti_lynx_setup_sidrcv(); ti_lynx_start_sidrcv();
    ti_lynx_setup_asyrcv(); ti_lynx_start_asyrcv();

    /* Start up the link layer */
    ti_lynx_start_ll();

    /* Register the ieee1394-callback functions to call this driver */
    board_driver_readreg=ti_lynx_readreg;
    board_driver_writereg=ti_lynx_writereg;
    board_driver_get_mmapsize=ti_lynx_get_mmapsize;
    board_driver_link_request=ti_lynx_link_request;
    board_driver_get_headerlist_info=ti_lynx_get_headerlist_info;
    board_driver_get_isorcv_info=ti_lynx_get_isorcv_info;
    board_driver_get_isoxmt_info=ti_lynx_get_isoxmt_info;
    board_driver_ReadOurSelfIDInfo=ti_lynx_ReadOurSelfIDInfo;

    /* XXX: experimental only. Please delete */
    board_driver_ioctl_test_isoxmt=ti_lynx_ioctl_test_isoxmt;

    /* Reset the ieee1394 serial bus */
    psema(&ti_lynx_state_ptr->Physema,PZERO);
    ti_lynx_WritePhyReg(1,0x40);
    vsema(&ti_lynx_state_ptr->Physema);

    /* Monitor for runaway interrupts */
    itimeout(ti_lynx_monitor_interrupts,0,100,splhi,0,0,0);

    return (0);
}

void
ti_lynx_unreg(void)
{
    pciio_driver_unregister("ti_lynx_");
}

int
ti_lynx_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    printf("TI_LYNX_OPEN called.\n");
    return(0);
}

void
ti_lynx_halt(void)
{
    /* Stop the link layer */
    ti_lynx_stop_ll();
    /* Stop the dmas */
    ti_lynx_stop_isorcv();
    ti_lynx_stop_isoxmt();
    ti_lynx_stop_sidrcv();
    ti_lynx_stop_asyrcv();
}

int
ti_lynx_readreg(int offset)
{
    int *p;
    int val;

    p=(int *)ti_lynx_state_ptr->p_mem0;
    if(p){
        p = (int *)( (char *)p + offset);
        val = *p;
    }
    return (val);
}

void
ti_lynx_writereg(int offset, int val)
{ 
    int *p;

    p=(int *)ti_lynx_state_ptr->p_mem0;
    if(p){
        p = (int *)( (char *)p + offset);
        *p = val;
    }
}

int
ti_lynx_get_mmapsize(void)
{
  return (NMAPPAGES*4096); 
}

void
ti_lynx_ioctl_test_isoxmt(void *arg)
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int t0,t1,t2,t3;
    int    d1,d2,d3;
    caddr_t carg;

    boardbuf=0;
    if(arg) {
        boardbuf=(char *)kern_malloc(144000);
        if(boardbuf==0){
	    printf("Can't malloc 144000\n");
	    return;
        }

	carg=(caddr_t)arg;
        copyin(carg,boardbuf,144000);
	printf("copyin succeeded\n");
        ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
        ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;
        ti_lynx_stop_isoxmt();
        ti_lynx_setup_isoxmt();
        ti_lynx_start_isoxmt();
	delay(200);
	kern_free(boardbuf);
	printf("kern_free succeeded\n");
	return;
    }

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    ti_lynx_stop_isoxmt();
    ti_lynx_setup_isoxmt();
    ti_lynx_start_isoxmt();

    delay(1);
    dki_dcache_inval((caddr_t)&ti_lynx_mem->isoxmt.HeadP,4);
    t0=ti_lynx_mem->isoxmt.HeadP;
    delay(1);
    dki_dcache_inval((caddr_t)&ti_lynx_mem->isoxmt.HeadP,4);
    t1=ti_lynx_mem->isoxmt.HeadP;
    delay(1);
    dki_dcache_inval((caddr_t)&ti_lynx_mem->isoxmt.HeadP,4);
    t2=ti_lynx_mem->isoxmt.HeadP;
    delay(1);
    dki_dcache_inval((caddr_t)&ti_lynx_mem->isoxmt.HeadP,4);
    t3=ti_lynx_mem->isoxmt.HeadP;

    d1=t1-t0; if(d1<0)d1+=ISOXMT_NUMDATAPACKETS;
    d2=t2-t1; if(d2<0)d2+=ISOXMT_NUMDATAPACKETS;
    d3=t3-t2; if(d3<0)d3+=ISOXMT_NUMDATAPACKETS;

    printf("    HeadP %d\n",t0);
    printf("    HeadP %d delta %d\n",t1,d1);
    printf("    HeadP %d delta %d\n",t2,d2);
    printf("    HeadP %d delta %d\n",t3,d3);

    ti_lynx_stop_isoxmt();
}


void
ti_lynx_ReadOurSelfIDInfo(
    int *phy_ID,
    int *gap_cnt,
    int *sp,
    int *c,
    signed char port_status[27] )
{
    int t,l;
    volatile ti_lynx_t* ti_lynx;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    psema(&ti_lynx_state_ptr->Physema,PZERO);
    t=ti_lynx_ReadPhyReg(0);
    *phy_ID=t>>2;
    t=ti_lynx_ReadPhyReg(1);
    *gap_cnt=t&0x3f;
    t=ti_lynx_ReadPhyReg(2);
    *sp=t>>6;
    t=ti_lynx_ReadPhyReg(6);
    *c=t&1;
    t=ti_lynx_ReadPhyReg(3);
    if(t&0x4) {
        if(t&8)
            port_status[0]=CONNECTED_TO_CHILD;
        else
            port_status[0]=CONNECTED_TO_PARENT;
    } else {
            port_status[0]=CONNECTED_TO_NOTHING;
    }
    t=ti_lynx_ReadPhyReg(4);
    if(t&0x4) {
        if(t&8)
            port_status[1]=CONNECTED_TO_CHILD;
        else
            port_status[1]=CONNECTED_TO_PARENT;
    } else {
            port_status[1]=CONNECTED_TO_NOTHING;
    }
    t=ti_lynx_ReadPhyReg(5);
    vsema(&ti_lynx_state_ptr->Physema);
    if(t&0x4) {
        if(t&8)
            port_status[2]=CONNECTED_TO_CHILD;
        else
            port_status[2]=CONNECTED_TO_PARENT;
    } else {
            port_status[2]=CONNECTED_TO_NOTHING;
    }
    for(l=3;l<27;l++)port_status[l]=NOT_PRESENT;
}


static void
ti_lynx_setup_isorcv()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i,nexti;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx_stop_isorcv();

    /* Initialize the variables used by the PCL list */

    ti_lynx_mem->isorcv.StartPCLAddress=PCI_ADDR(isorcv.pcl[0]);
    ti_lynx_mem->isorcv.TailP = -1;
    ti_lynx_mem->isoxmt.HeadP = -1;
    ti_lynx_mem->isorcv.WorkListP = PCI_ADDR(isorcv.worklist[0]);

    /* Initialize the work list */

    nexti=0;
    for(i=0; i<ISORCV_NUMDATAPACKETS;i++) {
        nexti++;
        if(nexti==ISORCV_NUMDATAPACKETS)nexti=0;
        ti_lynx_mem->isorcv.worklist[i].hdrloc=PCI_ADDR(isorcv.headerlist[i]);
        ti_lynx_mem->isorcv.worklist[i].dataloc=PCI_ADDR(isorcv.datapages[i*512]);
        ti_lynx_mem->isorcv.worklist[i].index=i;
        ti_lynx_mem->isorcv.worklist[i].next=PCI_ADDR(isorcv.worklist[nexti]);
    }

    /* Build the PCL list */

    ti_lynx_mem->isorcv.pcl[0].next=PCI_ADDR(isorcv.pcl[1]);
    ti_lynx_mem->isorcv.pcl[0].error_next=1;
    ti_lynx_mem->isorcv.pcl[0].sw=0;
    ti_lynx_mem->isorcv.pcl[0].status=0;
    ti_lynx_mem->isorcv.pcl[0].remainder=0;
    ti_lynx_mem->isorcv.pcl[0].nextbuf=0;
    ti_lynx_mem->isorcv.pcl[0].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[0].buf_0=     PCI_ADDR(isorcv.WorkListP);
    ti_lynx_mem->isorcv.pcl[0].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[0].buf_1=     PCI_ADDR(isorcv.pcl[0].buf_8);
    ti_lynx_mem->isorcv.pcl[0].buf_2_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isorcv.pcl[0].buf_2=     4;
    ti_lynx_mem->isorcv.pcl[0].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[0].buf_3=     PCI_ADDR(isorcv.pcl[0].buf_a);
    ti_lynx_mem->isorcv.pcl[0].buf_4_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isorcv.pcl[0].buf_4=     4;
    ti_lynx_mem->isorcv.pcl[0].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[0].buf_5=     PCI_ADDR(isorcv.pcl[3].buf_2);
    ti_lynx_mem->isorcv.pcl[0].buf_6_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isorcv.pcl[0].buf_6=     4;
    ti_lynx_mem->isorcv.pcl[0].buf_7_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[0].buf_7=     PCI_ADDR(isorcv.pcl[3].buf_5);
    ti_lynx_mem->isorcv.pcl[0].buf_8_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[0].buf_8=     0; /* Filled in by isorcv.pcl[0].buf_1_ctl */
    ti_lynx_mem->isorcv.pcl[0].buf_9_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[0].buf_9=     PCI_ADDR(isorcv.pcl[1].buf_0);
    ti_lynx_mem->isorcv.pcl[0].buf_a_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[0].buf_a=     0; /* Filled in by isorcv.pcl[0].buf_3_ctl */
    ti_lynx_mem->isorcv.pcl[0].buf_b_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST;
    ti_lynx_mem->isorcv.pcl[0].buf_b=     PCI_ADDR(isorcv.pcl[1].buf_1);

    ti_lynx_mem->isorcv.pcl[1].next=PCI_ADDR(isorcv.pcl[2]);
    ti_lynx_mem->isorcv.pcl[1].error_next=1;
    ti_lynx_mem->isorcv.pcl[1].sw=0;
    ti_lynx_mem->isorcv.pcl[1].status=0;
    ti_lynx_mem->isorcv.pcl[1].remainder=0;
    ti_lynx_mem->isorcv.pcl[1].nextbuf=0;
    ti_lynx_mem->isorcv.pcl[1].buf_0_ctl= FW_XFER_CMD_RCV_AND_UPDATE |
                                FW_XFER_CMD_BIGENDIAN |
                                4;
    ti_lynx_mem->isorcv.pcl[1].buf_0=0; /* Filled in by isorcv.pcl[0].buf_9_ctl */
    ti_lynx_mem->isorcv.pcl[1].buf_1_ctl= FW_XFER_CMD_RCV_AND_UPDATE |
                                FW_XFER_CMD_LAST_BUF |
                                FW_XFER_CMD_BIGENDIAN | 2048;
    ti_lynx_mem->isorcv.pcl[1].buf_1=0; /* Filled in by isorcv.pcl[0].buf_b_ctl */

    ti_lynx_mem->isorcv.pcl[2].next=PCI_ADDR(isorcv.pcl[3]);
    ti_lynx_mem->isorcv.pcl[2].error_next=0;
    ti_lynx_mem->isorcv.pcl[2].sw=0;
    ti_lynx_mem->isorcv.pcl[2].status=0;
    ti_lynx_mem->isorcv.pcl[2].remainder=0;
    ti_lynx_mem->isorcv.pcl[2].nextbuf=0;
    ti_lynx_mem->isorcv.pcl[2].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[2].buf_0=     PCI_ADDR_SWAPPED(isorcv.pcl[1].status);
    ti_lynx_mem->isorcv.pcl[2].buf_1_ctl= FW_AUX_CMD_COMPARE;
    ti_lynx_mem->isorcv.pcl[2].buf_1=     0x00100010;
    ti_lynx_mem->isorcv.pcl[2].buf_2_ctl= FW_AUX_CMD_BRANCH_DMAREADY;
    ti_lynx_mem->isorcv.pcl[2].buf_2=     PCI_ADDR(isorcv.pcl[1]);
    ti_lynx_mem->isorcv.pcl[2].buf_3_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[2].buf_3=     PCI_ADDR(isorcv.pcl[1].buf_0);
    ti_lynx_mem->isorcv.pcl[2].buf_4_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isorcv.pcl[2].buf_4=     4;
    ti_lynx_mem->isorcv.pcl[2].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[2].buf_5=     PCI_ADDR(isorcv.pcl[2].buf_b);
    ti_lynx_mem->isorcv.pcl[2].buf_6_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isorcv.pcl[2].buf_6=     4;
    ti_lynx_mem->isorcv.pcl[2].buf_7_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[2].buf_7=     PCI_ADDR(isorcv.pcl[3].buf_1);
    ti_lynx_mem->isorcv.pcl[2].buf_8_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isorcv.pcl[2].buf_8=     4;
    ti_lynx_mem->isorcv.pcl[2].buf_9_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[2].buf_9=     PCI_ADDR(isorcv.pcl[3].buf_3);
    ti_lynx_mem->isorcv.pcl[2].buf_a_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[2].buf_a=      ((int)kvtophys((caddr_t)&(ti_lynx->CycleTimer)))-0x1a000000+0x80000000;
    ti_lynx_mem->isorcv.pcl[2].buf_b_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST       ;
    ti_lynx_mem->isorcv.pcl[2].buf_b=     0; /* Filled in by isorcv.pcl[2].buf_5_ctl */

    ti_lynx_mem->isorcv.pcl[3].next=PCI_ADDR(isorcv.pcl[0]);
    ti_lynx_mem->isorcv.pcl[3].error_next=0;
    ti_lynx_mem->isorcv.pcl[3].sw=0;
    ti_lynx_mem->isorcv.pcl[3].status=0;
    ti_lynx_mem->isorcv.pcl[3].remainder=0;
    ti_lynx_mem->isorcv.pcl[3].nextbuf=0;
    ti_lynx_mem->isorcv.pcl[3].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[3].buf_0=     PCI_ADDR(isorcv.pcl[1].status);
    ti_lynx_mem->isorcv.pcl[3].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[3].buf_1=     0; /* Filled in by isorcv.pcl[2].buf_7_ctl */
    ti_lynx_mem->isorcv.pcl[3].buf_2_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[3].buf_2=     0; /* Filled in by isorcv.pcl[0].buf_5_ctl */
    ti_lynx_mem->isorcv.pcl[3].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[3].buf_3=     0; /* Filled in by isorcv.pcl[2].buf_9_ctl */
    ti_lynx_mem->isorcv.pcl[3].buf_4_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isorcv.pcl[3].buf_4=     PCI_ADDR(isorcv.TailP);
    ti_lynx_mem->isorcv.pcl[3].buf_5_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isorcv.pcl[3].buf_5=     0; /* Filled in by isorcv.pcl[0].buf_7_ctl */
    ti_lynx_mem->isorcv.pcl[3].buf_6_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST       |
                                       FW_AUX_CMD_INT;
    ti_lynx_mem->isorcv.pcl[3].buf_6=     PCI_ADDR(isorcv.WorkListP);

    /* Write it back */

    dki_dcache_wb   ((caddr_t)&ti_lynx_mem->isorcv,sizeof(ti_lynx_mem->isorcv));
    dki_dcache_inval((caddr_t)&ti_lynx_mem->isorcv,sizeof(ti_lynx_mem->isorcv));

    ti_lynx->dmaregs1[FW_ISORCV_DMA_CHAN].DMA_PCLAddress =
        PCI_ADDR(isorcv.StartPCLAddress);
}

/* static */ void 	/* XXX Should be static */
ti_lynx_setup_isoxmt()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i,nexti;
    int chan,tag;
    static int pkt[2]={0x11223344,0xffeeddcc};

    chan=63;
    tag=1;  /* tag=1 implies the presence of a CIP header */

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* Initialize the variables used by the PCL list */

    ti_lynx_mem->isoxmt.StartPCLAddress=PCI_ADDR(isoxmt.pcl[0]);
    ti_lynx_mem->isoxmt.HeadP = -1;
    ti_lynx_mem->isoxmt.XmitListP = PCI_ADDR(isoxmt.xmitlist[0]);

    /* Initialize the xmit list */

    nexti=0;
    for(i=0; i<ISOXMT_NUMDATAPACKETS;i++) {
        nexti++;
        if(nexti==ISOXMT_NUMDATAPACKETS)nexti=0;
        ti_lynx_sendpacket(i, (char *)pkt, 8, chan);
        ti_lynx_mem->isoxmt.xmitlist[i].dataloc=PCI_ADDR(isoxmt.datapages[i*512]);
        ti_lynx_mem->isoxmt.xmitlist[i].index=i;
        ti_lynx_mem->isoxmt.xmitlist[i].next=PCI_ADDR(isoxmt.xmitlist[nexti]);
    }

    /* Build the PCL list */
    ti_lynx_mem->isoxmt.pcl[0].next=PCI_ADDR(isoxmt.pcl[1]);
    ti_lynx_mem->isoxmt.pcl[0].error_next=1;
    ti_lynx_mem->isoxmt.pcl[0].sw=0xffffffff;
    ti_lynx_mem->isoxmt.pcl[0].status=0;
    ti_lynx_mem->isoxmt.pcl[0].remainder=0;
    ti_lynx_mem->isoxmt.pcl[0].nextbuf=0;
    ti_lynx_mem->isoxmt.pcl[0].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_0=     PCI_ADDR(isoxmt.XmitListP);
    ti_lynx_mem->isoxmt.pcl[0].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_1=     PCI_ADDR(isoxmt.pcl[0].buf_8);
    ti_lynx_mem->isoxmt.pcl[0].buf_2_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isoxmt.pcl[0].buf_2=     4;
    ti_lynx_mem->isoxmt.pcl[0].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_3=     PCI_ADDR(isoxmt.pcl[0].buf_a);
    ti_lynx_mem->isoxmt.pcl[0].buf_4_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isoxmt.pcl[0].buf_4=     4;
    ti_lynx_mem->isoxmt.pcl[0].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_5=     PCI_ADDR(isoxmt.pcl[2].buf_0);
    ti_lynx_mem->isoxmt.pcl[0].buf_6_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isoxmt.pcl[0].buf_6=     4;
    ti_lynx_mem->isoxmt.pcl[0].buf_7_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_7=     PCI_ADDR(isoxmt.pcl[2].buf_2);
    ti_lynx_mem->isoxmt.pcl[0].buf_8_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_8=     0; /* Filled in by isoxmt.pcl[0].buf_1_ctl */
    ti_lynx_mem->isoxmt.pcl[0].buf_9_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_9=     PCI_ADDR(isoxmt.pcl[1].buf_0_ctl);
    ti_lynx_mem->isoxmt.pcl[0].buf_a_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_a=     0; /* Filled in by isoxmt.pcl[0].buf_3_ctl */
    ti_lynx_mem->isoxmt.pcl[0].buf_b_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST;
    ti_lynx_mem->isoxmt.pcl[0].buf_b=     PCI_ADDR(isoxmt.pcl[1].buf_0);


    ti_lynx_mem->isoxmt.pcl[1].next=PCI_ADDR(isoxmt.pcl[2]);
    ti_lynx_mem->isoxmt.pcl[1].error_next=1;
    ti_lynx_mem->isoxmt.pcl[1].sw=0;
    ti_lynx_mem->isoxmt.pcl[1].status=0;
    ti_lynx_mem->isoxmt.pcl[1].remainder=0;
    ti_lynx_mem->isoxmt.pcl[1].nextbuf=0;
    ti_lynx_mem->isoxmt.pcl[1].buf_0_ctl= 0; /* Filled in by isoxmt.pcl[0].buf_9_ctl */
    ti_lynx_mem->isoxmt.pcl[1].buf_0=     0; /* Filled in by isoxmt.pcl[0].buf_a_ctl */

    ti_lynx_mem->isoxmt.pcl[2].next=PCI_ADDR(isoxmt.pcl[0]);
    ti_lynx_mem->isoxmt.pcl[2].error_next=1;
    ti_lynx_mem->isoxmt.pcl[2].sw=0;
    ti_lynx_mem->isoxmt.pcl[2].status=0;
    ti_lynx_mem->isoxmt.pcl[2].remainder=0;
    ti_lynx_mem->isoxmt.pcl[2].nextbuf=0;
    ti_lynx_mem->isoxmt.pcl[2].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_0=     0; /* Filled in by isoxmt.pcl[0].buf_5_ctl */
    ti_lynx_mem->isoxmt.pcl[2].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_1=     PCI_ADDR(isoxmt.HeadP);
    ti_lynx_mem->isoxmt.pcl[2].buf_2_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_2=     0; /* Filled in by isoxmt.pcl[0].buf_7_ctl */
    ti_lynx_mem->isoxmt.pcl[2].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_3=     PCI_ADDR(isoxmt.XmitListP);
    ti_lynx_mem->isoxmt.pcl[2].buf_4_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_4=      ((int)kvtophys((caddr_t)&(ti_lynx->CycleTimer)))-0x1a000000+0x80000000;
    ti_lynx_mem->isoxmt.pcl[2].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_5=     PCI_ADDR(isoxmt.pcl[0].sw);
    ti_lynx_mem->isoxmt.pcl[2].buf_6_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_6=     PCI_ADDR(isoxmt.pcl[2].buf_8_ctl);
    ti_lynx_mem->isoxmt.pcl[2].buf_7_ctl= FW_AUX_CMD_STORE_QUAD |  FW_AUX_CMD_LAST;
    ti_lynx_mem->isoxmt.pcl[2].buf_7=     PCI_ADDR(isoxmt.pcl[2].buf_3_ctl);
    ti_lynx_mem->isoxmt.pcl[2].buf_8_ctl= FW_AUX_CMD_STORE_QUAD | FW_AUX_CMD_LAST;
    ti_lynx_mem->isoxmt.pcl[2].buf_8=     0;
    

    dki_dcache_wb   ((caddr_t)&ti_lynx_mem->isoxmt,sizeof(ti_lynx_mem->isoxmt));
    dki_dcache_inval((caddr_t)&ti_lynx_mem->isoxmt,sizeof(ti_lynx_mem->isoxmt));

    ti_lynx->dmaregs1[FW_ISOXMT_DMA_CHAN].DMA_PCLAddress =
        PCI_ADDR(isoxmt.StartPCLAddress);

}

void ti_lynx_getslotcycle(int slot, int* cyc)
{
    int deltaslot,cycle;
    int ref0, ref1, ref2;
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    deltaslot=slot-referenceslot;
    if(deltaslot<0)deltaslot+=ISOXMT_NUMDATAPACKETS;
    ref0=referencecycle&0xfff;
    ref1=(referencecycle>>12)&0x1fff;
    ref2=(referencecycle>>25)&0x7f;
    ref1+=deltaslot;
    if(ref1>=8000){
	ref1-=8000;
        ref2++;
    }
    cycle=(ref2<<25)|(ref1<<12)|ref0;
    referenceslot=slot;
    referencecycle=cycle;
    *cyc=cycle;

#ifdef LATER
    int slot1, slot2, cycle, deltaslot;
    while(1) {
	dki_dcache_wb((caddr_t)&ti_lynx_mem->isoxmt.HeadP,4);
	slot1=ti_lynx_mem->isoxmt.HeadP;
	dki_dcache_wb((caddr_t)&ti_lynx_mem->isoxmt.HeadP,4);
	cycle=ti_lynx->CycleTimer;
	slot2=ti_lynx_mem->isoxmt.HeadP;
	if(slot1==slot2)break;
    }
    deltaslot=slot-slot1;
    if(deltaslot<0)deltaslot+=ISOXMT_NUMDATAPACKETS;
    *cyc=cycle+(deltaslot<<12);
{
    static cnt=0;
    if(cnt==0)printf("slot %d slot1 %d deltaslot %d cycle 0x%x *cyc 0x%x\n",
		slot,slot1,deltaslot,cycle,*cyc);
    cnt++;
    if(cnt==100)cnt=0;
}
#endif
}

/* ti_lynx_sendpacket - puts a packet into the isoxmt ring buffer */

/* static */ void 	/* XXX Should be static */
ti_lynx_sendpacket(int slot, char *pkt, int len, int chan)
{
    int tag;
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;

    tag=1;  /* I don't know why this should be 1, but that's what the
	       sony vcr sends, so we do the same thing for compatibility */

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* Setup Transmit PCL command field */

    ti_lynx_mem->isoxmt.xmitlist[slot].cmd=
        FW_XFER_CMD_XMT|
        FW_XFER_CMD_LAST_BUF|
        FW_XFER_CMD_ISO|
        FW_XFER_CMD_BIGENDIAN|
        FW_XFER_CMD_100MBPS|
        (len+4);

    /* Place Packet Header Into Ring Buffer Memory */

    ti_lynx_mem->isoxmt.datapages[slot*512]=len<<16|tag<<14|chan<<8|0xa<<4;
    dki_dcache_wb   ((caddr_t)&(ti_lynx_mem->isoxmt.xmitlist[slot].cmd),4);
    dki_dcache_inval((caddr_t)&(ti_lynx_mem->isoxmt.xmitlist[slot].cmd),4);

    /* Place Packet Into Ring Buffer Memory */

    bcopy(pkt,(void *)&(ti_lynx_mem->isoxmt.datapages[slot*512+1]),len);
    dki_dcache_wb   ((caddr_t)&(ti_lynx_mem->isoxmt.datapages[slot*512+1]),len);
    dki_dcache_inval((caddr_t)&(ti_lynx_mem->isoxmt.datapages[slot*512+1]),len);
}

static void
ti_lynx_setup_isoxmt_test()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i, nexti;
    int inslot, outslot, lastinslot;
    int chan,tag,framenum;

    chan=63;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx_stop_isoxmt();

    /* Initialize the variables used by the PCL list */
    ti_lynx_mem->isoxmt.StartPCLAddress=PCI_ADDR(isoxmt.pcl[0]);
    ti_lynx_mem->isoxmt.HeadP = -1;
    ti_lynx_mem->isoxmt.XmitListP = PCI_ADDR(isoxmt.xmitlist[0]);

    /* Initialize the xmit list */
    if(!boardbuf) {
    nexti=0;
    for(i=0; i<ISOXMT_NUMDATAPACKETS;i++) {
        nexti++;
        if(nexti==ISOXMT_NUMDATAPACKETS)nexti=0;
        ti_lynx_mem->isoxmt.xmitlist[i].cmd=
            FW_XFER_CMD_XMT|
            FW_XFER_CMD_LAST_BUF|
            FW_XFER_CMD_ISO|
            FW_XFER_CMD_BIGENDIAN|
            FW_XFER_CMD_100MBPS|
            12;
        ti_lynx_mem->isoxmt.xmitlist[i].dataloc=PCI_ADDR(isoxmt.datapages[i*512]);
        ti_lynx_mem->isoxmt.xmitlist[i].index=i;
        ti_lynx_mem->isoxmt.xmitlist[i].next=PCI_ADDR(isoxmt.xmitlist[nexti]);
        ti_lynx_mem->isoxmt.datapages[i*512]=8<<16|tag<<14|chan<<8|0xa<<4;
        ti_lynx_mem->isoxmt.datapages[i*512+1]=0x11223344;
        ti_lynx_mem->isoxmt.datapages[i*512+2]=0xffeeddcc;
    }
    } else {
    nexti=0;
    lastinslot = -1;
    framenum=0;
    for(i=0; i<ISOXMT_NUMDATAPACKETS;i++) {
        nexti++;
        if(nexti==ISOXMT_NUMDATAPACKETS)nexti=0;
	outslot=i%320;
	inslot=outslot*300/320;
	if(inslot == lastinslot) {
            /* xxx send a placeholder */
            ti_lynx_mem->isoxmt.xmitlist[i].cmd=
                FW_XFER_CMD_XMT|
                FW_XFER_CMD_LAST_BUF|
                FW_XFER_CMD_ISO|
                FW_XFER_CMD_BIGENDIAN|
                FW_XFER_CMD_100MBPS|
                12;
            ti_lynx_mem->isoxmt.datapages[i*512]=8<<16|tag<<14|chan<<8|0xa<<4;
            ti_lynx_mem->isoxmt.datapages[i*512+1]=0x01780000+inslot;
            ti_lynx_mem->isoxmt.datapages[i*512+2]=0x8080ffff;
        } else {
            /* xxx send a packet */
            ti_lynx_mem->isoxmt.xmitlist[i].cmd=
                FW_XFER_CMD_XMT|
                FW_XFER_CMD_LAST_BUF|
                FW_XFER_CMD_ISO|
                FW_XFER_CMD_BIGENDIAN|
                FW_XFER_CMD_100MBPS|
                480+12;
            ti_lynx_mem->isoxmt.datapages[i*512]=488<<16|tag<<14|chan<<8|0xa<<4;
            ti_lynx_mem->isoxmt.datapages[i*512+1]=0x01780000+inslot;
	    if(inslot==0) {
                ti_lynx_mem->isoxmt.datapages[i*512+2]=0x80800000;
		framenum++;
	    } else {
                ti_lynx_mem->isoxmt.datapages[i*512+2]=0x8080ffff;
	    }
	    bcopy(&(boardbuf[480*inslot]),(void *)&(ti_lynx_mem->isoxmt.datapages[i*512+3]),480);
	    lastinslot=inslot;
	}
        ti_lynx_mem->isoxmt.xmitlist[i].dataloc=PCI_ADDR(isoxmt.datapages[i*512]);
        ti_lynx_mem->isoxmt.xmitlist[i].index=i;
        ti_lynx_mem->isoxmt.xmitlist[i].next=PCI_ADDR(isoxmt.xmitlist[nexti]);
    }
    }

    /* Build the PCL list */
    ti_lynx_mem->isoxmt.pcl[0].next=PCI_ADDR(isoxmt.pcl[1]);
    ti_lynx_mem->isoxmt.pcl[0].error_next=1;
    ti_lynx_mem->isoxmt.pcl[0].sw=0xffffffff;
    ti_lynx_mem->isoxmt.pcl[0].status=0;
    ti_lynx_mem->isoxmt.pcl[0].remainder=0;
    ti_lynx_mem->isoxmt.pcl[0].nextbuf=0;
    ti_lynx_mem->isoxmt.pcl[0].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_0=     PCI_ADDR(isoxmt.XmitListP);
    ti_lynx_mem->isoxmt.pcl[0].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_1=     PCI_ADDR(isoxmt.pcl[0].buf_8);
    ti_lynx_mem->isoxmt.pcl[0].buf_2_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isoxmt.pcl[0].buf_2=     4;
    ti_lynx_mem->isoxmt.pcl[0].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_3=     PCI_ADDR(isoxmt.pcl[0].buf_a);
    ti_lynx_mem->isoxmt.pcl[0].buf_4_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isoxmt.pcl[0].buf_4=     4;
    ti_lynx_mem->isoxmt.pcl[0].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_5=     PCI_ADDR(isoxmt.pcl[2].buf_0);
    ti_lynx_mem->isoxmt.pcl[0].buf_6_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->isoxmt.pcl[0].buf_6=     4;
    ti_lynx_mem->isoxmt.pcl[0].buf_7_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_7=     PCI_ADDR(isoxmt.pcl[2].buf_2);
    ti_lynx_mem->isoxmt.pcl[0].buf_8_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_8=     0; /* Filled in by isoxmt.pcl[0].buf_1_ctl */
    ti_lynx_mem->isoxmt.pcl[0].buf_9_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_9=     PCI_ADDR(isoxmt.pcl[1].buf_0_ctl);
    ti_lynx_mem->isoxmt.pcl[0].buf_a_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[0].buf_a=     0; /* Filled in by isoxmt.pcl[0].buf_3_ctl */
    ti_lynx_mem->isoxmt.pcl[0].buf_b_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST;
    ti_lynx_mem->isoxmt.pcl[0].buf_b=     PCI_ADDR(isoxmt.pcl[1].buf_0);


    ti_lynx_mem->isoxmt.pcl[1].next=PCI_ADDR(isoxmt.pcl[2]);
    ti_lynx_mem->isoxmt.pcl[1].error_next=1;
    ti_lynx_mem->isoxmt.pcl[1].sw=0;
    ti_lynx_mem->isoxmt.pcl[1].status=0;
    ti_lynx_mem->isoxmt.pcl[1].remainder=0;
    ti_lynx_mem->isoxmt.pcl[1].nextbuf=0;
    ti_lynx_mem->isoxmt.pcl[1].buf_0_ctl= 0; /* Filled in by isoxmt.pcl[0].buf_9_ctl */
    ti_lynx_mem->isoxmt.pcl[1].buf_0=     0; /* Filled in by isoxmt.pcl[0].buf_a_ctl */

    ti_lynx_mem->isoxmt.pcl[2].next=PCI_ADDR(isoxmt.pcl[0]);
    ti_lynx_mem->isoxmt.pcl[2].error_next=1;
    ti_lynx_mem->isoxmt.pcl[2].sw=0;
    ti_lynx_mem->isoxmt.pcl[2].status=0;
    ti_lynx_mem->isoxmt.pcl[2].remainder=0;
    ti_lynx_mem->isoxmt.pcl[2].nextbuf=0;
    ti_lynx_mem->isoxmt.pcl[2].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_0=     0; /* Filled in by isoxmt.pcl[0].buf_5_ctl */
    ti_lynx_mem->isoxmt.pcl[2].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_1=     PCI_ADDR(isoxmt.HeadP);
    ti_lynx_mem->isoxmt.pcl[2].buf_2_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->isoxmt.pcl[2].buf_2=     0; /* Filled in by isoxmt.pcl[0].buf_7_ctl */
    ti_lynx_mem->isoxmt.pcl[2].buf_3_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST;
    ti_lynx_mem->isoxmt.pcl[2].buf_3=     PCI_ADDR(isoxmt.XmitListP);

    dki_dcache_wb   ((caddr_t)&ti_lynx_mem->isoxmt,sizeof(ti_lynx_mem->isoxmt));
    dki_dcache_inval((caddr_t)&ti_lynx_mem->isoxmt,sizeof(ti_lynx_mem->isoxmt));

    ti_lynx->dmaregs1[FW_ISOXMT_DMA_CHAN].DMA_PCLAddress =
        PCI_ADDR(isoxmt.StartPCLAddress);

    printf("ti_lynx_setup_isoxmt_test done.\n");
}

static void
ti_lynx_start_isorcv()
{
    volatile ti_lynx_t *ti_lynx;
    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;

    /* Start the DMA */
    ti_lynx->dmaregs1[FW_ISORCV_DMA_CHAN].DMA_Control = 0xa0000000;
}

/* static */ void 	/* XXX Should be static */
ti_lynx_start_isoxmt()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int sw;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* Start the DMA */
    ti_lynx->dmaregs1[FW_ISOXMT_DMA_CHAN].DMA_Control = 0xa0000000;
    while(1) {
	dki_dcache_inval((caddr_t)&(ti_lynx_mem->isoxmt.pcl[0].sw),4);
	sw=ti_lynx_mem->isoxmt.pcl[0].sw;
	if(sw != 0xffffffff)break;
    }
    referenceslot=0;
    referencecycle=(sw&0xfffff000)+0x1000;
}

static void
ti_lynx_setup_sidrcv()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i,nexti;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx_stop_sidrcv();

    /* Initialize the variables used by the PCL list */

    ti_lynx_mem->sidrcv.StartPCLAddress=PCI_ADDR(sidrcv.pcl[0]);
    ti_lynx_mem->sidrcv.TailP = -1;
    ti_lynx_mem->sidrcv.WorkListP = PCI_ADDR(sidrcv.worklist[0]);

    /* Initialize the work list */

    nexti=0;
    for(i=0; i<SIDRCV_NUMDATAPACKETS;i++) {
        nexti++;
        if(nexti==SIDRCV_NUMDATAPACKETS)nexti=0;
        ti_lynx_mem->sidrcv.worklist[i].hdrloc=PCI_ADDR(sidrcv.headerlist[i]);
        ti_lynx_mem->sidrcv.worklist[i].dataloc=PCI_ADDR(sidrcv.datapages[i*512]);
        ti_lynx_mem->sidrcv.worklist[i].index=i;
        ti_lynx_mem->sidrcv.worklist[i].next=PCI_ADDR(sidrcv.worklist[nexti]);
    }

    /* Build the PCL list */

    ti_lynx_mem->sidrcv.pcl[0].next=PCI_ADDR(sidrcv.pcl[1]);
    ti_lynx_mem->sidrcv.pcl[0].error_next=1;
    ti_lynx_mem->sidrcv.pcl[0].sw=0;
    ti_lynx_mem->sidrcv.pcl[0].status=0;
    ti_lynx_mem->sidrcv.pcl[0].remainder=0;
    ti_lynx_mem->sidrcv.pcl[0].nextbuf=0;
    ti_lynx_mem->sidrcv.pcl[0].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[0].buf_0=     PCI_ADDR(sidrcv.WorkListP);
    ti_lynx_mem->sidrcv.pcl[0].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[0].buf_1=     PCI_ADDR(sidrcv.pcl[0].buf_8);
    ti_lynx_mem->sidrcv.pcl[0].buf_2_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->sidrcv.pcl[0].buf_2=     4;
    ti_lynx_mem->sidrcv.pcl[0].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[0].buf_3=     PCI_ADDR(sidrcv.pcl[0].buf_a);
    ti_lynx_mem->sidrcv.pcl[0].buf_4_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->sidrcv.pcl[0].buf_4=     4;
    ti_lynx_mem->sidrcv.pcl[0].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[0].buf_5=     PCI_ADDR(sidrcv.pcl[3].buf_2);
    ti_lynx_mem->sidrcv.pcl[0].buf_6_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->sidrcv.pcl[0].buf_6=     4;
    ti_lynx_mem->sidrcv.pcl[0].buf_7_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[0].buf_7=     PCI_ADDR(sidrcv.pcl[3].buf_5);
    ti_lynx_mem->sidrcv.pcl[0].buf_8_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[0].buf_8=     0; /* Filled in by sidrcv.pcl[0].buf_1_ctl */
    ti_lynx_mem->sidrcv.pcl[0].buf_9_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[0].buf_9=     PCI_ADDR(sidrcv.pcl[1].buf_0);
    ti_lynx_mem->sidrcv.pcl[0].buf_a_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[0].buf_a=     0; /* Filled in by sidrcv.pcl[0].buf_3_ctl */
    ti_lynx_mem->sidrcv.pcl[0].buf_b_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST;
    ti_lynx_mem->sidrcv.pcl[0].buf_b=     PCI_ADDR(sidrcv.pcl[1].buf_1);

    ti_lynx_mem->sidrcv.pcl[1].next=PCI_ADDR(sidrcv.pcl[2]);
    ti_lynx_mem->sidrcv.pcl[1].error_next=1;
    ti_lynx_mem->sidrcv.pcl[1].sw=0;
    ti_lynx_mem->sidrcv.pcl[1].status=0;
    ti_lynx_mem->sidrcv.pcl[1].remainder=0;
    ti_lynx_mem->sidrcv.pcl[1].nextbuf=0;
    ti_lynx_mem->sidrcv.pcl[1].buf_0_ctl= FW_XFER_CMD_RCV_AND_UPDATE |
                                FW_XFER_CMD_BIGENDIAN |
                                4;
    ti_lynx_mem->sidrcv.pcl[1].buf_0=0; /* Filled in by sidrcv.pcl[0].buf_9_ctl */
    ti_lynx_mem->sidrcv.pcl[1].buf_1_ctl= FW_XFER_CMD_RCV_AND_UPDATE |
                                FW_XFER_CMD_LAST_BUF |
                                FW_XFER_CMD_BIGENDIAN | 2048;
    ti_lynx_mem->sidrcv.pcl[1].buf_1=0; /* Filled in by sidrcv.pcl[0].buf_b_ctl */

    ti_lynx_mem->sidrcv.pcl[2].next=PCI_ADDR(sidrcv.pcl[3]);
    ti_lynx_mem->sidrcv.pcl[2].error_next=0;
    ti_lynx_mem->sidrcv.pcl[2].sw=0;
    ti_lynx_mem->sidrcv.pcl[2].status=0;
    ti_lynx_mem->sidrcv.pcl[2].remainder=0;
    ti_lynx_mem->sidrcv.pcl[2].nextbuf=0;
    ti_lynx_mem->sidrcv.pcl[2].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[2].buf_0=     PCI_ADDR_SWAPPED(sidrcv.pcl[1].status);
    ti_lynx_mem->sidrcv.pcl[2].buf_1_ctl= FW_AUX_CMD_COMPARE;
    ti_lynx_mem->sidrcv.pcl[2].buf_1=     0x00100010;
    ti_lynx_mem->sidrcv.pcl[2].buf_2_ctl= FW_AUX_CMD_BRANCH_DMAREADY;
    ti_lynx_mem->sidrcv.pcl[2].buf_2=     PCI_ADDR(sidrcv.pcl[1]);
    ti_lynx_mem->sidrcv.pcl[2].buf_3_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[2].buf_3=     PCI_ADDR(sidrcv.pcl[1].buf_0);
    ti_lynx_mem->sidrcv.pcl[2].buf_4_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->sidrcv.pcl[2].buf_4=     4;
    ti_lynx_mem->sidrcv.pcl[2].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[2].buf_5=     PCI_ADDR(sidrcv.pcl[2].buf_b);
    ti_lynx_mem->sidrcv.pcl[2].buf_6_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->sidrcv.pcl[2].buf_6=     4;
    ti_lynx_mem->sidrcv.pcl[2].buf_7_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[2].buf_7=     PCI_ADDR(sidrcv.pcl[3].buf_1);
    ti_lynx_mem->sidrcv.pcl[2].buf_8_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->sidrcv.pcl[2].buf_8=     4;
    ti_lynx_mem->sidrcv.pcl[2].buf_9_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[2].buf_9=     PCI_ADDR(sidrcv.pcl[3].buf_3);
    ti_lynx_mem->sidrcv.pcl[2].buf_a_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[2].buf_a=      ((int)kvtophys((caddr_t)&(ti_lynx->CycleTimer)))-0x1a000000+0x80000000;
    ti_lynx_mem->sidrcv.pcl[2].buf_b_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST       ;
    ti_lynx_mem->sidrcv.pcl[2].buf_b=     0; /* Filled in by sidrcv.pcl[2].buf_5_ctl */

    ti_lynx_mem->sidrcv.pcl[3].next=PCI_ADDR(sidrcv.pcl[0]);
    ti_lynx_mem->sidrcv.pcl[3].error_next=0;
    ti_lynx_mem->sidrcv.pcl[3].sw=0;
    ti_lynx_mem->sidrcv.pcl[3].status=0;
    ti_lynx_mem->sidrcv.pcl[3].remainder=0;
    ti_lynx_mem->sidrcv.pcl[3].nextbuf=0;
    ti_lynx_mem->sidrcv.pcl[3].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[3].buf_0=     PCI_ADDR(sidrcv.pcl[1].status);
    ti_lynx_mem->sidrcv.pcl[3].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[3].buf_1=     0; /* Filled in by sidrcv.pcl[2].buf_7_ctl */
    ti_lynx_mem->sidrcv.pcl[3].buf_2_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[3].buf_2=     0; /* Filled in by sidrcv.pcl[0].buf_5_ctl */
    ti_lynx_mem->sidrcv.pcl[3].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[3].buf_3=     0; /* Filled in by sidrcv.pcl[2].buf_9_ctl */
    ti_lynx_mem->sidrcv.pcl[3].buf_4_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->sidrcv.pcl[3].buf_4=     PCI_ADDR(sidrcv.TailP);
    ti_lynx_mem->sidrcv.pcl[3].buf_5_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->sidrcv.pcl[3].buf_5=     0; /* Filled in by sidrcv.pcl[0].buf_7_ctl */
    ti_lynx_mem->sidrcv.pcl[3].buf_6_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST       |
                                       FW_AUX_CMD_INT;
    ti_lynx_mem->sidrcv.pcl[3].buf_6=     PCI_ADDR(sidrcv.WorkListP);

    /* Write it back */

    dki_dcache_wb   ((caddr_t)&ti_lynx_mem->sidrcv,sizeof(ti_lynx_mem->sidrcv));
    dki_dcache_inval((caddr_t)&ti_lynx_mem->sidrcv,sizeof(ti_lynx_mem->sidrcv));


    ti_lynx->dmaregs1[FW_SIDRCV_DMA_CHAN].DMA_PCLAddress =
        PCI_ADDR(sidrcv.StartPCLAddress);
}

static void
ti_lynx_start_sidrcv()
{
    volatile ti_lynx_t *ti_lynx;
    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;

    /* Start the DMA */
    ti_lynx->dmaregs1[FW_SIDRCV_DMA_CHAN].DMA_Control = 0xa0000000;
}

static void
ti_lynx_setup_asyrcv()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i,nexti;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx_stop_asyrcv();

    /* Initialize the variables used by the PCL list */

    ti_lynx_mem->asyrcv.StartPCLAddress=PCI_ADDR(asyrcv.pcl[0]);
    ti_lynx_mem->asyrcv.TailP = -1;
    ti_lynx_mem->asyrcv.WorkListP = PCI_ADDR(asyrcv.worklist[0]);

    /* Initialize the work list */

    nexti=0;
    for(i=0; i<ASYRCV_NUMDATAPACKETS;i++) {
        nexti++;
        if(nexti==ASYRCV_NUMDATAPACKETS)nexti=0;
        ti_lynx_mem->asyrcv.worklist[i].hdrloc=PCI_ADDR(asyrcv.headerlist[i]);
        ti_lynx_mem->asyrcv.worklist[i].dataloc=PCI_ADDR(asyrcv.datapages[i*512]);
        ti_lynx_mem->asyrcv.worklist[i].index=i;
        ti_lynx_mem->asyrcv.worklist[i].next=PCI_ADDR(asyrcv.worklist[nexti]);
    }

    /* Build the PCL list */

    ti_lynx_mem->asyrcv.pcl[0].next=PCI_ADDR(asyrcv.pcl[1]);
    ti_lynx_mem->asyrcv.pcl[0].error_next=1;
    ti_lynx_mem->asyrcv.pcl[0].sw=0;
    ti_lynx_mem->asyrcv.pcl[0].status=0;
    ti_lynx_mem->asyrcv.pcl[0].remainder=0;
    ti_lynx_mem->asyrcv.pcl[0].nextbuf=0;
    ti_lynx_mem->asyrcv.pcl[0].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[0].buf_0=     PCI_ADDR(asyrcv.WorkListP);
    ti_lynx_mem->asyrcv.pcl[0].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[0].buf_1=     PCI_ADDR(asyrcv.pcl[0].buf_8);
    ti_lynx_mem->asyrcv.pcl[0].buf_2_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->asyrcv.pcl[0].buf_2=     4;
    ti_lynx_mem->asyrcv.pcl[0].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[0].buf_3=     PCI_ADDR(asyrcv.pcl[0].buf_a);
    ti_lynx_mem->asyrcv.pcl[0].buf_4_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->asyrcv.pcl[0].buf_4=     4;
    ti_lynx_mem->asyrcv.pcl[0].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[0].buf_5=     PCI_ADDR(asyrcv.pcl[3].buf_2);
    ti_lynx_mem->asyrcv.pcl[0].buf_6_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->asyrcv.pcl[0].buf_6=     4;
    ti_lynx_mem->asyrcv.pcl[0].buf_7_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[0].buf_7=     PCI_ADDR(asyrcv.pcl[3].buf_5);
    ti_lynx_mem->asyrcv.pcl[0].buf_8_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[0].buf_8=     0; /* Filled in by asyrcv.pcl[0].buf_1_ctl */
    ti_lynx_mem->asyrcv.pcl[0].buf_9_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[0].buf_9=     PCI_ADDR(asyrcv.pcl[1].buf_0);
    ti_lynx_mem->asyrcv.pcl[0].buf_a_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[0].buf_a=     0; /* Filled in by asyrcv.pcl[0].buf_3_ctl */
    ti_lynx_mem->asyrcv.pcl[0].buf_b_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST;
    ti_lynx_mem->asyrcv.pcl[0].buf_b=     PCI_ADDR(asyrcv.pcl[1].buf_1);

    ti_lynx_mem->asyrcv.pcl[1].next=PCI_ADDR(asyrcv.pcl[2]);
    ti_lynx_mem->asyrcv.pcl[1].error_next=1;
    ti_lynx_mem->asyrcv.pcl[1].sw=0;
    ti_lynx_mem->asyrcv.pcl[1].status=0;
    ti_lynx_mem->asyrcv.pcl[1].remainder=0;
    ti_lynx_mem->asyrcv.pcl[1].nextbuf=0;
    ti_lynx_mem->asyrcv.pcl[1].buf_0_ctl= FW_XFER_CMD_RCV_AND_UPDATE |
                                FW_XFER_CMD_BIGENDIAN |
                                4;
    ti_lynx_mem->asyrcv.pcl[1].buf_0=0; /* Filled in by asyrcv.pcl[0].buf_9_ctl */
    ti_lynx_mem->asyrcv.pcl[1].buf_1_ctl= FW_XFER_CMD_RCV_AND_UPDATE |
                                FW_XFER_CMD_LAST_BUF |
                                FW_XFER_CMD_BIGENDIAN | 2048;
    ti_lynx_mem->asyrcv.pcl[1].buf_1=0; /* Filled in by asyrcv.pcl[0].buf_b_ctl */

    ti_lynx_mem->asyrcv.pcl[2].next=PCI_ADDR(asyrcv.pcl[3]);
    ti_lynx_mem->asyrcv.pcl[2].error_next=0;
    ti_lynx_mem->asyrcv.pcl[2].sw=0;
    ti_lynx_mem->asyrcv.pcl[2].status=0;
    ti_lynx_mem->asyrcv.pcl[2].remainder=0;
    ti_lynx_mem->asyrcv.pcl[2].nextbuf=0;
    ti_lynx_mem->asyrcv.pcl[2].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[2].buf_0=     PCI_ADDR_SWAPPED(asyrcv.pcl[1].status);
    ti_lynx_mem->asyrcv.pcl[2].buf_1_ctl= FW_AUX_CMD_COMPARE;
    ti_lynx_mem->asyrcv.pcl[2].buf_1=     0x00100010;
    ti_lynx_mem->asyrcv.pcl[2].buf_2_ctl= FW_AUX_CMD_BRANCH_DMAREADY;
    ti_lynx_mem->asyrcv.pcl[2].buf_2=     PCI_ADDR(asyrcv.pcl[1]);
    ti_lynx_mem->asyrcv.pcl[2].buf_3_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[2].buf_3=     PCI_ADDR(asyrcv.pcl[1].buf_0);
    ti_lynx_mem->asyrcv.pcl[2].buf_4_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->asyrcv.pcl[2].buf_4=     4;
    ti_lynx_mem->asyrcv.pcl[2].buf_5_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[2].buf_5=     PCI_ADDR(asyrcv.pcl[2].buf_b);
    ti_lynx_mem->asyrcv.pcl[2].buf_6_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->asyrcv.pcl[2].buf_6=     4;
    ti_lynx_mem->asyrcv.pcl[2].buf_7_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[2].buf_7=     PCI_ADDR(asyrcv.pcl[3].buf_1);
    ti_lynx_mem->asyrcv.pcl[2].buf_8_ctl= FW_AUX_CMD_ADD;
    ti_lynx_mem->asyrcv.pcl[2].buf_8=     4;
    ti_lynx_mem->asyrcv.pcl[2].buf_9_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[2].buf_9=     PCI_ADDR(asyrcv.pcl[3].buf_3);
    ti_lynx_mem->asyrcv.pcl[2].buf_a_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[2].buf_a=      ((int)kvtophys((caddr_t)&(ti_lynx->CycleTimer)))-0x1a000000+0x80000000;
    ti_lynx_mem->asyrcv.pcl[2].buf_b_ctl= FW_AUX_CMD_STORE_QUAD |
                                       FW_AUX_CMD_LAST       ;
    ti_lynx_mem->asyrcv.pcl[2].buf_b=     0; /* Filled in by asyrcv.pcl[2].buf_5_ctl */

    ti_lynx_mem->asyrcv.pcl[3].next=PCI_ADDR(asyrcv.pcl[0]);
    ti_lynx_mem->asyrcv.pcl[3].error_next=0;
    ti_lynx_mem->asyrcv.pcl[3].sw=0;
    ti_lynx_mem->asyrcv.pcl[3].status=0;
    ti_lynx_mem->asyrcv.pcl[3].remainder=0;
    ti_lynx_mem->asyrcv.pcl[3].nextbuf=0;
    ti_lynx_mem->asyrcv.pcl[3].buf_0_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[3].buf_0=     PCI_ADDR(asyrcv.pcl[1].status);
    ti_lynx_mem->asyrcv.pcl[3].buf_1_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[3].buf_1=     0; /* Filled in by asyrcv.pcl[2].buf_7_ctl */
    ti_lynx_mem->asyrcv.pcl[3].buf_2_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[3].buf_2=     0; /* Filled in by asyrcv.pcl[0].buf_5_ctl */
    ti_lynx_mem->asyrcv.pcl[3].buf_3_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[3].buf_3=     0; /* Filled in by asyrcv.pcl[2].buf_9_ctl */
    ti_lynx_mem->asyrcv.pcl[3].buf_4_ctl= FW_AUX_CMD_STORE_QUAD;
    ti_lynx_mem->asyrcv.pcl[3].buf_4=     PCI_ADDR(asyrcv.TailP);
    ti_lynx_mem->asyrcv.pcl[3].buf_5_ctl= FW_AUX_CMD_LOAD;
    ti_lynx_mem->asyrcv.pcl[3].buf_5=     0; /* Filled in by asyrcv.pcl[0].buf_7_ctl */
    ti_lynx_mem->asyrcv.pcl[3].buf_6_ctl= FW_AUX_CMD_STORE_QUAD | FW_AUX_CMD_LAST|
                                       FW_AUX_CMD_INT;
    ti_lynx_mem->asyrcv.pcl[3].buf_6=     PCI_ADDR(asyrcv.WorkListP);

    /* Write it back */

    dki_dcache_wb   ((caddr_t)&ti_lynx_mem->asyrcv,sizeof(ti_lynx_mem->asyrcv));
    dki_dcache_inval((caddr_t)&ti_lynx_mem->asyrcv,sizeof(ti_lynx_mem->asyrcv));

    ti_lynx->dmaregs1[FW_ASYRCV_DMA_CHAN].DMA_PCLAddress =
        PCI_ADDR(asyrcv.StartPCLAddress);
}

static void
ti_lynx_start_asyrcv()
{
    volatile ti_lynx_t *ti_lynx;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;

    /* Start the DMA */
    ti_lynx->dmaregs1[FW_ASYRCV_DMA_CHAN].DMA_Control = 0xa0000000;
}

/* Initialize device registers */

static void
ti_lynx_init_lynx_regs(void)
{
    volatile ti_lynx_t *ti_lynx;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;

    /* Disable and reset Rx and Tx and the link layer */
    ti_lynx->LLControl = BUSY_CNTRL | RSTTX | RSTRX;
    /* Setup fifos */
    ti_lynx->FIFO_Size = (0x41<<16) | (0x41<<8) | 0x7e;
    ti_lynx->BISTHeaderLatencyCache = 0x3020;
    ti_lynx->FIFO_Threshold = 0x3838;
    /* Flush RX fifo */
    ti_lynx->FIFO_Control = 0x10;
    /* Setup iso rcv comparators */
    ti_lynx->dmaregs2[FW_ISORCV_DMA_CHAN].DMA_W0_Value = 0;
    ti_lynx->dmaregs2[FW_ISORCV_DMA_CHAN].DMA_W0_Mask = 0x90;
    ti_lynx->dmaregs2[FW_ISORCV_DMA_CHAN].DMA_W1_Value = 0;
    ti_lynx->dmaregs2[FW_ISORCV_DMA_CHAN].DMA_W1_Mask = 0x100;
    /* Setup sid rcv comparators */
    ti_lynx->dmaregs2[FW_SIDRCV_DMA_CHAN].DMA_W0_Value = 0x80000000;
    ti_lynx->dmaregs2[FW_SIDRCV_DMA_CHAN].DMA_W0_Mask =  0xc00000a0;
    ti_lynx->dmaregs2[FW_SIDRCV_DMA_CHAN].DMA_W1_Value = 0;
    ti_lynx->dmaregs2[FW_SIDRCV_DMA_CHAN].DMA_W1_Mask = 0x500;
    /* Setup asy rcv comparators */
    ti_lynx->dmaregs2[FW_ASYRCV_DMA_CHAN].DMA_W0_Value = 0xffc00000;
    ti_lynx->dmaregs2[FW_ASYRCV_DMA_CHAN].DMA_W0_Mask =  0xffc000a0;
    ti_lynx->dmaregs2[FW_ASYRCV_DMA_CHAN].DMA_W1_Value = 0;
    ti_lynx->dmaregs2[FW_ASYRCV_DMA_CHAN].DMA_W1_Mask = 0x100;
    ti_lynx->LLIntEnable |= PHY_BUSRESET | IT_STUCK | AT_STUCK | SNTRJ |
                         HDR_ERR | TC_ERR | GRF_OVER_FLOW | ITF_UNDER_FLOW
                        | ATF_UNDER_FLOW;
    /* Enable 1394 bus reset interrupt */
    /* Enable the self-id pcl interrupt and the async rcv interrupt */
    ti_lynx->PCIIntEnable =  P1394_INT                      |
                          FW_ASYXMT_DMA_HLT_INT |
                          FW_SIDRCV_DMA_PCL_INT |
                          FW_ASYRCV_DMA_PCL_INT |
                          FW_ASYXMT_DMA_PCL_INT;
    /* Reset the error counters */
    ti_lynx->LLErrorCounters = 0;
    /* Retry 10 times */
    ti_lynx->LLRetry=0xa;
}

static void
ti_lynx_start_ll()
{
    volatile ti_lynx_t *ti_lynx;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;

    /* Start up the link layer */

    ti_lynx->LLControl = /* 0x07800080 */
        TX_ISO_EN | RX_ISO_EN | TX_ASYNC_EN | RX_ASYNC_EN | RCV_COMP_VALID;
}

static void
ti_lynx_stop_ll()
{
    volatile ti_lynx_t *ti_lynx;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;

    /* Disable and reset Rx and Tx and the link layer */
    ti_lynx->LLControl = BUSY_CNTRL | RSTTX | RSTRX;
}

/* Interrupt counters - for debugging */

int ti_lynx_it_stuck, ti_lynx_at_stuck,      ti_lynx_sntrj,         ti_lynx_hdr_err;
int ti_lynx_tc_err,   ti_lynx_gre_over_flow, ti_lynx_busreset,      ti_lynx_asyrcv_pcl;
int ti_lynx_asyxmt_hlt, ti_lynx_asysid_pcl,  ti_lynx_itf_under_flow, ti_lynx_atf_under_flow;
int oldti_lynx_it_stuck, oldti_lynx_at_stuck, oldti_lynx_sntrj;
int oldti_lynx_hdr_err, oldti_lynx_tc_err;
int oldti_lynx_gre_over_flow, oldti_lynx_busreset, oldti_lynx_asyrcv_pcl;
int oldti_lynx_asyxmt_hlt, oldti_lynx_asysid_pcl;
int oldti_lynx_itf_under_flow, oldti_lynx_atf_under_flow;

static void
ti_lynx_monitor_interrupts()
{
int n;

    if( (n=(ti_lynx_it_stuck       - oldti_lynx_it_stuck     )) > 100)
        printf("ti_lynx_it_stuck interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_at_stuck       - oldti_lynx_at_stuck     )) > 100)
        printf("ti_lynx_at_stuck interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_sntrj          - oldti_lynx_sntrj        )) > 100)
        printf("ti_lynx_sntrj interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_hdr_err        - oldti_lynx_hdr_err      )) > 100)
        printf("ti_lynx_hdr_err interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_tc_err         - oldti_lynx_tc_err       )) > 100)
        printf("ti_lynx_tc_err interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_gre_over_flow  - oldti_lynx_gre_over_flow)) > 100)
        printf("ti_lynx_gre_over_flow interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_busreset       - oldti_lynx_busreset     )) > 100)
        printf("ti_lynx_busreset interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_asyrcv_pcl     - oldti_lynx_asyrcv_pcl   )) > 100)
        printf("ti_lynx_asyrcv_pcl interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_asyxmt_hlt     - oldti_lynx_asyxmt_hlt   )) > 100)
        printf("ti_lynx_asyxmt_hlt interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_asysid_pcl     - oldti_lynx_asysid_pcl   )) > 100)
        printf("ti_lynx_asysid_pcl interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_itf_under_flow     - oldti_lynx_itf_under_flow   )) > 100)
        printf("ti_lynx_itf_under_flow interrupt overrun %d i/s\n",n);
    if( (n=(ti_lynx_atf_under_flow     - oldti_lynx_atf_under_flow   )) > 100)
        printf("ti_lynx_atf_under_flow interrupt overrun %d i/s\n",n);

    oldti_lynx_it_stuck=ti_lynx_it_stuck;
    oldti_lynx_at_stuck=ti_lynx_at_stuck;
    oldti_lynx_sntrj=ti_lynx_sntrj;
    oldti_lynx_hdr_err=ti_lynx_hdr_err;
    oldti_lynx_tc_err=ti_lynx_tc_err;
    oldti_lynx_gre_over_flow=ti_lynx_gre_over_flow;
    oldti_lynx_busreset=ti_lynx_busreset;
    oldti_lynx_asyrcv_pcl=ti_lynx_asyrcv_pcl;
    oldti_lynx_asyxmt_hlt=ti_lynx_asyxmt_hlt;
    oldti_lynx_asysid_pcl=ti_lynx_asysid_pcl;
    oldti_lynx_itf_under_flow=ti_lynx_itf_under_flow;
    oldti_lynx_atf_under_flow=ti_lynx_atf_under_flow;

    itimeout(ti_lynx_monitor_interrupts,0,100,splhi,0,0,0);
}

int
ti_lynx_map(dev_t dev, vhandl_t *vt, off_t off, int len, int prot)
{
    int error;

    /* Fail if:
         vt is NULL,
         the offset is non-zero,
         len != NMMAPPAGES*4096
         ti_lynx_init failed to allocate mappages
    */
    if(vt==0 || off != 0 || len != NMAPPAGES*4096 || ti_lynx_state_ptr->mappages==0){
        return(EFAULT);
    }

    if(error = v_mapphys(vt, ti_lynx_state_ptr->mappages, len)) {
        return(error);
    }
    return(0);
}

/****************************************************************************
 *  PCI Infrastructure Functions                                            *
 ****************************************************************************
 */

/* int ti_lynx_error_handler()
 * called by pci infrastructure, registered in ti_lynx_attach
 */

static int
ti_lynx_error_handler(void *einfo,
                  int error_code,
                  ioerror_mode_t mode,
                  ioerror_t *ioerror)
{
    info_t *info = (info_t *)einfo;
    vertex_hdl_t vhdl = info->parent_vertex;
    char           *ecname;
    iopaddr_t erraddr = IOERROR_GETVALUE(ioerror,busaddr);

    switch (error_code) {

      case PIO_READ_ERROR:
        ecname = "PIO Read Error";
        break;

      case PIO_WRITE_ERROR:
        ecname = "PIO Write Error";
        break;

      case DMA_READ_ERROR:
        ecname = "DMA Read Error";
        break;

      case DMA_WRITE_ERROR:
        ecname = "DMA Write Error";
        break;

      default:
        ecname = "Unknown Error Type";
        break;
    }

    /* XXX should shut down subsystem at worst */
    cmn_err(CE_WARN,
            "ti_lynx %v: %s (code=%d,mode=%d) at PCI address 0x%X\n",
            vhdl, ecname, error_code, mode, erraddr);

    return IOERROR_UNHANDLED;           /* let pciio do a full dump */
}

__int32_t
ti_lynx_detach ( vertex_hdl_t vertex)
{
    return 0;
}

static int
ti_lynx_pcierror ( error_handler_arg_t einfo,
                int error_code,
                ioerror_mode_t mode,
                ioerror_t *ioe)
{
    printf("ti_lynx_pcierror\n");
    return 0;
}

static int
ti_lynx_kvtopci(caddr_t vaddr,  int nbytes, int *pciaddrs, int step)
{
    int npgs,i;
    paddr_t phys;

    npgs = numpages(vaddr, nbytes);
    if (npgs == 0) return(0);

    for(i=0; i<npgs; i++) {
        phys = kvtophys(vaddr);
        vaddr = (caddr_t)( (int)vaddr & ~4095);
        vaddr += 4096;
        *pciaddrs = PHYS_TO_PCI_NATIVE(phys);
        pciaddrs = (int *)((char *)pciaddrs + step);
    }
    return(npgs);
}

static void
ti_lynx_WritePhyReg(int regnum, int val)
{
    int t,ntries,nspins;
    volatile ti_lynx_t* ti_lynx;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;

    ntries=3;
    while(ntries--) {
        ti_lynx->PHYAccess = 0x40000000|(regnum<<24)|(val<<16);
        nspins=1000;
        while(nspins--) {
            t=ti_lynx->PHYAccess;
            if( (t&0x40000000) ==0)return;
        }
    }
    printf("Failed writing 1394 Phy Register %d with 0x%x\n",regnum,val);
}

static int
ti_lynx_ReadPhyReg(int regnum)
{
    int returned_regnum,s;
    volatile ti_lynx_t* ti_lynx;
    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;

    while(1) {
        ti_lynx->PHYAccess = 0x80000000|regnum<<24;
        delay(1);
        s=ti_lynx->PHYAccess;
        returned_regnum=(s>>8)&0xff;
        if(returned_regnum==regnum)break;
        printf("Read of PHY register %d failed, retrying\n",regnum); 
    }
    return(s&0xff);
}

static void
ti_lynx_setup_asyxmt(int xferlen)
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx_stop_asyxmt();

    /* Initialize the variables used by the PCL list */

    ti_lynx_mem->asyxmt.StartPCLAddress=PCI_ADDR(asyxmt.pcl[0]);

    /* Build the PCL list */

    ti_lynx_mem->asyxmt.pcl[0].next=PCI_ADDR(asyxmt.pcl[1]);
    ti_lynx_mem->asyxmt.pcl[0].error_next=PCI_ADDR(asyxmt.pcl[1]);
    ti_lynx_mem->asyxmt.pcl[0].sw=0;
    ti_lynx_mem->asyxmt.pcl[0].status=0;
    ti_lynx_mem->asyxmt.pcl[0].remainder=0;
    ti_lynx_mem->asyxmt.pcl[0].buf_0_ctl= FW_AUX_CMD_ADD | FW_AUX_CMD_LAST;
    ti_lynx_mem->asyxmt.pcl[0].buf_0=     4;

    ti_lynx_mem->asyxmt.pcl[1].next=PCI_ADDR(asyxmt.pcl[2]);
    ti_lynx_mem->asyxmt.pcl[1].error_next=PCI_ADDR(asyxmt.pcl[2]);
    ti_lynx_mem->asyxmt.pcl[1].sw=0;
    ti_lynx_mem->asyxmt.pcl[1].status=0;
    ti_lynx_mem->asyxmt.pcl[1].remainder=0;
    ti_lynx_mem->asyxmt.pcl[1].nextbuf=0;
    ti_lynx_mem->asyxmt.pcl[1].buf_0_ctl= FW_XFER_CMD_XMT           |
                                       FW_XFER_CMD_BIGENDIAN     |
                                       FW_XFER_CMD_LAST_BUF      |
                                       FW_XFER_CMD_WAITFORSTATUS |
                                       FW_XFER_CMD_INT           |
                                       xferlen;
    ti_lynx_mem->asyxmt.pcl[1].buf_0=     PCI_ADDR(asyxmt.buf[0]);

    ti_lynx_mem->asyxmt.pcl[2].next=1;
    ti_lynx_mem->asyxmt.pcl[2].error_next=1;
    ti_lynx_mem->asyxmt.pcl[2].sw=0;
    ti_lynx_mem->asyxmt.pcl[2].status=0;
    ti_lynx_mem->asyxmt.pcl[2].remainder=0;
    ti_lynx_mem->asyxmt.pcl[2].buf_0_ctl= FW_AUX_CMD_ADD | FW_AUX_CMD_LAST;
    ti_lynx_mem->asyxmt.pcl[2].buf_0=     4;

    /* Write it back */

    dki_dcache_wb   ((caddr_t)&ti_lynx_mem->asyxmt,sizeof(ti_lynx_mem->asyxmt));
    dki_dcache_inval((caddr_t)&ti_lynx_mem->asyxmt,sizeof(ti_lynx_mem->asyxmt));

    /* Start the DMA */

    ti_lynx->dmaregs1[FW_ASYXMT_DMA_CHAN].DMA_PCLAddress =
        PCI_ADDR(asyxmt.StartPCLAddress);
}

static void
ti_lynx_start_asyxmt()
{
    volatile ti_lynx_t *ti_lynx;
    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx->dmaregs1[FW_ASYXMT_DMA_CHAN].DMA_Control = 0xa0000000;
}

int ti_lynx_stop_isorcv_nretries=20;
static void
ti_lynx_stop_isorcv()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx->dmaregs1[FW_ISORCV_DMA_CHAN].DMA_Control = 0x00000000;
    for(i=0;i<ti_lynx_stop_isorcv_nretries;i++) {
        if(!(ti_lynx->dmaregs1[FW_ISORCV_DMA_CHAN].DMA_Control & 0x80000000))break;
        drv_usecwait(1);
    }

    #ifdef DEBUG
    if(i==ti_lynx_stop_isorcv_nretries)printf("Can't stop FW_ISORCV DMA\n");
    #endif
}

int ti_lynx_stop_isoxmt_nretries=20;

/* static */ void 	/* XXX Should be static */
ti_lynx_stop_isoxmt()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx->dmaregs1[FW_ISOXMT_DMA_CHAN].DMA_Control = 0x00000000;
    for(i=0;i<ti_lynx_stop_isoxmt_nretries;i++) {
        if(!(ti_lynx->dmaregs1[FW_ISOXMT_DMA_CHAN].DMA_Control & 0x80000000))break;
        drv_usecwait(1);
    }

    #ifdef DEBUG
    if(i==ti_lynx_stop_isoxmt_nretries)printf("Can't stop FW_ISOXMT DMA\n");
    #endif
}

int ti_lynx_stop_sidrcv_nretries=20;
static void
ti_lynx_stop_sidrcv()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx->dmaregs1[FW_SIDRCV_DMA_CHAN].DMA_Control = 0x00000000;
    for(i=0;i<ti_lynx_stop_sidrcv_nretries;i++) {
        if(!(ti_lynx->dmaregs1[FW_SIDRCV_DMA_CHAN].DMA_Control & 0x80000000))break;
        drv_usecwait(1);
    }

    #ifdef DEBUG
    if(i==ti_lynx_stop_sidrcv_nretries)printf("Can't stop FW_SIDRCV DMA\n");
    #endif
}

int ti_lynx_stop_asyrcv_nretries=20;
static void
ti_lynx_stop_asyrcv()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx->dmaregs1[FW_ASYRCV_DMA_CHAN].DMA_Control = 0x00000000;
    for(i=0;i<ti_lynx_stop_asyrcv_nretries;i++) {
        if(!(ti_lynx->dmaregs1[FW_ASYRCV_DMA_CHAN].DMA_Control & 0x80000000))break;
        drv_usecwait(1);
    }

    #ifdef DEBUG
    if(i==ti_lynx_stop_asyrcv_nretries)printf("Can't stop FW_ASYRCV DMA\n");
    #endif

}

int ti_lynx_stop_asyxmt_nretries=20;
static void
ti_lynx_stop_asyxmt()
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int i;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    /* A DMA may be running.  Stop it. */
    ti_lynx->dmaregs1[FW_ASYXMT_DMA_CHAN].DMA_Control = 0x00000000;
    for(i=0;i<ti_lynx_stop_asyxmt_nretries;i++) {
        if(!(ti_lynx->dmaregs1[FW_ASYXMT_DMA_CHAN].DMA_Control & 0x80000000))break;
        drv_usecwait(1);
    }

    #ifdef DEBUG
    if(i==ti_lynx_stop_asyxmt_nretries)printf("Can't stop FW_ASYXMT DMA\n");
    #endif
}

int
ti_lynx_get_headerlist_info(int op, int index, int **hdr, int **cyclecounter,
				int **len, int **datap)
{
   int ret=0;
   ti_lynx_mem_layout_t *ti_lynx_mem;
   ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

   switch(op) {
 	case SIDRCV:
    		/* Cache invalidate the packet header */
    		dki_dcache_inval((caddr_t)&ti_lynx_mem->sidrcv.headerlist[index].hdr,16);
		(*hdr)=&ti_lynx_mem->sidrcv.headerlist[index].hdr;		
		(*cyclecounter)=&ti_lynx_mem->sidrcv.headerlist[index].cyclecounter;
		(*len)=&ti_lynx_mem->sidrcv.headerlist[index].len;
		(*datap)=&ti_lynx_mem->sidrcv.datapages[index*512];
		break;
	case ISORCV:
                /* Cache invalidate the packet header */
                dki_dcache_inval((caddr_t)&ti_lynx_mem->isorcv.headerlist[index].hdr,16);
                (*hdr)=&ti_lynx_mem->isorcv.headerlist[index].hdr;         
                (*cyclecounter)=&ti_lynx_mem->isorcv.headerlist[index].cyclecounter;
                (*len)=&ti_lynx_mem->isorcv.headerlist[index].len;
                (*datap)=&ti_lynx_mem->isorcv.datapages[index*512];
		break;
	case ISOXMT:
		/* XXX: This must be completed */
		/*
                Cache invalidate the packet header 
                dki_dcache_inval((caddr_t)&ti_lynx_mem->isoxmt.headerlist[index].hdr,16);
                (*hdr)=&ti_lynx_mem->isoxmt.headerlist[index].hdr;         
                (*cyclecounter)=&ti_lynx_mem->isoxmt.headerlist[index].cyclecounter;
                (*len)=&ti_lynx_mem->isoxmt.headerlist[index].len;
		*/
                (*datap)=&ti_lynx_mem->isoxmt.datapages[index*512];
		break;
	case ASYRCV:
                /* Cache invalidate the packet header */
                dki_dcache_inval((caddr_t)&ti_lynx_mem->asyrcv.headerlist[index].hdr,16);
                (*hdr)=&ti_lynx_mem->asyrcv.headerlist[index].hdr;         
                (*cyclecounter)=&ti_lynx_mem->asyrcv.headerlist[index].cyclecounter;
                (*len)=&ti_lynx_mem->asyrcv.headerlist[index].len;
                (*datap)=&ti_lynx_mem->asyrcv.datapages[index*512];
		break;
	case ASYXMT:
		/* XXX: This does not exist. Maybe we should not provide 
			ASYXMT interface at all */
                /* 
                Cache invalidate the packet header
                dki_dcache_inval((caddr_t)&ti_lynx_mem->asyxmt.headerlist[index].hdr,16);
		(*hdr)=&ti_lynx_mem->asyxmt.headerlist[index].hdr;         
                (*cyclecounter)=&ti_lynx_mem->asyxmt.headerlist[index].cyclecounter;
                (*len)=&ti_lynx_mem->asyxmt.headerlist[index].len;
                (*datap)=&ti_lynx_mem->asyxmt.datapages[index*512];
		*/
		ret=1;
		break;
	default:
		ret=1;
   }
   return (ret);
}

/* This function returns ISORCV_NUMDATAPACKETS. It implicitly returns
   tailp (&ti_lynx_mem->isorcv.TailP) */
int         
ti_lynx_get_isorcv_info(int *tailp)
{
  volatile ti_lynx_mem_layout_t *ti_lynx_mem;

  ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;
  dki_dcache_inval((caddr_t)&ti_lynx_mem->isorcv.TailP,4);
  (*tailp)=ti_lynx_mem->isorcv.TailP;
  return (ISORCV_NUMDATAPACKETS);
}

/* This function returns ISOXMT_NUMDATAPACKETS. It implicitly returns
   headp (&ti_lynx_mem->isorcv.HeadP) */
int         
ti_lynx_get_isoxmt_info(int *headp)
{
  volatile ti_lynx_mem_layout_t *ti_lynx_mem;

  ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;
  dki_dcache_inval((caddr_t)&ti_lynx_mem->isoxmt.HeadP,4);
  (*headp)=ti_lynx_mem->isoxmt.HeadP;
  return (ISOXMT_NUMDATAPACKETS);
}

/*=================== Link Layer ==========================*/
void
ti_lynx_link_request(ieee1394_transaction_t *t)
{
    int i;
    short destination_id;
    int tlabel;
    char tcode;
    char priority;
    short source_id;
    long long destination_offset;
    short data_length;

    short extended_tcode;
    int *block_data_p;
    int nwords;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int packet_length;

    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    destination_id=t->destination_id;
    tlabel=t->tlabel;
    printf("TLABEL: %d\n", tlabel);
    tcode=t->tcode;
    priority=t->priority;
    source_id=t->source_id;
    destination_offset=t->destination_offset;
    data_length=t->data_length;
    extended_tcode=t->extended_tcode;
    block_data_p=t->block_data_p;

    ti_lynx_mem->asyxmt.buf[0]= (destination_id<<16) |
                             (tlabel<<10)   |
                             (tcode<<4) |
                             priority;
    ti_lynx_mem->asyxmt.buf[1]= (source_id<<16)  |
                             (destination_offset>>32);
    ti_lynx_mem->asyxmt.buf[2]= destination_offset;
    ti_lynx_mem->asyxmt.buf[3]= (data_length<<16)  |
                             extended_tcode;

    if(tcode==IEEE1394_WRITE_REQUEST_QUAD) {
        ti_lynx_mem->asyxmt.buf[3]=block_data_p[0];
        packet_length=16;
    } else if(tcode==IEEE1394_READ_REQUEST_QUAD) {
        packet_length=12;
        for (i=0; i<10; i++)
           printf("buf[%d] = 0x%x\n", i, ti_lynx_mem->asyxmt.buf[i]);
    } else if (tcode==IEEE1394_LOCK_REQUEST) {
       /* data_length = 8, nwords should be 2
        * block_data_p[0] = arg_value;
        * block_data_p[1] = data_value;
        */
        nwords=(data_length+3)/4;
        for(i=0;i<nwords;i++){
            ti_lynx_mem->asyxmt.buf[3+(i+1)]=block_data_p[i];
        }
        packet_length=16+data_length;
    } else if (tcode == IEEE1394_WRITE_REQUEST_BLOCK) {
        nwords=(data_length+3)/4;
        for(i=0;i<nwords;i++)
            ti_lynx_mem->asyxmt.buf[3+(i+1)]=block_data_p[i];
        packet_length=16+data_length;
        for (i=0; i<10; i++)
           printf("buf[%d] = 0x%x\n", i, ti_lynx_mem->asyxmt.buf[i]);
    } else {
        printf("Unsupported request type 0x%x\n",tcode);
        t->busy=0;
        t->state=TQ_AWAITING_XMIT;
        packet_length=12;
    }

    ti_lynx_setup_asyxmt(packet_length);
    ti_lynx_start_asyxmt();
}


/*
 * ti_lynx_attach()
 *
 * Description:
 * called by OS to register ti_lynx card to PCI infra structure
 * which takes care of PCI initialization.
 */
__int32_t
ti_lynx_attach (vertex_hdl_t vertex)
{

    pciio_info_t info_p;
    pciio_device_id_t device;
    pciio_vendor_id_t vendor;
    pciio_slot_t slot;
    inventory_t *inv_ptr = NULL;
    int i;
    info_t *info;
    vertex_hdl_t tmp_vertex;
    pciio_intr_t ti_lynx_int_handle;        /* interrupt handle */
    int retcode;

    /* setup interrupt handle so we can shut off interrupts later */
    ti_lynx_int_handle = pciio_intr_alloc(vertex, 0, PCIIO_INTR_LINE_A,
        vertex);

    /* allocate memory for ti_lynx_state */
    ti_lynx_state_ptr = kmem_zalloc(sizeof(ti_lynx_state_t), KM_SLEEP);
    if (ti_lynx_state_ptr == NULL) {
        printf("TI_LYNX-ATTACH: Error allocating memory for ti_lynx_state_ptr->\n");
        return(ENOMEM);
    }

    /* Temporarily initializing with No Data */
    ti_lynx_state_ptr->p_mem0 = 0;
    ti_lynx_state_ptr->p_mem1 = 0;
    ti_lynx_state_ptr->p_mem2 = 0;
    ti_lynx_state_ptr->mappages = 0;
    ti_lynx_state_ptr->tlist = 0;
    ti_lynx_state_ptr->sidrcv_HeadP = SIDRCV_NUMDATAPACKETS-1;
    ti_lynx_state_ptr->isorcv_HeadP = ISORCV_NUMDATAPACKETS-1;
    ti_lynx_state_ptr->asyrcv_HeadP = ASYRCV_NUMDATAPACKETS-1;
    ti_lynx_state_ptr->rcvquad = 0;

    ti_lynx_state_ptr->p_mem0 = (__psunsigned_t)pciio_piotrans_addr
        (vertex, 0,             /* device and (override) dev_info */
         PCIIO_SPACE_WIN(0),    /* select configuration address space */
         0,                     /* from the start of space, */
         4096,                  /* byte count */
         0);                    /* flag word */
    ti_lynx_state_ptr->p_mem1 = (__psunsigned_t)pciio_piotrans_addr
        (vertex, 0,             /* device and (override) dev_info */
         PCIIO_SPACE_WIN(1),    /* select configuration address space */
         0,                     /* from the start of space, */
         65536,                 /* byte count */
         0);                    /* flag word */
    ti_lynx_state_ptr->p_mem2 = (__psunsigned_t)pciio_piotrans_addr
        (vertex, 0,             /* device and (override) dev_info */
         PCIIO_SPACE_WIN(2),    /* select configuration address space */
         0,                     /* from the start of space, */
         65536,                 /* byte count */
         0);                    /* flag word */

/* printf("Device id:\n");
   printf("\t0x%x\n",pciio_config_get(vertex,PCI_CFG_DEVICE_ID, 2));
   printf("Command:\n");
   printf("\t0x%x\n",pciio_config_get(vertex,PCI_CFG_COMMAND, 2));
*/


    /*
     * create a new info_t and partially initialize it
     */

    info = (info_t*) kmem_zalloc(sizeof(info_t), 0);
    if (info == 0) {
        printf("TI_LYNX-ATTACH: out of memory\n");
        return ENOMEM;
    }
    info->parent_vertex = vertex;

    pciio_error_register(vertex, ti_lynx_error_handler, info);

    /*
     * we add an openable node to the hardware graph solely so ioconfig can
     * open us and assign a unit number.
     */
    if (hwgraph_char_device_add(vertex, "ti_lynx", "ti_lynx_", &tmp_vertex) !=
                        GRAPH_SUCCESS) {
        printf("TI_LYNX-ATTACH: hwgraph_char_device_add failed\n");
        return 0;
    }
    hwgraph_fastinfo_set(tmp_vertex, (arbitrary_info_t)info);

    /*
     * XXX We send in the revision field as a single number. It is
     * up to hinv.c to do the formatting.
     */
    /* device_inventory_add(tmp_vertex, INV_PCI,INV_BUS, -1, 0,
                     rev_level | (board_id << 4));  */


    ti_lynx_state_ptr->mappages = kvpalloc(NMAPPAGES,0,0);
    #ifdef DEBUG_ZERO_MAPPAGES
    {
        int i;
        for(i=0;i<NMAPPAGES*4096;i+=4) {
            *((int *)(((char *)ti_lynx_state_ptr->mappages)+i))=0xdeaddeed;
        }
    }
    #endif /* DEBUG_ZERO_MAPPAGES */

    if (ti_lynx_setup()==ENOMEM) return (ENOMEM);

    /* Need to make sure that the selfid thread has started before the
       interrupt vector is setup and registered. If the asyrecv thread
       has not started, that's not a problem. */
    MUTEX_LOCK(&selfid_thread_started_lock, -1);
    if (!selfid_thread_started)
      SV_WAIT(&selfid_thread_started_sv, &selfid_thread_started_lock, 0);
    else
      MUTEX_UNLOCK(&selfid_thread_started_lock);

    /****************************
    set up interrupt vector
    ****************************/
    retcode = pciio_intr_connect(ti_lynx_int_handle,(void(*)())ti_lynx_intr,
                                  (intr_arg_t)0,NULL);
    if (retcode < 0) {
      cmn_err(CE_ALERT, "TI_LYNX-ATTACH: vertex %v can't connect interrupt.",
              info->parent_vertex);
            return(-1);
    }

    return (0);
}

void
ti_lynx_intr(eframe_t *e, void *p)
{
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;
    int llintstatus,pciintstatus;
    volatile ti_lynx_t *ti_lynx;
    int tailp;
    unsigned long long startust, endust;
    int dmafinished;

    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;
    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    llintstatus = ti_lynx -> LLIntStatus;
    pciintstatus = ti_lynx -> PCIIntStatus;

    if(llintstatus & IT_STUCK)printf("IT_STUCK\n");
    if(llintstatus & AT_STUCK)printf("AT_STUCK\n");
    if(llintstatus & SNTRJ)printf("SNTRJ\n");
    if(llintstatus & HDR_ERR)printf("HDR_ERR\n");
    if(llintstatus & TC_ERR)printf("TC_ERR\n");
    if(llintstatus & GRF_OVER_FLOW)printf("GRF_OVER_FLOW\n");
    if(llintstatus & PHY_BUSRESET)printf("PHY_BUSRESET\n");
#ifdef DEBUG_INTERRUPTS
    if(pciintstatus & FW_ASYRCV_DMA_PCL_INT)printf("FW_ASYRCV_DMA_PCL_INT\n");
    if(pciintstatus & FW_ASYXMT_DMA_HLT_INT)printf("FW_ASYXMT_DMA_HLT_INT\n");
    if(pciintstatus & FW_SIDRCV_DMA_PCL_INT)printf("FW_SIDRCV_DMA_PCL_INT\n");
#endif /* DEBUG_INTERRUPTS */

    if(llintstatus & ITF_UNDER_FLOW)printf("ITF_UNDER_FLOW\n");
    if(llintstatus & ATF_UNDER_FLOW)printf("ATF_UNDER_FLOW\n");

    if(llintstatus & IT_STUCK)ti_lynx_it_stuck++;
    if(llintstatus & AT_STUCK)ti_lynx_at_stuck++;
    if(llintstatus & SNTRJ)ti_lynx_sntrj++;
    if(llintstatus & HDR_ERR)ti_lynx_hdr_err++;
    if(llintstatus & TC_ERR)ti_lynx_tc_err++;
    if(llintstatus & GRF_OVER_FLOW)ti_lynx_gre_over_flow++;
    if(llintstatus & PHY_BUSRESET)ti_lynx_busreset++;
    if(pciintstatus & FW_ASYRCV_DMA_PCL_INT)ti_lynx_asyrcv_pcl++;
    if(pciintstatus & FW_ASYXMT_DMA_HLT_INT)ti_lynx_asyxmt_hlt++;
    if(pciintstatus & FW_SIDRCV_DMA_PCL_INT)ti_lynx_asysid_pcl++;
    if(llintstatus & ITF_UNDER_FLOW)ti_lynx_itf_under_flow++;
    if(llintstatus & ATF_UNDER_FLOW)ti_lynx_atf_under_flow++;

    /* Handle Transmit done interrupt */

    if(pciintstatus &        FW_ASYXMT_DMA_HLT_INT) {
        ti_lynx->PCIIntStatus = FW_ASYXMT_DMA_HLT_INT;
        pciintstatus &=     ~FW_ASYXMT_DMA_HLT_INT;
        dki_dcache_inval((caddr_t)&ti_lynx_mem->asyxmt.pcl[1].status,4);
	ieee1394_interrupt(ACKRCV, (int*) &ti_lynx_mem->asyxmt.pcl[1].status, 
				0);
    }

    /* Handle async packet received interrupt */

    if(pciintstatus &        FW_ASYRCV_DMA_PCL_INT) {
        ti_lynx->PCIIntStatus = FW_ASYRCV_DMA_PCL_INT;
        pciintstatus &=     ~FW_ASYRCV_DMA_PCL_INT;

        dki_dcache_inval((caddr_t)&ti_lynx_mem->asyrcv.TailP,4);
        tailp=ti_lynx_mem->asyrcv.TailP;
        if(-1 != tailp) {
            if(tailp==ti_lynx_state_ptr->asyrcv_HeadP)printf("Bad news #1\n");
            while(tailp != ti_lynx_state_ptr->asyrcv_HeadP) {
                ti_lynx_state_ptr->asyrcv_HeadP++;
                if(ti_lynx_state_ptr->asyrcv_HeadP>=ASYRCV_NUMDATAPACKETS)
                    ti_lynx_state_ptr->asyrcv_HeadP-=ASYRCV_NUMDATAPACKETS;
		ieee1394_interrupt(ASYRCV, (int*) &ti_lynx_mem->asyrcv.TailP, 
					0);
            }
        }
    }


    /* Handle BUS_RESET */

    if(llintstatus & PHY_BUSRESET) {
        ti_lynx->LLIntStatus = PHY_BUSRESET;
        llintstatus &= ~PHY_BUSRESET;
        MUTEX_LOCK(&bus_reset_lock,-1);
        reset_state=1;
        bus_reset_state=1;
        SV_BROADCAST(&bus_reset_sv);
        MUTEX_UNLOCK(&bus_reset_lock);
        /* Workaround as per "Texas Instruments TSB12LV21PGF PCILynx Errata,
           Revision 6, Last Updated 9/18/96", errata #8 */
           /* XXX need to disable and reenable the async channels */
#ifdef LATER
           ti_lynx_stop_asyxmt();
           ti_lynx_stop_asyrcv();
           ti_lynx->FIFO_Control=0x1c;
#endif
        /* End workaround */
	ieee1394_interrupt(BUSRESET, 0, 0);
    }

    /* Handle Self ID Packet Reception */


    if(pciintstatus & FW_SIDRCV_DMA_PCL_INT) {
        ti_lynx->PCIIntStatus = FW_SIDRCV_DMA_PCL_INT;
        pciintstatus &= ~FW_SIDRCV_DMA_PCL_INT;

        dki_dcache_inval((caddr_t)&ti_lynx_mem->sidrcv.TailP,4);
        tailp=ti_lynx_mem->sidrcv.TailP;

        if(tailp == -1) {
        } else if(tailp == ti_lynx_state_ptr->sidrcv_HeadP) {
        } else {
            tree_db.bus_reset_sequence_number++;
	    ieee1394_interrupt(SIDRCV, (int*) &ti_lynx_mem->sidrcv.TailP,
				ti_lynx_post_interrupt);
            ti_lynx_setup_asyrcv();
            ti_lynx_start_asyrcv();
      }
   }

    ti_lynx->LLIntStatus = llintstatus;
    ti_lynx->PCIIntStatus = pciintstatus;
    flushbus();
    flushbus();
    drv_usecwait(100);
    flushbus();
    flushbus();
}

static void 
ti_lynx_post_interrupt(int op) 
{
    volatile ti_lynx_t *ti_lynx;
    volatile ti_lynx_mem_layout_t *ti_lynx_mem;

    ti_lynx = (ti_lynx_t*)ti_lynx_state_ptr->p_mem0;
    ti_lynx_mem = (ti_lynx_mem_layout_t *)ti_lynx_state_ptr->mappages;

    switch (op) {
	case SIDRCV:
	   ti_lynx_state_ptr->sidrcv_HeadP=ti_lynx_mem->sidrcv.TailP;

           if(tree_db.n_self_ids-1 == tree_db.our_phy_ID) {
                /* If we are the root */
                ti_lynx->LLControl =
                (       TX_ISO_EN   | RX_ISO_EN   |
                        TX_ASYNC_EN | RX_ASYNC_EN |
                        CYCMASTER   | CYCTIMEREN  | RCV_COMP_VALID);
            }
            else {
                /* If someone else is the root */
                ti_lynx->LLControl =
                        TX_ISO_EN   | RX_ISO_EN  |
                        TX_ASYNC_EN |RX_ASYNC_EN |
                        CYCSOURCE   |CYCTIMEREN  | RCV_COMP_VALID;
            }
	    break;

	default:
		printf("Unknown post_interrupt operation in ti_lynx.\n");
		break;
    }
    return;
}
