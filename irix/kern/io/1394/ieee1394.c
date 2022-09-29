#include "ieee1394_kern.h"

/* Function prototypes */
void	    ieee1394_shared_init(void);
static int  ieee1394_hwgraphregister(void);
int         ieee1394_init(struct edt *edtp);
int 	    ieee1394_open(dev_t *devp, int oflag, int otyp, cred_t *crp);
int         ieee1394_ioctl(dev_t dev, int cmd, void *arg, int mode,
	    		cred_t *crp, int *rvalp);
static void ieee1394_ioctl_readreg(void *arg);
static void ieee1394_ioctl_writereg(void *arg);
static int  ieee1394_ioctl_get_mmapsize(void);
static int  ieee1394_ioctl_get_tree(void *arg);
static int  ieee1394_ioctl_quadread(void *arg);
static int  ieee1394_ioctl_quadwrite(void *arg);
static int  ieee1394_ioctl_quadlock(void *arg);
static int  ieee1394_ioctl_blockread(void *arg);
static int  ieee1394_ioctl_blockwrite(void *arg);
void 	    ieee1394_interrupt(int op, int *param, void (*funp)(int op));
static void ieee1394_link_data_confirmation(ieee1394_transaction_t *t, int status);
static void ieee1394_link_data_indication(int *hdrp, int *cyclecounter, int *lenp,
				int *datap);
int ieee1394_gain_interest(long long Vendor_ID, long long Device_ID, 
				 int app_id, long long begaddr, long long endaddr, 
				 fun_t fp);
int ieee1394_lose_interest(long long Vendor_ID, long long Device_ID, 
				  int app_id, long long begaddr, long long endaddr, 
				  fun_t fp);
static int  ieee1394_update_InterestTable(void);
static void ieee1394_free_mem_InterestTable(void); 
static void ieee1394_print_InterestTable(void);
/* XXX: for debug purposes */
static void ieee1394_ioctl_test_isoxmt(void *arg);
void 	    thread_selfid_recv(void);
void 	    thread_asyrecv(void);
static int  ieee1394_wait_for_reset(void);
static int  ieee1394_check_for_reset(void);
static void ieee1394_ParseSelfIDPacket(int *headerp,int *cyclecounterp,
					int *lenp, int *datap);
static int  ieee1394_BuildSelfIDTree(self_id_db_t *tree_db);
static int  ieee1394_get_UID(int phy_ID, self_id_record_t *selfid_record);
static int  ieee1394_quadread(int phy_ID, long long offset);
static void ieee1394_add_transaction(ieee1394_transaction_t *t);
static void ieee1394_remove_transaction(ieee1394_transaction_t *t);
static void ieee1394_activate_tq_rotor(void);
static void ieee1394_do_tq_rotor(void);
static int  ieee1394_service_a_transaction(ieee1394_transaction_t *t);
static void ieee1394_print_transaction(ieee1394_transaction_t *t);


void 
ieee1394_shared_init(void) 
{
   MUTEX_LOCK(&shared_init_lock, -1);
   if (shared_init_called) {
     MUTEX_UNLOCK(&shared_init_lock);
     return;
   }
   shared_init_called++;

   /* Initialize shared variables */
   reset_state=0;
   bus_reset_state=0;
   MUTEX_INIT(&bus_reset_lock, MUTEX_DEFAULT, "bus_reset_lock");
   SV_INIT(&bus_reset_sv, SV_DEFAULT, "bus_reset_sv");
   tree_db.bus_reset_sequence_number=0;
   tree_db.self_id_invalid=1;
   tree_db.n_self_ids=1;
   tree_db.our_phy_ID=0;
   tree_db.IRM_phy_ID=-1;
   MUTEX_INIT(&selfid_thread_started_lock, MUTEX_DEFAULT, 
			"selfid_thread_started_lock");
   SV_INIT(&selfid_thread_started_sv, SV_DEFAULT, "selfid_thread_started_sv");
   selfid_thread_started=0;

   /* Required to dynamically call the correct lower-level driver
        function */
   board_driver_readreg=0;
   board_driver_writereg=0;
   board_driver_get_mmapsize=0;
   board_driver_link_request=0;
   board_driver_get_headerlist_info=0;
   board_driver_get_isorcv_info=0;
   board_driver_ReadOurSelfIDInfo=0;

   /* XXX: Experimental. Please delete later. */
   board_driver_ioctl_test_isoxmt=0;

   MUTEX_UNLOCK(&shared_init_lock);
}

int
ieee1394_open(dev_t *devp, int oflag, int otyp, cred_t *crp)
{
    return(0);
}

/* ********************************************************************
 * Create /hw/ieee1394 directory and char_device_add ieee1394 in it.   *
 * Returns 0 for no error and -1 if error occured while adding vertex *
 **********************************************************************/

static int
ieee1394_hwgraphregister(void)
{
    int rc;
    vertex_hdl_t vertex, tmp_vertex;
    info_t *info;

    /*
     * create a new info_t and partially initialize it
     */

    info = (info_t*) kmem_zalloc(sizeof(info_t), 0);
    if (info == 0) {
        printf("IEEE1394:ieee1394_hwgraphregister() out of memory\n");
        return ENOMEM;
    }

    vertex = GRAPH_VERTEX_NONE;
    rc = hwgraph_path_add(hwgraph_root, "ieee1394", &vertex);
    if (   (rc != GRAPH_SUCCESS)
        || (vertex == NULL)
        || (vertex == GRAPH_VERTEX_NONE)) {
        cmn_err(CE_ALERT, "ieee1394_hwgraphregister: can't add path /hw/ieee1394");
        return(-1);
    }

    info->parent_vertex = vertex;
    /* info->app_no = -1; */

    if (hwgraph_char_device_add(vertex, "ieee1394", "ieee1394_", &tmp_vertex) !=
                        GRAPH_SUCCESS) {
        printf("ieee1394_hwgraphregister: hwgraph_char_device_add failed\n");
        return (-1);
    }
    /* By default grant READ, WRITE permissions to all */
    hwgraph_chmod(tmp_vertex, (mode_t) 0666);

    hwgraph_fastinfo_set(tmp_vertex, (arbitrary_info_t)info);
    return(0);
}


/**************************************************************************
 *  Device Driver Entry Points
 *************************************************************************/

/*
 * ieee1394_init()
 *
 * Description:
 * Called once by kernel at system startup time.
 * Returns ENOMEM, or 0 (for success).
 */

int
ieee1394_init (struct edt *edtp)
{
    int i, j;
    UID_node_t *UID_nodep;
    node_t *prev;
    node_t *currnode;
    int s;

    printf("Came to IEEE1394_INIT()\n");

    s=ieee1394_hwgraphregister();
    if (s<0) {
	printf("Error occured while creating hwgraph for ieee1394 driver.\n");
	return(ENOMEM);
    }

    tlabel=0; /* The initial value of tlabel. */

    /* Make sure that the shared variables have been initialized */
    ieee1394_shared_init();
    selfid_reset_state=0;
    SV_INIT(&tq_sv,SV_DEFAULT,"tq_sv");
    MUTEX_INIT(&tq_lock, MUTEX_DEFAULT, "tq_lock");
    initnsema(&selfid_thread_sema, 0, "sid_sema");

   /* "headp" points to the last msg taken by the thread_selfid_recv function.
       It points to a msg whose data is currently being used, and hence the
       interrupt routine board_driver_intr *must* not change selfid_msg_q.entry[headp].
       "tailp" points to either the last message *completely* processed by
       the thread_.._recv function or to the last message written into by
       the interrupt routine. Any change must be made to msg_q.entry[tailp].
   */
    selfid_msg_q.headp=0;
    selfid_msg_q.tailp=0;
    MUTEX_INIT(&selfid_msg_q_lock, MUTEX_DEFAULT, "selfidlock");
    initnsema(&asyrecv_thread_sema, 0, "asysema");
    asyrecv_msg_q.headp=MAX_ASYRECV_TRANSACTIONS_PENDING-1;
    asyrecv_msg_q.tailp=0;
    MUTEX_INIT(&asyrecv_msg_q_lock, MUTEX_DEFAULT, "asyrecvlock");

    /* Initialize the transaction layer variables */
    tx_state=TX0;
    tq_rotor_active=0;
    tq_rescan=0;
    tq_head=0;
    tq_tail=0;
    tq_rotor=0;

    /* Initialize the InterestTable */
    InterestTable.seq_no=0;
    InterestTable.nentries=0;
    prev=0;

    for (i=0; i<64; i++) {
     /* Allocate the UID_node_t structure */
     UID_nodep = (UID_node_t*) kmem_zalloc(sizeof(UID_node_t), KM_SLEEP);     if (UID_nodep==0) return(ENOMEM);

     UID_nodep->CurrentInterest=0;
     UID_nodep->Vendor_ID=0;
     UID_nodep->Device_ID=0;
     UID_nodep->UID=0;
     InterestTable.table[i].nodevalid_flag=1;
     InterestTable.table[i].UIDvalid_flag=1;
     InterestTable.table[i].nodep=UID_nodep;
     InterestTable.table[i].prevnodep=UID_nodep;

     for (j=0; j<MAX_INTEREST_PER_DEVICE; j++) {

        /* Allocate the node_t structure */
        currnode = (node_t*) kmem_zalloc(sizeof(node_t), KM_SLEEP);
        if (currnode==0) return(ENOMEM);

        /* Initialize the node_t structure */
        currnode->app_id=-1;
        currnode->begaddr=0;
        currnode->endaddr=0;
        currnode->fp=0;
        currnode->next=0;

        /* Link the node_t structure */
        if (j==0) {
            UID_nodep->next=currnode;
            prev=currnode;
        }
        else {
           prev->next= currnode;
           prev=currnode;
        }
     }
    }

    /* Start the thread that parses self-ids, and requests UIDs */
    sthread_create("thread_selfid_recv", 0, DKTSTKSZ, 0, 250, KT_PS,
                        (st_func_t *)thread_selfid_recv, 0, 0, 0, 0);

    /* Start the thread that parses the asy receive transactions and
       callbacks the appropriate Upper level driver (depending on the
       InterestTable */
    sthread_create("thread_asyrecv", 0, DKTSTKSZ, 0, 249, KT_PS,
                        (st_func_t *)thread_asyrecv, 0, 0, 0, 0);

    return(0);
}

int
ieee1394_ioctl(dev_t dev, int cmd, void *arg, int mode, cred_t *crp, int *rvalp)
{
    int ret;

    printf("ieee1394_IOCTL called: %d.\n", cmd);

    ret=0;
    switch (cmd) {
        case FW_READREG:
            ieee1394_ioctl_readreg(arg);
            break;
        case FW_WRITEREG:
            ieee1394_ioctl_writereg(arg);
            break;
        case FW_GET_MMAPSIZE:
	    *rvalp=ieee1394_ioctl_get_mmapsize();
            break;
        case FW_GET_TREE:
            ret=ieee1394_ioctl_get_tree(arg);
            break;
        case FW_QUADREAD:
            ret=ieee1394_ioctl_quadread(arg);
            break;
        case FW_QUADWRITE:
            ret=ieee1394_ioctl_quadwrite(arg);
            break;
        case FW_QUADLOCK:
            ret=ieee1394_ioctl_quadlock(arg);
            break;
        case FW_BLOCKREAD:
            ret=ieee1394_ioctl_blockread(arg);
            break;
        case FW_BLOCKWRITE:
            ret=ieee1394_ioctl_blockwrite(arg);
            break;
        case FW_PRINTINTERESTTABLE:
            ieee1394_print_InterestTable();
            break;
	case FW_TEST_ISOXMT:
	    ieee1394_ioctl_test_isoxmt(arg);
	    break;
        default:
            break;
    }
    return ret;
}

static void
ieee1394_ioctl_readreg(void *arg)
{
    int offset;
    int *iarg;
    int val;

    iarg=arg;
    copyin(iarg,&offset,4);
    val=board_driver_readreg(offset);
    copyout((caddr_t)&val,iarg+1,4);
}

static void
ieee1394_ioctl_writereg(void *arg)
{
    int offset;
    int *iarg;
    int val;

    iarg=arg;
    copyin(iarg,&offset,4);
    copyin(iarg+1,&val,4);
    board_driver_writereg(offset, val);
}

static int
ieee1394_ioctl_get_mmapsize()
{
   return(board_driver_get_mmapsize());
}

static int 
ieee1394_ioctl_get_tree(void *arg)
{
   int 		   tree_copy_size;
   self_id_db_t    *tree_db_snapshotp;
   int             beforeseqnum, afterseqnum;
   int             s;

    tree_db_snapshotp=kmem_zalloc(sizeof(self_id_db_t), KM_SLEEP);
    if (tree_db_snapshotp==0) return(ENOMEM);

    /* Wait until we are out of the reset state, exit on signal. */

    s=ieee1394_wait_for_reset();
    if(s) {
        kmem_free(tree_db_snapshotp, sizeof(self_id_db_t) );
	return s;
    }

    /* Read the tree and our own self-id info in the PHY.
       Note that only a self-id packet interrupt will change the bus
       reset sequence number, and it will always change the reset
       sequence number.  So, if the sequence number changes during
       the copyout, we must retry the copyout.  And if it doesn't
       change, then we can be sure that the self-id packet interrupt
       did not occur during the copyout. */

    while (1) {
        beforeseqnum = tree_db.bus_reset_sequence_number;
 	tree_copy_size= (char*)&tree_db.self_id_tab[tree_db.n_self_ids] -
            		(char*)&tree_db;
        bcopy(&tree_db, tree_db_snapshotp, tree_copy_size);
        afterseqnum = tree_db.bus_reset_sequence_number;
        if (beforeseqnum != afterseqnum)
            continue;

	copyout((caddr_t)tree_db_snapshotp, arg, tree_copy_size);
        break;
    }
    kmem_free(tree_db_snapshotp, sizeof(self_id_db_t) );
    return 0;
}

static int
ieee1394_ioctl_quadread(void *arg)
{
    int xferlen,*iarg;
    ioctl_quadread_t a;

    iarg=arg;
    copyin(arg,&a,sizeof(ioctl_quadread_t));
    ieee1394_ioctl_quadread_transaction.destination_id=0xffc0|a.destination_id;
    ieee1394_ioctl_quadread_transaction.tcode=IEEE1394_READ_REQUEST_QUAD;
    ieee1394_ioctl_quadread_transaction.tlabel=(tlabel++)%64;
    ieee1394_ioctl_quadread_transaction.priority=0;
    ieee1394_ioctl_quadread_transaction.source_id=0xffc0|tree_db.our_phy_ID; 
    ieee1394_ioctl_quadread_transaction.destination_offset=a.destination_offset;
    ieee1394_ioctl_quadread_transaction.data_length=4;
    ieee1394_ioctl_quadread_transaction.extended_tcode=0;
    ieee1394_ioctl_quadread_transaction.block_data_p = 
			&ieee1394_ioctl_quadread_word;
    ieee1394_ioctl_quadread_transaction.busy=1;
    ieee1394_ioctl_quadread_transaction.state=TQ_AWAITING_XMIT;
    ieee1394_ioctl_quadread_transaction.stat=TQ_STATUS_AWAITING;
    ieee1394_add_transaction(&ieee1394_ioctl_quadread_transaction);
    MUTEX_LOCK(&tq_lock, -1);
    while(1) {
        if(!ieee1394_ioctl_quadread_transaction.busy)break;
        SV_WAIT(&tq_sv,&tq_lock,0);
        MUTEX_LOCK(&tq_lock, -1);
    }
    MUTEX_UNLOCK(&tq_lock);
    copyout((caddr_t)&ieee1394_ioctl_quadread_word,iarg+5,4);
    ieee1394_remove_transaction(&ieee1394_ioctl_quadread_transaction);
    printf("ioctl_quadread status: %d\n", ieee1394_ioctl_quadread_transaction.stat);    
    return (ieee1394_ioctl_quadread_transaction.stat);
}

static int
ieee1394_ioctl_quadwrite(void *arg)
{
    int xferlen,*iarg;
    ioctl_quadwrite_t a;

    iarg=arg;
    copyin(arg,&a,sizeof(ioctl_quadwrite_t));
    ieee1394_ioctl_quadwrite_transaction.destination_id=0xffc0|a.destination_id;
    ieee1394_ioctl_quadwrite_transaction.tcode=IEEE1394_WRITE_REQUEST_QUAD;
    ieee1394_ioctl_quadwrite_transaction.tlabel=(tlabel++)%64;
    ieee1394_ioctl_quadwrite_transaction.priority=0;
    ieee1394_ioctl_quadwrite_transaction.source_id=0xffc0|tree_db.our_phy_ID; 
    ieee1394_ioctl_quadwrite_transaction.destination_offset=a.destination_offset;
    ieee1394_ioctl_quadwrite_transaction.data_length=4;
    ieee1394_ioctl_quadwrite_transaction.extended_tcode=0;
    ieee1394_ioctl_quadwrite_word=a.quadlet_data;
    ieee1394_ioctl_quadwrite_transaction.block_data_p = &ieee1394_ioctl_quadwrite_word; 
    ieee1394_ioctl_quadwrite_transaction.busy=1;
    ieee1394_ioctl_quadwrite_transaction.state=TQ_AWAITING_XMIT;
    ieee1394_ioctl_quadwrite_transaction.stat=TQ_STATUS_AWAITING;
    ieee1394_add_transaction(&ieee1394_ioctl_quadwrite_transaction);
    MUTEX_LOCK(&tq_lock, -1);
    while(1) {
        if(!ieee1394_ioctl_quadwrite_transaction.busy)break;
        SV_WAIT(&tq_sv,&tq_lock,0);
        MUTEX_LOCK(&tq_lock, -1);
    }
    MUTEX_UNLOCK(&tq_lock);
    ieee1394_remove_transaction(&ieee1394_ioctl_quadwrite_transaction);
    return (ieee1394_ioctl_quadwrite_transaction.stat);
}

static int
ieee1394_ioctl_quadlock(void *arg)
{
    int xferlen;
    ioctl_quadlock_t a;

    copyin(arg,&a,sizeof(ioctl_quadlock_t));
    ieee1394_ioctl_quadlock_transaction.destination_id=0xffc0|a.destination_id;
    ieee1394_ioctl_quadlock_transaction.tcode=IEEE1394_LOCK_REQUEST;
    ieee1394_ioctl_quadlock_transaction.tlabel=(tlabel++)%64;
    ieee1394_ioctl_quadlock_transaction.priority=0;
    ieee1394_ioctl_quadlock_transaction.source_id=0xffc0|tree_db.our_phy_ID; 
    ieee1394_ioctl_quadlock_transaction.destination_offset=a.destination_offset;
    ieee1394_ioctl_quadlock_transaction.data_length=8;
    ieee1394_ioctl_quadlock_transaction.extended_tcode=2; /* Compare & Swap */
    ieee1394_ioctl_quadlock_word[0]=a.quadlet_arg_value;
    ieee1394_ioctl_quadlock_word[1]=a.quadlet_data_value;
    ieee1394_ioctl_quadlock_transaction.block_data_p=ieee1394_ioctl_quadlock_word;
    ieee1394_ioctl_quadlock_transaction.busy=1;
    ieee1394_ioctl_quadlock_transaction.state=TQ_AWAITING_XMIT;
    ieee1394_ioctl_quadlock_transaction.stat=TQ_STATUS_AWAITING;
    ieee1394_add_transaction(&ieee1394_ioctl_quadlock_transaction);
    MUTEX_LOCK(&tq_lock, -1);
    while(1) {
        if(!ieee1394_ioctl_quadlock_transaction.busy)break;
        SV_WAIT(&tq_sv,&tq_lock,0);
        MUTEX_LOCK(&tq_lock, -1);
    }
    MUTEX_UNLOCK(&tq_lock);

    /* The statement below is a hack. The board-level driver stores the 
     * old_value returned by the compare & swap transaction in 
     * t->destination_offset in the indication function. This value is 
     * transferred to a.old_value here and "copyout"ed, so that the 
     * application can use it to determine if the lock was successful or not.
    */
    a.old_value = ieee1394_ioctl_quadlock_transaction.destination_offset;
    copyout((caddr_t)&a, arg, sizeof(ioctl_quadlock_t));
    ieee1394_remove_transaction(&ieee1394_ioctl_quadlock_transaction);
    printf("ioctl_quadlock status: %d\n", ieee1394_ioctl_quadlock_transaction.stat);
    return (ieee1394_ioctl_quadlock_transaction.stat);
}

static int
ieee1394_ioctl_blockread(void *arg)
{
    int iarg;
    ioctl_blockread_t a;

    copyin(arg,&a,sizeof(ioctl_blockread_t));
    ieee1394_ioctl_blockread_transaction.destination_id=0xffc0|a.destination_id;
    ieee1394_ioctl_blockread_transaction.tlabel=(tlabel++)%64;
    ieee1394_ioctl_blockread_transaction.tcode=IEEE1394_READ_REQUEST_BLOCK;
    ieee1394_ioctl_blockread_transaction.priority=0;
    ieee1394_ioctl_blockread_transaction.source_id=0xffc0|tree_db.our_phy_ID; 
    ieee1394_ioctl_blockread_transaction.destination_offset=a.destination_offset;
    ieee1394_ioctl_blockread_transaction.data_length=a.data_length;
    /* In general reads dont have any compare & swaps etc.,
       so, extended_tcode will be ignored (unless tcode=LOCK) */

    /* XXX:Dont use (int*) a.payload since it is a local stack variable */
    ieee1394_ioctl_blockread_transaction.block_data_p=(int *)a.payload;
    ieee1394_ioctl_blockread_transaction.busy=1;
    ieee1394_ioctl_blockread_transaction.state=TQ_AWAITING_XMIT;
    ieee1394_ioctl_blockread_transaction.stat=TQ_STATUS_AWAITING;

    ieee1394_add_transaction(&ieee1394_ioctl_blockread_transaction);

    MUTEX_LOCK(&tq_lock, -1);
    while(1) {
        if(!ieee1394_ioctl_blockread_transaction.busy)break;
        SV_WAIT(&tq_sv,&tq_lock,0);
        MUTEX_LOCK(&tq_lock, -1);
    }
    MUTEX_UNLOCK(&tq_lock);

    /* XXX: Need to implement IEEE1394_READ_RESPONSE_BLOCK processing in
       link_data_indication that sets the payload field in 
       ieee1394_ioctl_blockread_transaction. */

    copyout((caddr_t)&ieee1394_ioctl_blockread_transaction, arg, 
			sizeof(ioctl_blockread_t));
    ieee1394_remove_transaction(&ieee1394_ioctl_blockread_transaction);
    printf("ioctl_blockread status: %d\n",
                ieee1394_ioctl_blockread_transaction.stat);
    return (ieee1394_ioctl_blockread_transaction.stat);
}


static int
ieee1394_ioctl_blockwrite(void *arg)
{
    int iarg;
    ioctl_blockwrite_t a;

    copyin(arg,&a,sizeof(ioctl_blockwrite_t));
    ieee1394_ioctl_blockwrite_transaction.destination_id=0xffc0|a.destination_id;
    ieee1394_ioctl_blockwrite_transaction.tlabel=(tlabel++)%64;
    ieee1394_ioctl_blockwrite_transaction.tcode=IEEE1394_WRITE_REQUEST_BLOCK;
    ieee1394_ioctl_blockwrite_transaction.priority=0;
    ieee1394_ioctl_blockwrite_transaction.source_id=0xffc0|tree_db.our_phy_ID; 
    ieee1394_ioctl_blockwrite_transaction.destination_offset=a.destination_offset;
    ieee1394_ioctl_blockwrite_transaction.data_length=a.data_length;
    /* If tcode = LOCK, then extended_tcode matters, else it is
       ignored. */
    ieee1394_ioctl_blockwrite_transaction.block_data_p=(int*)a.payload;

    ieee1394_ioctl_blockwrite_transaction.busy=1;
    ieee1394_ioctl_blockwrite_transaction.state=TQ_AWAITING_XMIT;
    ieee1394_ioctl_blockwrite_transaction.stat=TQ_STATUS_AWAITING;

    ieee1394_add_transaction(&ieee1394_ioctl_blockwrite_transaction);

    MUTEX_LOCK(&tq_lock, -1);
    while(1) {
        if(!ieee1394_ioctl_blockwrite_transaction.busy)break;
        SV_WAIT(&tq_sv,&tq_lock,0);
        MUTEX_LOCK(&tq_lock, -1);
    }
    MUTEX_UNLOCK(&tq_lock);

    copyout((caddr_t)&ieee1394_ioctl_blockwrite_transaction, arg, 
		sizeof(ioctl_blockwrite_t));
    ieee1394_remove_transaction(&ieee1394_ioctl_blockwrite_transaction);
    printf("ioctl_blockwrite status: %d\n", ieee1394_ioctl_blockwrite_transaction.stat);

    return (ieee1394_ioctl_blockwrite_transaction.stat);
}

/* This is a callback function called by board-level drivers, when an
   interrupt occurs. 'op' tells us which interrupt occured. 'funp'
   is the function (pointer) in the board-level driver that does
   post-processing */
void
ieee1394_interrupt(int op, int *param, void (*funp)(int op))
{
   int tailp;

   switch(op) {
      case SIDRCV:
   	printf("IEEE1394_INTERRUPT called: SIDRCV\n");
	tailp=(*param);

        MUTEX_LOCK(&selfid_msg_q_lock, -1);
         if (selfid_msg_q.headp==selfid_msg_q.tailp) {
             /* Write the new one at tailp and increment tailp%2 */
             selfid_msg_q.entry[selfid_msg_q.tailp].bus_reset_sequence_no=
                    tree_db.bus_reset_sequence_number;

             selfid_msg_q.entry[selfid_msg_q.tailp].sid_rcv_tailp=tailp;
	     selfid_msg_q.entry[selfid_msg_q.tailp].post_funp=funp;
             selfid_msg_q.tailp=(selfid_msg_q.tailp+1)%2;
         }
         else {
            /* Overwrite the new entry at tailp, dont incr tailp*/
            selfid_msg_q.entry[selfid_msg_q.tailp].bus_reset_sequence_no=
                    tree_db.bus_reset_sequence_number;
            selfid_msg_q.entry[selfid_msg_q.tailp].sid_rcv_tailp=tailp;
	    selfid_msg_q.entry[selfid_msg_q.tailp].post_funp=funp;
         }
         MUTEX_LOCK(&bus_reset_lock,-1);
            bus_reset_state=0;
            selfid_reset_state=1;
            /* XXX: This is a hack, please change it, initialize sema_t to 1. */
            if (valusema(&selfid_thread_sema)==-1) vsema(&selfid_thread_sema);
         MUTEX_UNLOCK(&bus_reset_lock);
        MUTEX_UNLOCK(&selfid_msg_q_lock);
	
	break;
      case ASYRCV:
      {
	int *datap, *headerp, *cyclecounter, *lenp;
        int packetlen;
	int s;

        printf("IEEE1394_INTERRUPT called: ASYRCV\n");
	s=board_driver_get_headerlist_info(ASYRCV, *param, &headerp, 
				&cyclecounter, &lenp, &datap);
	if (s) {
	   printf("board_driver_get_headerlist_info failed in ieee1394_interrupt.\n");
	   return;
	}
        dki_dcache_inval((caddr_t)headerp,16);
        packetlen=(*lenp & 0x7ff)-4;
        if(packetlen > 0) {
             dki_dcache_inval((caddr_t)datap,packetlen);
        }
        ieee1394_link_data_indication(headerp, cyclecounter, lenp, datap);
      } 
	break;
      case ACKRCV:
        printf("IEEE1394_INTERRUPT called: ACKRCV\n");
        ieee1394_link_data_confirmation(tq_transaction_p, *param);
	break;
      case BUSRESET:
        printf("IEEE1394_INTERRUPT called: BUSRESET\n");
	break;
      default:
	printf("IEEE1394_INTERRUPT called: Unknown interrupt callback.\n");
   }
}


/* This is an ack for a transaction. If the ack is pending: ack_type = 0 &
 * ack = 2, then the transaction.stat must be left as TQ_STATUS_AWAITING
 * and it will be set in the link_data_indication function to rcode. Else
 * the transaction.stat must be set based on the ack in this function.
*/

static void
ieee1394_link_data_confirmation(ieee1394_transaction_t *t, int status)
{
    int ack_type,ack;

    ack_type=(status>>14)&1;
    ack=(status>>15)&0xf;

    if(ack_type==0) {
        switch (ack) {
            case 0:   /* reserved */
            case 3:   /* reserved */
            case 7:   /* reserved */
            case 8:   /* reserved */
            case 9:   /* reserved */
            case 0xa: /* reserved */
            case 0xb: /* reserved */
            case 0xc: /* reserved */
            case 0xf: /* reserved */
                MUTEX_LOCK(&tq_lock, -1);
                t->stat=TQ_STATUS_RESERVED;
                t->busy=0;
                t->state=TQ_FINISHED;
                printf("normal ack: RESERVED: %s.\n", normal_acks[ack]);
                SV_BROADCAST(&tq_sv);
                MUTEX_UNLOCK(&tq_lock);
                break;
            case 1:   /* ack_complete */
                MUTEX_LOCK(&tq_lock, -1);
                t->stat=TQ_STATUS_SUCCESS;
                t->busy=0;
                t->state=TQ_FINISHED;
                printf("normal ack: SUCCESS.\n");
                SV_BROADCAST(&tq_sv);
                MUTEX_UNLOCK(&tq_lock);
                break;
            case 2:   /* ack_pending */
                MUTEX_LOCK(&tq_lock, -1);
                t->state=TQ_AWAITING_CONF;
                printf("normal ack: PENDING.\n");
                MUTEX_UNLOCK(&tq_lock);
                break;
            case 4:   /* ack_busy_X */
            case 5:   /* ack_busy_A */
            case 6:   /* ack_busy_B */
                /* XXX Need to limit to a finite number of retries */
                MUTEX_LOCK(&tq_lock, -1);
                t->state=TQ_AWAITING_XMIT;
                printf("normal ack: BUSY, Retrying.\n");
                MUTEX_UNLOCK(&tq_lock);

                /* XXX: Need to replace with some thread semantics */
                ieee1394_activate_tq_rotor();
                break;
            case 0xd:   /* ack_data_error */
                MUTEX_LOCK(&tq_lock, -1);
                t->stat=TQ_STATUS_FAILED_CRC_OR_DATA_LENGTH_ERROR;
                t->busy=0;
                t->state=TQ_FINISHED;
                printf("normal ack: FAILED CRC OR DATA LENGTH ERROR.\n");
                MUTEX_UNLOCK(&tq_lock);
                break;
            case 0xe:   /* ack_data_error */
                MUTEX_LOCK(&tq_lock, -1);
                t->stat=TQ_STATUS_INVALID_TRANSACTION_ERROR;
                t->busy=0;
                t->state=TQ_FINISHED;
                printf("normal ack: INVALID TRANSACTION ERROR.\n");
                MUTEX_UNLOCK(&tq_lock);
                break;
        }
    } else if(ack_type==1) {
        switch(ack) {
            case 0:   /* link reported a retry overrun */
                /* XXX Need to limit to a finite number of retries */
                MUTEX_LOCK(&tq_lock, -1);
                t->state=TQ_AWAITING_XMIT;
                MUTEX_UNLOCK(&tq_lock);
                ieee1394_activate_tq_rotor();
                break;
            case 1:   /* link reported a timeout */
            case 2:   /* link reported a FIFO underrun */
            case 3:   /* reserved */
            case 4:   /* reserved */
            case 5:   /* No expected End of receive Packet */
            case 6:   /* Pipelined Async Transmit Command encountered a command
other than another Async Transmit */
            case 7:   /* reserved */
            case 8:   /* reserved */
            case 9:   /* reserved */
            case 0xa: /* reserved */
            case 0xb: /* reserved */
            case 0xc: /* reserved */
            case 0xd: /* reserved */
            case 0xe: /* Link reported a corrupted header before the packet was
transmitted */
            case 0xf: /* reserved */
               /* XXX should indicate an error here */
                MUTEX_LOCK(&tq_lock, -1);
                t->stat=TQ_STATUS_SPECIAL_RESERVED;
                t->busy=0;
                t->state=TQ_FINISHED;
        printf("special ack  %d %s\n",ack,special_acks[ack]);
                SV_BROADCAST(&tq_sv);
                MUTEX_UNLOCK(&tq_lock);
                break;
        }
    }
}

static void
ieee1394_link_data_indication(int *hdrp, int *cyclecounter, int *lenp, 
				int *datap)
{
    int hdr;
    int len;
    int i;
    int tcode;
    int tlabel;
    int destination_id;
    int source_id;
    int rcode;
    long long destination_offset;
    ieee1394_transaction_t *tqp;

    len=0x7ff&(*lenp);
    hdr=(*hdrp);

    tcode=(hdr&0xf0)>>4;
    if(tcode == IEEE1394_READ_RESPONSE_QUAD) {
        destination_id=(hdr>>16)&0xffff;
        source_id=(datap[0]>>16)&0xffff;
        rcode=(datap[0]>>12)&0xf;
        tlabel=(hdr>>10)&0x3f;
        printf("Got: Read Response Quad: %d.\n", tlabel);
        MUTEX_LOCK(&tq_lock, -1);
        tqp=tq_head;
        while(tqp) {
            if(tqp->destination_id == source_id &&
               tqp->tlabel == tlabel) {
                tqp->block_data_p[0]=datap[2];
                switch (rcode) {
                  case 0: tqp->stat=TQ_STATUS_SUCCESS;
                          break;
                  case 1:
                  case 2:
                  case 3:
                  case 8:
                  case 9:
                  case 0xa:
                  case 0xb:
                  case 0xc:
                  case 0xd:
                  case 0xe:
                  case 0xf:  tqp->stat=TQ_STATUS_RESERVED;
                             break;
                  case 4: tqp->stat=TQ_STATUS_CONFLICT_ERROR;
                          break;
                  case 5: tqp->stat=TQ_STATUS_DATA_UNAVAILABLE_ERROR;
                          break;
                  case 6: tqp->stat=TQ_STATUS_INVALID_TRANSACTION_ERROR;
                          break;
                  case 7: tqp->stat=TQ_STATUS_INVALID_ADDRESS_ERROR;
                          break;
                }
                printf("Indication status: %d\n", tqp->stat);
                tqp->busy=0;
                tqp->state=TQ_FINISHED;
                SV_BROADCAST(&tq_sv);
                break;
            } else {
            }
            tqp=tqp->next;
        }
        MUTEX_UNLOCK(&tq_lock);
    }
    else if (tcode == IEEE1394_WRITE_RESPONSE) {
        destination_id=(hdr>>16)&0xffff;
        source_id=(datap[0]>>16)&0xffff;
        rcode=(datap[0]>>12)&0xf;
        tlabel=(hdr>>10)&0x3f;
        printf("Got: Write Response: %d 0x%x.\n", tlabel, source_id);

        /* Now that we've received an indication, we should reset the
           write_request_transaction->busy. Also we should set the
           write_request_transaction->state = TQ_FINISHED.
        */

        MUTEX_LOCK(&tq_lock, -1);
        tqp=tq_head;
        while(tqp) {
            if(tqp->destination_id == source_id &&
               tqp->tlabel == tlabel) {
                switch (rcode) {
                  case 0: tqp->stat=TQ_STATUS_SUCCESS;
                          break;
                  case 1:
                  case 2:
                  case 3:
                  case 8:
                  case 9:
                  case 0xa:
                  case 0xb:
                  case 0xc:
                  case 0xd:
                  case 0xe:
                  case 0xf:  tqp->stat=TQ_STATUS_RESERVED;
                             break;
                  case 4: tqp->stat=TQ_STATUS_CONFLICT_ERROR;
                          break;
                  case 5: tqp->stat=TQ_STATUS_DATA_UNAVAILABLE_ERROR;
                          break;
                  case 6: tqp->stat=TQ_STATUS_INVALID_TRANSACTION_ERROR;
                          break;
                  case 7: tqp->stat=TQ_STATUS_INVALID_ADDRESS_ERROR;
                          break;
                }
                printf("Indication status: %d\n", tqp->stat);
                tqp->busy=0;
                tqp->state=TQ_FINISHED;
                SV_BROADCAST(&tq_sv);
                break;
            } else {
            }
            tqp=tqp->next;
        }
        MUTEX_UNLOCK(&tq_lock);
    }
    else if (tcode == IEEE1394_LOCK_RESPONSE) {
        long long old_value;

        destination_id=(hdr>>16)&0xffff;
        tlabel=(hdr>>10)&0x3f;
        source_id=(datap[0]>>16)&0xffff;
        rcode=(datap[0]>>12)&0xf;
        old_value = datap[3];
        printf("Got: Lock Response: %d.\n", tlabel);

        /* Now that we've received an indication, we should reset the
           lock_request_transaction->busy. Also we should set the
           lock_request_transaction->state = TQ_FINISHED.
        */

        MUTEX_LOCK(&tq_lock, -1);
        tqp=tq_head;
        while(tqp) {
            if(tqp->destination_id == source_id &&
               tqp->tlabel == tlabel) {
                tqp->block_data_p[0]=datap[2];
                tqp->destination_offset = old_value;
                switch (rcode) {
                  case 0: tqp->stat=TQ_STATUS_SUCCESS;
                          break;
                  case 1:
                  case 2:
                  case 3:
                  case 8:
                  case 9:
                  case 0xa:
                  case 0xb:
                  case 0xc:
                  case 0xd:
                  case 0xe:
                  case 0xf: tqp->stat=TQ_STATUS_RESERVED;
                             break;
                  case 4: tqp->stat=TQ_STATUS_CONFLICT_ERROR;
                          break;
                  case 5: tqp->stat=TQ_STATUS_DATA_UNAVAILABLE_ERROR;
                          break;
                  case 6: tqp->stat=TQ_STATUS_INVALID_TRANSACTION_ERROR;
                          break;
                  case 7: tqp->stat=TQ_STATUS_INVALID_ADDRESS_ERROR;
                          break;
                }
                printf("Indication status: %d\n", tqp->stat);
                tqp->busy=0;
                tqp->state=TQ_FINISHED;
                SV_BROADCAST(&tq_sv);
                break;
            } else {
            }
            tqp=tqp->next;
        }
        MUTEX_UNLOCK(&tq_lock);
    }
    else if ((tcode == IEEE1394_WRITE_REQUEST_BLOCK) ||
             (tcode ==IEEE1394_WRITE_REQUEST_QUAD)   ||
             (tcode==IEEE1394_READ_REQUEST_QUAD)     ||
             (tcode==IEEE1394_READ_REQUEST_BLOCK)    ||
             (tcode==IEEE1394_LOCK_REQUEST)) {
        destination_id=(hdr>>16)&0x3f;
        source_id=(datap[0]>>16)&0x3f;
        tlabel=(hdr>>10)&0x3f;
        destination_offset=(datap[0]&0xffff);
        destination_offset=destination_offset<<32;
        destination_offset|=datap[1];

        MUTEX_LOCK(&asyrecv_msg_q_lock, -1);
          /* Check to see if the asyrecv_msg_q ring buffer has overflowed */
          if (asyrecv_msg_q.tailp==asyrecv_msg_q.headp) {
              printf("ASYRECV ring buffer overflowed. \n");
              printf("Increase MAX_ASYRECV_TRANSACTIONS_PENDING in ieee1394_kern.h and retry.\n");
              MUTEX_UNLOCK(&asyrecv_msg_q_lock);
              return;
          }

          asyrecv_msg_q.entry[asyrecv_msg_q.tailp].callback_code=tcode;
          asyrecv_msg_q.entry[asyrecv_msg_q.tailp].source_id=source_id;
          asyrecv_msg_q.entry[asyrecv_msg_q.tailp].destination_id=destination_id;
          asyrecv_msg_q.entry[asyrecv_msg_q.tailp].destination_offset =
                                                   destination_offset;
          asyrecv_msg_q.entry[asyrecv_msg_q.tailp].tlabel=tlabel;
          switch(tcode) {
            case IEEE1394_READ_REQUEST_QUAD:
		  printf("IEEE1394_READ_REQUEST_QUAD received\n");
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].data_length=0;
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].extended_tcode=-1;
                  break;
            case IEEE1394_READ_REQUEST_BLOCK:
		  printf("IEEE1394_READ_REQUEST_BLOCK received\n");
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].data_length =
                                (datap[2]>>16)&0xffff;
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].extended_tcode =
                                datap[2] & 0xffff;
                  break;
            case IEEE1394_WRITE_REQUEST_QUAD:
		  printf("IEEE1394_WRITE_REQUEST_QUAD received\n");
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].data_length=4;
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].extended_tcode=-1;
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].payload[0]=datap[2];
                  break;
            case IEEE1394_WRITE_REQUEST_BLOCK:
		  printf("IEEE1394_WRITE_REQUEST_BLOCK received\n");
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].data_length =
                                (datap[2]>>16)&0xffff;
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].extended_tcode =
                                datap[2] & 0xffff;
                  for (i=0; i<asyrecv_msg_q.entry[asyrecv_msg_q.tailp].data_length;
                        i+=4) {
                     asyrecv_msg_q.entry[asyrecv_msg_q.tailp].payload[0]=datap[3+i/4];
                  }
                  break;
            case IEEE1394_LOCK_REQUEST:
		  printf("IEEE1394_LOCK_REQUEST received\n");
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].data_length =
                                (datap[2]>>16)&0xffff;
                  asyrecv_msg_q.entry[asyrecv_msg_q.tailp].extended_tcode =
                                datap[2] & 0xffff;
                  for (i=0; i<asyrecv_msg_q.entry[asyrecv_msg_q.tailp].data_length;
                        i+=4) {
                     asyrecv_msg_q.entry[asyrecv_msg_q.tailp].payload[0]=datap[3+i/4];
                  }
                  break;
          }
        /* Now increment asyrecv_msg_q.tailp */
        asyrecv_msg_q.tailp=(asyrecv_msg_q.tailp+1) % MAX_ASYRECV_TRANSACTIONS_PENDING;

        /* Wake up the thread_asyrecv to handle the asynchronous transaction */
        vsema(&asyrecv_thread_sema);

        MUTEX_UNLOCK(&asyrecv_msg_q_lock);
    }
    else {
        /* Unknown Indication: Display tcode and release the lock. */
        destination_id=(hdr>>16)&0xffff;
        source_id=(datap[0]>>16)&0xffff;
        rcode=(datap[0]>>12)&0xf;
        tlabel=(hdr>>10)&0x3f;
        printf("Unknown Indication: tcode=%d\n", tcode);
        printf("2nd word: 0x%x\n", datap[0]);
        printf("3rd word: 0x%x\n", datap[1]);
        printf("4th word: 0x%x\n", datap[2]);
        printf("FCP word: 0x%x\n", datap[3]);
        printf("FCP word2: 0x%x\n", datap[4]);

        MUTEX_LOCK(&tq_lock, -1);
        tqp=tq_head;
        while(tqp) {
            if(tqp->destination_id == source_id &&
               tqp->tlabel == tlabel) {
                tqp->busy=0;
                tqp->state=TQ_FINISHED;
                SV_BROADCAST(&tq_sv);
                break;
            }
            else {
            }
            tqp=tqp->next;
        }
        MUTEX_UNLOCK(&tq_lock);
    }
}


/* ieee1394_gain_interest is called by Upper level driver to gain
   interest in a certain node (or device). It specifies:
   Vendor_ID: Vendor ID of the device interested in. (Must be specified).
   Device_Id: Device_ID of the device (if available, else 0).
    They form UID    : Unique (device) ID of the device it is interested in.
   app_id : Unique id for each upper level driver that differentiates
            each application request.
   begaddr: Beginning addr (for a range), Exact address,
            or 0 (for callback when the device ceases to exist).
   endaddr: End addr (for a range), or 0.
   fp     : Function Pointer (callback function pointer).
*/

int
ieee1394_gain_interest(long long Vendor_ID, long long Device_ID, int app_id,
                       long long begaddr, long long endaddr, fun_t fp)
{
  node_t *nodep;
  long long UID;
  int i;

  if (fp==0) return(INVALID_FP);
  UID=0;
  if (Device_ID != 0) {
    UID=(Vendor_ID<<40)&0xffffff | (Device_ID<<24)&0xffffffffff;
  }

  MUTEX_LOCK(&InterestTable_lock, -1);
  for (i=0; i<InterestTable.nentries; i++) {
    /* Locate the node_id, or i */
    if (UID) {
        if (InterestTable.table[i].nodep->UID == UID) break;
    }
    else if (InterestTable.table[i].nodep->Vendor_ID==Vendor_ID) break;
  }
  if (i==InterestTable.nentries) {
     MUTEX_UNLOCK(&InterestTable_lock);
     return (INVALID_UID);
  }

  if (InterestTable.table[i].nodep->CurrentInterest < MAX_INTEREST_PER_DEVICE) {     
     InterestTable.table[i].nodep->CurrentInterest++;
     nodep=InterestTable.table[i].nodep->next;
     while (nodep->fp != 0) {
        nodep=nodep->next;
     }
     if (!nodep) {
        printf("Fatal error occured. Should not happen\n");
        MUTEX_UNLOCK(&InterestTable_lock);
        return(FATAL_ERROR);
     }
     nodep->app_id=app_id;
     nodep->begaddr=begaddr;
     nodep->endaddr=endaddr;
     nodep->fp=fp;
  }
  else {
     MUTEX_UNLOCK(&InterestTable_lock);
     return (MAX_INTEREST_EXCEEDED);
  }

  MUTEX_UNLOCK(&InterestTable_lock);
  return (0); /* SUCCESS */
}

/* ieee1394_lose_interest is called by the Upper level driver to lose interest 
   in a certain node (or device). It specifies:
   app_id : Application id (unique for each driver).
   Vendor_ID: Vendor ID of the device interested in. (Must be specified).
   Device_Id: Device_ID of the device (if available, else 0).
    They form UID: The device ID in which it is no more interested.
   begaddr: Beg addr of the entry.
   endaddr: End addr of the entry.
   fp     : function pointer of the entry.
   app_id & the last 3 params uniquely identify the node_t that must be
   removed the list.
*/

int
ieee1394_lose_interest(long long Vendor_ID, long long Device_ID, int app_id,
             		long long begaddr, long long endaddr, fun_t fp)
{
   node_t *nodep;
   node_t *nextnodep;
   long long UID;
   int i;

   if (fp==0) return (INVALID_FP);

   UID=0;
   if (Device_ID != 0) {
     UID=(Vendor_ID<<40)&0xffffff | (Device_ID<<24)&0xffffffffff;
   }

   /* Find the UID in the InterestTable */
   MUTEX_LOCK(&InterestTable_lock, -1);
   for (i=0; i<InterestTable.nentries; i++) {
    if (UID) {
        if (InterestTable.table[i].nodep->UID == UID) break;
    }
    else if (InterestTable.table[i].nodep->Vendor_ID == Vendor_ID) break;
   }

   if (i==InterestTable.nentries) {
     MUTEX_UNLOCK(&InterestTable_lock);
     return (INVALID_UID);
   }

   if (InterestTable.table[i].nodep->CurrentInterest==0) {
     MUTEX_UNLOCK(&InterestTable_lock);
     return (INVALID_INTEREST_PARAMS);
   }
   nodep = InterestTable.table[i].nodep->next;

   /* Find the node in the i'th bucket which needs to be removed */
   while ((nodep->fp != fp) || (nodep->begaddr != begaddr) ||
          (nodep->endaddr != endaddr) || (nodep->app_id != app_id)) {
        nodep = nodep->next;
        if (nodep==0) {
            MUTEX_UNLOCK(&InterestTable_lock);
            return (INVALID_INTEREST_PARAMS);
        }
   }

   /* To remove the node, we pull all nodes after it 1 space up */
   InterestTable.table[i].nodep->CurrentInterest--;
   nextnodep=nodep->next;
   if (nextnodep != 0) {
       while (nextnodep->fp != 0) {
          nodep->app_id =nextnodep->app_id;
          nodep->begaddr=nextnodep->begaddr;
          nodep->endaddr=nextnodep->endaddr;
          nodep->fp     =nextnodep->fp;
          nodep=nextnodep;
          nextnodep=nextnodep->next;
          if (nextnodep==0) break;
       }
   }

   /* Remove the node by re-initializing */
   nodep->app_id=-1;
   nodep->begaddr=0;
   nodep->endaddr=0;
   nodep->fp=0;

   MUTEX_UNLOCK(&InterestTable_lock);
   return(0); /* SUCCESS */
}

/* ieee1394_update_InterestTable() updates the interest table based on the
   new tree that has been created after the bus-reset. It updates
   the table, inserts a new UID for each new device, and calls back
   each function (pointed to by fp) if a device (and hence UID) ceases
   to exist.
*/

static int
ieee1394_update_InterestTable(void)
{
   int nextnode, n_self_ids;
   node_t *nextnodep;
   callback_t callback_pkt;
   int status;
   int i, j;

   /* Get the latest UID info */

   MUTEX_LOCK(&InterestTable_lock, -1);

   n_self_ids=tree_db.n_self_ids;
   nextnode=InterestTable.nentries;

   /* Initialize all flags: max (nextnode+n_self_ids) */
   for (i=0; i<InterestTable.nentries+n_self_ids; i++) {
     InterestTable.table[i].nodevalid_flag=0;
     InterestTable.table[i].UIDvalid_flag=0;
   }

   for (i=0; i<tree_db.n_self_ids; i++) {
     /* Find tree_db.self_id_tab[i].UID in InterestTable */
     if (tree_db.self_id_tab[i].UID == 0) {
        /* n_self_ids = tree_db.n_self_ids - Invalid UIDs */
        n_self_ids--;
        continue;
     }

     for (j=0; j<InterestTable.nentries; j++) {
        if (InterestTable.table[j].nodep->UID==
                tree_db.self_id_tab[i].UID)
            break;
     }
     if (j==InterestTable.nentries) {
        /* Not Found in table */
        InterestTable.table[i].nodep=InterestTable.table[nextnode].prevnodep;
        InterestTable.table[i].nodep->Vendor_ID=tree_db.self_id_tab[i].Vendor_ID;
        InterestTable.table[i].nodep->Device_ID=tree_db.self_id_tab[i].Device_ID;
        InterestTable.table[i].nodep->UID=tree_db.self_id_tab[i].UID;
        InterestTable.table[i].nodep->CurrentInterest=0;
        InterestTable.table[i].nodevalid_flag=1;
        InterestTable.table[nextnode].UIDvalid_flag=1;
        nextnode++;
        printf("Not found UID: 0x%llx, matched: 0x%llx\n",
			 tree_db.self_id_tab[i].UID,
                	 InterestTable.table[i].nodep->UID);
     }
     else {
        /* Found in table */
        InterestTable.table[i].nodep=InterestTable.table[j].prevnodep;
        InterestTable.table[i].nodevalid_flag=1;
        InterestTable.table[j].UIDvalid_flag=1;
        printf("Found UID: 0x%llx, matched: 0x%llx\n",
                tree_db.self_id_tab[i].UID,
                InterestTable.table[i].nodep->UID);
     }
   }

   /* Link the nodep of new nodes to dangling linked lists (of UIDs)
      that have ceased to exist). */
   for (i=n_self_ids; i<nextnode; i++) {
     if (InterestTable.table[i].nodevalid_flag==1) {
        printf("Fatal error: This node nodevalid_flag must be 0\n");
        MUTEX_UNLOCK(&InterestTable_lock);
        return (FATAL_ERROR);
     }
     if (InterestTable.table[i].UIDvalid_flag==1) {
        /* This guy must be linked to some node with nodevalid_flag==0 */
        for (j=0; j<nextnode; j++) {
           if ((InterestTable.table[j].UIDvalid_flag==0) &&
               (InterestTable.table[j].nodevalid_flag==1)) {
                 InterestTable.table[i].nodep=InterestTable.table[j].prevnodep;
                 InterestTable.table[i].nodevalid_flag=1;
           }
        }
        if (j==nextnode) {
           printf("Fatal error: Have a node without a link list, but no \
                        link list without a valid node.\n");
            MUTEX_UNLOCK(&InterestTable_lock);
            return (FATAL_ERROR);
        }
     }
     else {
        /* No need to change this node's nodep */
     }
   }

   /* Now reassign all prevnodep=nodep */
   for (i=0; i<nextnode; i++)
     InterestTable.table[i].prevnodep=InterestTable.table[i].nodep;

   /* Find all UIDs that have ceased to exist (table[i].UIDvalid_flag==0).
      Call the callback functions, and reinitialize the InterestTable.table[i] 
      & node_t structure. */

   callback_pkt.callback_code=-1; /* -1 indicates DEVICE_DISAPPEARED */
   for (i=0; i<nextnode; i++) {
      if (InterestTable.table[i].UIDvalid_flag==0) {
        printf("UID: 0x%llx is gone...\n", InterestTable.table[i].nodep->UID);
        nextnodep=InterestTable.table[i].nodep->next;
        while (InterestTable.table[i].nodep->CurrentInterest != 0) {
          callback_pkt.app_id=nextnodep->app_id;
          /* Callback the function */
          (*(nextnodep->fp))((void*) &callback_pkt);
          /* Re-initialize the node */
          nextnodep->begaddr=0;
          nextnodep->endaddr=0;
          nextnodep->fp     =0;
          nextnodep->app_id =-1;
          InterestTable.table[i].nodep->CurrentInterest--;
          nextnodep=nextnodep->next;
        }
        InterestTable.table[i].nodep->Vendor_ID=0;
        InterestTable.table[i].nodep->Device_ID=0;
        InterestTable.table[i].nodep->UID=0;
        InterestTable.table[i].nodep->CurrentInterest=0;
        InterestTable.table[i].UIDvalid_flag=1;
      }
   }

   /* Initialize all flags: max (nextnode+n_self_ids) */
   for (i=0; i<InterestTable.nentries+tree_db.n_self_ids; i++) {     
     InterestTable.table[i].nodevalid_flag=1;
     InterestTable.table[i].UIDvalid_flag=1;
   }

   InterestTable.seq_no=tree_db.bus_reset_sequence_number;
   InterestTable.nentries=n_self_ids;

   MUTEX_UNLOCK(&InterestTable_lock);
   return (0);
}

/* ieee1394_free_mem_InterestTable free's the memory allocated for
   InterestTable global structure. Does not return anything.
*/

static void
ieee1394_free_mem_InterestTable(void)
{
  int i, j;
  UID_node_t *UID_nodep;
  node_t *next;
  node_t *curr;

  for (i=0; i<64; i++) {
    UID_nodep=InterestTable.table[i].nodep;
    curr=UID_nodep->next;
    free(UID_nodep);
    for (j=0; j<MAX_INTEREST_PER_DEVICE; j++) {
      next=curr->next;
      free(curr);
    }
  }
}

static void
ieee1394_print_InterestTable(void)
{
   int i, j;
   node_t *nodep;

   MUTEX_LOCK(&InterestTable_lock, -1);

   printf("=============================================\n");
   printf("InterestTable.seq_no: %lld\n", InterestTable.seq_no);
   printf("InterestTable.nentries: %d\n", InterestTable.nentries);

   for (i=0; i<InterestTable.nentries; i++) {
        printf("Entry: %d\n", i);
        printf("nodep: %p\n", InterestTable.table[i].nodep);
        printf("prevnodep: %p\n", InterestTable.table[i].prevnodep);
        printf("nodevalid_flag: %d\n", InterestTable.table[i].nodevalid_flag);
        printf("UIDvalid_flag: %d\n", InterestTable.table[i].UIDvalid_flag);
        printf("Vendor_ID: 0x%llx\n", InterestTable.table[i].nodep->Vendor_ID);
        printf("Device_ID: 0x%llx\n", InterestTable.table[i].nodep->Device_ID);
        printf("UID: 0x%llx\n", InterestTable.table[i].nodep->UID);
        printf("CurrentInterest: %d\n",
                        InterestTable.table[i].nodep->CurrentInterest);
        nodep = InterestTable.table[i].nodep->next;
        for (j=0; j<InterestTable.table[i].nodep->CurrentInterest; j++) {
           printf("    app_id : %d\n", nodep->app_id);
           printf("    begaddr: 0x%llx\n", nodep->begaddr);
           printf("    endaddr: 0x%llx\n", nodep->endaddr);
           printf("    fp     : 0x%x\n", nodep->fp     );
           nodep=nodep->next;
        }
   }
   printf("-----------------------------------------------\n\n");

   MUTEX_UNLOCK(&InterestTable_lock);
}

static void
ieee1394_ioctl_test_isoxmt(void *arg)
{
   board_driver_ioctl_test_isoxmt(arg);
   return;
}


/* This thread finds the last msg in the msg queue (written
   when self_id packets arrived), parses the packet, fills
   in self_id information and queries with each device regarding
   it's UID */

void
thread_selfid_recv(void)
{
  int index;
  int *headerp, *datap, *lenp, *cyclecounterp;
  int s;

  MUTEX_LOCK(&selfid_thread_started_lock, -1);
    selfid_thread_started=1; 
    SV_BROADCAST(&selfid_thread_started_sv);
  MUTEX_UNLOCK(&selfid_thread_started_lock);

  while(1) {
      psema(&selfid_thread_sema, PZERO);

      /* Ok, now read the headp, tailp values and find out
         which msg_queue entry you must read */

      MUTEX_LOCK(&selfid_msg_q_lock, -1);

        if (selfid_msg_q.headp==selfid_msg_q.tailp) {
           printf("Error, thread_selfid_recv: \
			selfid_msg_q.headp==selfid_msg_q.tailp\n");
        }
        else {
           index=selfid_msg_q.headp;
	   if (board_driver_get_headerlist_info) {
	   s=board_driver_get_headerlist_info(SIDRCV, 
		selfid_msg_q.entry[index].sid_rcv_tailp, &headerp, &cyclecounterp, 
		&lenp, &datap);
	   }
	   else s=1;

	   if (s) {
		printf("Error getting board_driver headerlist_info\n");
		continue;
	   }
        }

      MUTEX_UNLOCK(&selfid_msg_q_lock);

      ieee1394_ParseSelfIDPacket(headerp, cyclecounterp, lenp, datap);

      if (valusema(&selfid_thread_sema)) continue;

      /* Call the post-processing function in board-driver. */
      if (selfid_msg_q.entry[index].post_funp) {
        selfid_msg_q.entry[index].post_funp(SIDRCV);
      }

      if (tree_db.self_id_invalid!=1) {
            int i, status;

            /* Get the Vendor_ID, Device_ID, UIDs for all devices
                other than myself */
            for (i=0;i<tree_db.n_self_ids; i++) {
              if (i!=tree_db.our_phy_ID) {
                   status=ieee1394_get_UID(i, &tree_db.self_id_tab[i]);
                   if (status!=0) { /*Error occured while getting UID*/
                         tree_db.self_id_tab[i].Vendor_ID=0;
                         tree_db.self_id_tab[i].Device_ID=0;
                         tree_db.self_id_tab[i].UID=0;
                   }
              }
              else {
                    /* I will set my UID to 0 */
                    tree_db.self_id_tab[i].Vendor_ID=0;
                    tree_db.self_id_tab[i].Device_ID=0;
                    tree_db.self_id_tab[i].UID=0;
              }
            }
        }
        else {
            int i;
            for (i=0; i<tree_db.n_self_ids; i++) {
                  tree_db.self_id_tab[i].Vendor_ID=0;
                  tree_db.self_id_tab[i].Device_ID=0;
                  tree_db.self_id_tab[i].UID=0;
            }
        }

        /* Call UpdateInterestTable to update the interest table based
           on the new node_id assignments. This function will also callback
           functions if device/s have ceased to exist on the bus. */

        s=ieee1394_update_InterestTable();
        if (s!=0) {
           printf("ieee1394_update_InterestTable() reported an error.\n");
        }

        MUTEX_LOCK(&selfid_msg_q_lock, -1);
          selfid_msg_q.headp=selfid_msg_q.tailp;
        MUTEX_UNLOCK(&selfid_msg_q_lock);

        MUTEX_LOCK(&bus_reset_lock,-1);
          if ((bus_reset_state==0) && (!valusema(&selfid_thread_sema))) {
            selfid_reset_state=0;
            reset_state=0;
          }
        MUTEX_UNLOCK(&bus_reset_lock);

  } /* End of while(1) */
}


/* This thread finds the *next* msg in the asyrecv_msg_q (written
   by tifwintr when asy recv packets arrived), checks the
   InterestTable to see if any Upper level driver/s is interested
   in the asy recv transaction, and accordingly callsback.
   Here asyrecv_msg_q.headp is the last msg read by the thread.
   Hence we need to first increment the headp count to read the
   next msg. */

void
thread_asyrecv(void)
{
   int index, source_id;
   node_t *nextnodep;
   int i;

   while (1) {
      psema(&asyrecv_thread_sema, PZERO);

      /* Now read the headp, tailp values and find out which
         asyrecv_msg_q entry to read */
      MUTEX_LOCK(&asyrecv_msg_q_lock,-1);
        index=(asyrecv_msg_q.headp+1) % MAX_ASYRECV_TRANSACTIONS_PENDING;
      MUTEX_UNLOCK(&asyrecv_msg_q_lock);

      source_id=(asyrecv_msg_q.entry[index].source_id & 0x3f);

      /* Check out the InterestTable.table[source_id] for interest */
      MUTEX_LOCK(&InterestTable_lock, -1);
        nextnodep=InterestTable.table[source_id].nodep->next;
        for (i=0; i<InterestTable.table[source_id].nodep->CurrentInterest; i++) {
           if ((nextnodep->endaddr==0) && (nextnodep->begaddr!=0) &&
               (asyrecv_msg_q.entry[index].destination_offset==nextnodep->begaddr)) {
                  asyrecv_msg_q.entry[index].app_id=nextnodep->app_id;
                  /* It's important to know that the callback routine must complete
                     before the ring buffer overfills, else ring buffer overflowed 
		     is reported in link_data_indication() and driver is in unusable 
		     state. */
                  (*(nextnodep->fp))((void*)&asyrecv_msg_q.entry[index]);
           }
           else if ((nextnodep->endaddr!=0) && (nextnodep->begaddr!=0) &&
                    (asyrecv_msg_q.entry[index].destination_offset>=nextnodep->begaddr) && 
		    (asyrecv_msg_q.entry[index].destination_offset<=nextnodep->endaddr)) {
                  asyrecv_msg_q.entry[index].app_id=nextnodep->app_id;
                  (*(nextnodep->fp))((void*)&asyrecv_msg_q.entry[index]);
           }
           nextnodep=nextnodep->next;
        }
      MUTEX_UNLOCK(&InterestTable_lock);

      MUTEX_LOCK(&asyrecv_msg_q_lock, -1);
          asyrecv_msg_q.headp=(asyrecv_msg_q.headp+1) % MAX_ASYRECV_TRANSACTIONS_PENDING;
      MUTEX_UNLOCK(&asyrecv_msg_q_lock);

   } /* End of while (1) */
}


static int
ieee1394_wait_for_reset(void)
{
    int t;

    /* Wait until we are out of the reset state */

    MUTEX_LOCK(&bus_reset_lock, -1);
    while(1) {
        if(!reset_state)break;
        t=SV_WAIT_SIG(&bus_reset_sv, &bus_reset_lock,0);
        if(!t) return EINTR;
        MUTEX_LOCK(&bus_reset_lock,-1);
    }
    MUTEX_UNLOCK(&bus_reset_lock);
    return 0;
}

static int
ieee1394_check_for_reset(void)
{
    int t;

    t=0;
    MUTEX_LOCK(&bus_reset_lock,-1);
    if(reset_state)t=1;
    MUTEX_UNLOCK(&bus_reset_lock);
    return t;
}

static void
ieee1394_ParseSelfIDPacket(int *headerp, int *cyclecounterp, int *lenp,
			   int *datap)
{
    int header;
    int xferlen;
    int sct, dseq;
    int *p;
    int firstpart, secondpart, npairs, wordoffset, csum;
    int expected_id, have_our_phy_ID;
    int phy_ID, IRM_phy_ID;
    int l;

    xferlen= (*lenp) & 0x1fff;

    dki_dcache_inval((caddr_t)datap,xferlen-4);

    header=(*headerp);

    /* The self-id packet length should be a multiple of 8.  Unfortunately,
       a chip bug causes extra junk to get tacked onto it, especially when
       we force a bus reset by writing to the PHY chip.  So we do not
       treat a wrong-length self-id packet as an error. */

    if(0!=(xferlen%8))printf("Packet length, %d, not a multiple of 8\n",
                                xferlen);

    /* Iterate through the self-id pairs.

       Each pair describes one physical ID on the bus.  The phys_ID
       fields should come in sequentially, starting at zero, with at
       most one gap.  The missing phys_ID associated with the gap is
       our phys_ID.  If there is no gap, then ours is one plus the
       last phys_ID returned.

       If there is more than one gap, or if the id's are not ascending,
       then we mark the self-id as invalid and return.

       We fill in the self_id_record_t in the self_id_tab for each
       pair, indexed by the phys_ID.  We don't fill in our own
       self_id_record_t yet, since that would require reading the PHY
       chip's registers which can only be done from the top half of a
       driver.

       Normally, the pairs should xor to 0xffffffff, but due to a chip
       bug, sometimes there is extra junk at the end of the self-id
       packet.  So we don't treat these xor failures as an error.
       Instead we treat them as the end of the self-id data in the
       packet.  The first two bits of the first word of the pair should
       be 10, but due to that same bug, sometimes it is not.  So, we
       also don't treat this case as an error, but rather as the end of
       the self-id data in the packet */

    npairs=xferlen/8;
    firstpart=header;
    wordoffset=0;
    expected_id=0;
    have_our_phy_ID=0;
    IRM_phy_ID = -1; /* Setting invalid phy_ID to find the IRM node */

    while(npairs) {

        /* Find the next pair. The secondpart should be an
         * inverse of the firstpart.
         */

        secondpart=datap[wordoffset];
        wordoffset++;

        /* Test that it looks like a self-id packet */

        if((firstpart & 0xc0000000) != 0x80000000) {
            printf("Not a self-id pair 0x%x\n",firstpart);
            break;
        }

        /* Test that the pair xors to 0xffffffff */

        csum=firstpart ^ secondpart;
        if(csum != 0xffffffff) {
            printf("Not a self-id pair 0x%x 0x%x\n",firstpart,secondpart);
            break;
        }

        /* Parse the packet */

        phy_ID=(firstpart>>24)&0x3f;
        tree_db.self_id_tab[phy_ID].link_active=(firstpart>>22)&1;
        tree_db.self_id_tab[phy_ID].gap_cnt=(firstpart>>16)&0x3f;
        tree_db.self_id_tab[phy_ID].sp=(firstpart>>14)&3;
        tree_db.self_id_tab[phy_ID].del=(firstpart>>12)&3;
        tree_db.self_id_tab[phy_ID].c=(firstpart>>11)&1;
        tree_db.self_id_tab[phy_ID].pwr=(firstpart>>8)&7;
        tree_db.self_id_tab[phy_ID].port_status[0]=(firstpart>>6)&3;
        tree_db.self_id_tab[phy_ID].port_status[1]=(firstpart>>4)&3;
        tree_db.self_id_tab[phy_ID].port_status[2]=(firstpart>>2)&3;
        tree_db.self_id_tab[phy_ID].i=(firstpart>>1)&1;
        tree_db.self_id_tab[phy_ID].m=firstpart&1;

        /* Find out the IRM phy_ID */
        if ((tree_db.self_id_tab[phy_ID].link_active) &&
            (tree_db.self_id_tab[phy_ID].c) &&
            (IRM_phy_ID < phy_ID))
        {
            IRM_phy_ID = phy_ID;
            printf("Potential IRM: %d.\n", IRM_phy_ID);
        }

        bzero(&tree_db.self_id_tab[phy_ID].port_status[3],27-3);
        for(l=0;l<27;l++)tree_db.self_id_tab[phy_ID].port_connection[l]= -1;

        /* Verify that the phy_ID's are in ascending order with at most
           one gap.  The gap is our phy_ID, so make a not of it.  If
           they are out of order or have more than one gap, then mark
           the self_id sequence invalid and return */

        if(phy_ID != expected_id) { /* if not in order */
            if(phy_ID == expected_id+1) { /* if it's a single gap */
                if(!have_our_phy_ID) { /* if we haven't found a gap yet */
                    have_our_phy_ID=1;
                    tree_db.our_phy_ID=expected_id;
                } else { /* found more than one gap */
                    tree_db.self_id_invalid=1;
                    return;
                }
            } else { /* not a single gap */
                tree_db.self_id_invalid=1;
                return;
            }
        }

        expected_id=phy_ID+1;
        npairs--;
        firstpart=datap[wordoffset];
        wordoffset++;
    }

    if(have_our_phy_ID == 0) {
        if(expected_id >= 63) {
            tree_db.self_id_invalid=1;
            return;
        } else {
            tree_db.our_phy_ID=expected_id;
            expected_id++;
            have_our_phy_ID=1;
        }
    }

    /* It is possible that we are the root (with the highest
       phy_ID) and hence if we are ISO capable, we should be
       the IRM: So check that. */

    {
        int phy_ID, gap_cnt, sp, c, l, s;
        int i, status;
        signed char port_status[27];

        board_driver_ReadOurSelfIDInfo(&phy_ID, &gap_cnt, &sp, &c, port_status);
        printf("phy_ID=%d, gap_cnt=%d, sp=%d, c=%d\n", phy_ID, gap_cnt,
                        sp, c);

        /* We know that our link_active=1 */
        if ((c) && (phy_ID>IRM_phy_ID)) {
             IRM_phy_ID = phy_ID;
        }

        /* Now, add our self_id info in the tree_db */

        tree_db.self_id_tab[phy_ID].link_active=1;
        tree_db.self_id_tab[phy_ID].gap_cnt=gap_cnt;
        tree_db.self_id_tab[phy_ID].sp=sp;
        tree_db.self_id_tab[phy_ID].del=0;
        tree_db.self_id_tab[phy_ID].c=c;
        tree_db.self_id_tab[phy_ID].pwr=0;
        for(l=0;l<27;l++) tree_db.self_id_tab[phy_ID].port_status[l]=port_status[l];
        for(l=0;l<27;l++) tree_db.self_id_tab[phy_ID].port_connection[l]= -1;
        tree_db.self_id_tab[phy_ID].i=0;
        tree_db.self_id_tab[phy_ID].m=0;

        /* Build the complete tree (with port_connections) */
        status=ieee1394_BuildSelfIDTree(&tree_db);
        if (status!=0) {
            tree_db.self_id_invalid = 1;
            return;
        }
    }

    printf("Our SelfID: %d\n", tree_db.our_phy_ID);
    printf("IRM phy_ID: %d\n", IRM_phy_ID);

    tree_db.n_self_ids=expected_id;
    tree_db.self_id_invalid=0;
    tree_db.IRM_phy_ID = IRM_phy_ID;
    return;
}

static int
ieee1394_BuildSelfIDTree(self_id_db_t *tree_db)
{
    int l, m, n, parent_count, child_count, stackp;
    signed char stack[64];
    signed char unconnected_child_count[64], unconnected_parent_count[64];


    /* Count the number of parent and child connections for each phy_ID */

    for(n=0;n<tree_db->n_self_ids;n++) {
        parent_count=0;
        child_count=0;
        for(l=0;l<27;l++) {
            if(tree_db->self_id_tab[n].port_status[l]==CONNECTED_TO_CHILD)
                child_count++;
            else if(tree_db->self_id_tab[n].port_status[l]==CONNECTED_TO_PARENT)
                parent_count++;
            tree_db->self_id_tab[n].port_connection[l] = -1;
        }
        unconnected_parent_count[n]=parent_count;
        unconnected_child_count[n]=child_count;
    }

    stackp=0;
    for(n=0;n<tree_db->n_self_ids;n++) {
        while(unconnected_child_count[n]!=0) {
            stackp--;
            if(stackp<0) {
                printf("Stack underflow\n");
                return -1;
            }
            m=stack[stackp];
            /* link n (parent) to m (child) */
            for(l=0;l<27;l++) {
                if(tree_db->self_id_tab[n].port_status[l]==CONNECTED_TO_CHILD &&
                        tree_db->self_id_tab[n].port_connection[l] == -1) break;
            }
            if(l==27) {
                printf("fail can\'t link parent to child\n");
                return -1;
            }
            tree_db->self_id_tab[n].port_connection[l]=m;
            unconnected_child_count[n] -= 1;
            /* link m to n; */
            for(l=0;l<27;l++) {
                if(tree_db->self_id_tab[m].port_status[l]==CONNECTED_TO_PARENT &&
                  tree_db->self_id_tab[m].port_connection[l] == -1)break;
            }
            if(l==27) {
                printf("fail can\'t link child to parent\n");
                return -1;
            }
            tree_db->self_id_tab[m].port_connection[l]=n;
            unconnected_parent_count[m] -= 1;
        }
        stack[stackp]=n;
        stackp++;
    }

    if(stackp != 1) {
        printf("wrong number of items left on stack\n");
        return -1;
    }
    if(stack[0] != tree_db->n_self_ids-1) {
        printf("root node not left on stack\n");
        return 1;
    }
    return 0;
}

static int
ieee1394_get_UID(int phy_ID, self_id_record_t *selfid_record)
{
  int status;

  selfid_record->Vendor_ID=0;
  selfid_record->Device_ID=0;
  selfid_record->UID=0;
  status=ieee1394_quadread(phy_ID, 0xfffff000040c);
  if (status==0) {
    selfid_record->Vendor_ID=(UIDword>>8);
    selfid_record->Device_ID=(UIDword & 0xff); /* chip_id_hi (8 bits) */
    selfid_record->Device_ID=selfid_record->Device_ID<<32;
    selfid_record->UID=UIDword;
    selfid_record->UID=selfid_record->UID<<32;
  }
  else return(-1);

  status=ieee1394_quadread(phy_ID, 0xfffff0000410);
  if (status==0) {
     selfid_record->Device_ID |= UIDword; /* chip_id_lo (32 bits) */
     selfid_record->UID |= UIDword;
  }
  else return(-1);

  printf("Vendor_ID: 0x%llx\n", selfid_record->Vendor_ID);
  printf("Device_ID: 0x%llx\n", selfid_record->Device_ID);
  printf("UID:       0x%llx\n", selfid_record->UID);
  return(0);
}

static int
ieee1394_quadread(int phy_ID, long long offset)
{
   printf("offset: 0x%x\n", offset);
   ieee1394_quadread_transaction.destination_id=0xffc0|phy_ID;
   ieee1394_quadread_transaction.tcode=IEEE1394_READ_REQUEST_QUAD;
   ieee1394_quadread_transaction.tlabel=(tlabel++)%64;
   ieee1394_quadread_transaction.priority=0;
   ieee1394_quadread_transaction.source_id=0xffc0 | tree_db.our_phy_ID;
   ieee1394_quadread_transaction.destination_offset=offset;
   ieee1394_quadread_transaction.data_length=4;
   ieee1394_quadread_transaction.extended_tcode=0;
   ieee1394_quadread_transaction.block_data_p = &UIDword;
   ieee1394_quadread_transaction.busy=1;
   ieee1394_quadread_transaction.state=TQ_AWAITING_XMIT;
   ieee1394_quadread_transaction.stat=TQ_STATUS_AWAITING;
   ieee1394_add_transaction(&ieee1394_quadread_transaction);
   MUTEX_LOCK(&tq_lock, -1);
   while(1) {
        if(!ieee1394_quadread_transaction.busy)break;
        SV_WAIT(&tq_sv,&tq_lock,0);
        MUTEX_LOCK(&tq_lock, -1);
   }
   MUTEX_UNLOCK(&tq_lock);
   ieee1394_remove_transaction(&ieee1394_quadread_transaction);
   printf("quadread status: %d\n", ieee1394_quadread_transaction.stat);
   return (ieee1394_quadread_transaction.stat);
}

/*=================== Transaction Layer ==========================*/

static void
ieee1394_add_transaction(ieee1394_transaction_t *t)
{
    int need_to_activate_tq_rotor;

    /* Lock the transaction list */

    MUTEX_LOCK(&tq_lock,-1);

    /* Link the transaction after the transaction pointed to by tq_tail */

    if(tq_tail == 0) {
        tq_head=t;
        t->next=0;
        t->prev=0;
    } else {
        t->prev=tq_tail;
        t->next=tq_tail->next;
        tq_tail->next=t;
    }
    tq_tail=t;

    /* We need to activate the tq_rotor if it's not already active
       so that this transaction will eventually be processed.  Here
       we determine if the tq_rotor is already active or not. */

    need_to_activate_tq_rotor=0;
    if(!tq_rotor_active) {
        need_to_activate_tq_rotor=1;
    }

    /* Unlock the transaction queue */

    MUTEX_UNLOCK(&tq_lock);

    /* Activate the tq_rotor if necessary */

    if(need_to_activate_tq_rotor){
        tq_rescan = 1;
        ieee1394_activate_tq_rotor();
    }
}

static void
ieee1394_remove_transaction(ieee1394_transaction_t *t)
{
    int need_to_activate_tq_rotor;

    /* Lock the transaction list */

    MUTEX_LOCK(&tq_lock,-1);

    /* If we are removing the tail transaction of the list, adjust
       tq_tail to point to its predecessor */

    if(t==tq_tail){
        tq_tail=t->prev;
    }

    /* If we are removing the transaction pointed to by tq_rotor, adjust
       tq_rotor to point to its successor */

    if(t==tq_rotor) {
        tq_rotor=t->next;
    }

    /* Unlink the transaction */

    if(t->prev) {
        (t->prev)->next=t->next;
    } else {
        tq_head=t->next;
    }
    if(t->next) {
        (t->next)->prev=t->prev;
    }
    t->next=0;
    t->prev=0;

    /* Unlock the transaction queue */

    MUTEX_UNLOCK(&tq_lock);
}

static void
ieee1394_activate_tq_rotor()
{
    int need_to_do_tq_rotor;

    /* Lock the transaction list */

    MUTEX_LOCK(&tq_lock,-1);

    need_to_do_tq_rotor=0;
    if(!tq_rotor_active) {
        tq_rotor_active=1;
        need_to_do_tq_rotor=1;
    }

    /* Unlock the transaction queue */

    MUTEX_UNLOCK(&tq_lock);

    if(need_to_do_tq_rotor) {
        ieee1394_do_tq_rotor();
    }
}

static void
ieee1394_do_tq_rotor(void)
{
    int s;
    int tx_busy;

    while (1) {
        /* Lock the transaction list */

        MUTEX_LOCK(&tq_lock, -1);

        /* Handle tq_rotor at end of the list */

        if (tq_rotor == 0) {
            tq_rotor = tq_head;
            if (tq_rotor == 0) {
                tq_rescan = 0;
                tq_rotor_active = 0;
            } else {
                if (tq_rescan == 0) {
                    tq_rotor_active = 0;
                } else {
                    tq_rescan=0;
                }
            }
        }

        if (!tq_rotor_active) {
            /* Unlock the transaction queue */
            MUTEX_UNLOCK(&tq_lock);
            return;
        }

        tx_busy = ieee1394_service_a_transaction(tq_rotor);
        if (tx_busy) {
            tq_rotor_active=0;
            MUTEX_UNLOCK(&tq_lock);
            return;
        }

        tq_rotor = tq_rotor->next;

        /* Unlock the transaction queue */

        MUTEX_UNLOCK(&tq_lock);
    }
}

static int
ieee1394_service_a_transaction(ieee1394_transaction_t *t)
{
    int ret;

    ret=0;
    if(t->state == TQ_AWAITING_XMIT) {
        if(tx_state == TX0) {
    	    tq_transaction_p=t;
            board_driver_link_request(t);
            t->state=TQ_AWAITING_ACK;
        } else {
            ret=1;
        }
    }
    return ret;
}

static void
ieee1394_print_transaction(ieee1394_transaction_t *t)
{
  if (t) {
    printf("t: 0x%x\n", t);
    printf("tq_head: 0x%x\n", tq_head);
    printf("tq_tail: 0x%x\n", tq_tail);
    printf("destination_id     0x%x\n",t->destination_id);
    printf("tlabel             0x%x\n",t->tlabel);
    printf("tcode              0x%x\n",t->tcode);
    printf("priority           0x%x\n",t->priority);
    printf("source_id          0x%x\n",t->source_id);
    printf("destination_offset 0x%lx\n",t->destination_offset);
    printf("data_length        0x%x\n",t->data_length);
    printf("extended_tcode     0x%x\n",t->extended_tcode);
    printf("block_data_p       0x%x\n",t->block_data_p);
    printf("next               0x%x\n",t->next);
    printf("prev               0x%x\n",t->prev);
    printf("stat               0x%x\n",t->stat);
    printf("busy               0x%x\n",t->busy);
    printf("state              0x%x\n",t->state);
    printf("ntimeout           0x%x\n",t->ntimeout);
  }
  else {
    printf("TIFW_PRINT_TRANSACTION: Got t NULL.\n");
  }
}

