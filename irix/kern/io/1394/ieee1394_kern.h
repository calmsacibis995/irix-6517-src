/* This file is the ieee1394 middle-level driver file that supports
   any chipset through a lower-level driver. */

#define  _IEEE1394_KERN

#include "sys/types.h"
#include "sys/ksynch.h"
#include "sys/ddi.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/edt.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/mman.h"
#include "ksys/ddmap.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/mbuf.h"
#include "sys/sbd.h"
#include "sys/socket.h"
#include "sys/cpu.h"
#include "sys/conf.h"
#include "sys/ddi.h"
#include "sys/mload.h"
#include <sys/cred.h>
#ifndef BONSAI
#include "sys/pci_intf.h"
#endif
#include "sys/PCI/pciio.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/ktime.h"
#include "sys/sema.h"
#include "ksys/sthread.h"
#include "ieee1394.h"

#define DEBUG_ZERO_MAPPAGES

/* OPERATIONS (to get headerlist info & interrupt callback) */
#define SIDRCV 0
#define ISORCV 1
#define ISOXMT 2
#define ASYRCV 3
#define ASYXMT 4
#define ACKRCV 5
#define BUSRESET 6

#define ISORCV_NUMDATAPAGES    500
#define ISORCV_NUMDATAPACKETS   (ISORCV_NUMDATAPAGES*2)
#define ISOXMT_NUMDATAPAGES    50
#define ISOXMT_NUMDATAPACKETS   (ISOXMT_NUMDATAPAGES*2)
#define SIDRCV_NUMDATAPAGES    5
#define SIDRCV_NUMDATAPACKETS   (SIDRCV_NUMDATAPAGES*2)
#define ASYRCV_NUMDATAPAGES    5
#define ASYRCV_NUMDATAPACKETS   (ASYRCV_NUMDATAPAGES*2)

#define MAX_ASYRECV_TRANSACTIONS_PENDING 16

/* Daemon kthread stack size - 4k on 32 bit systems, 8k on 64bit systems
   From: os/main.c  */
#define DKTSTKSZ        (1024 * sizeof(void *))

/* Transaction states */
#define TQ_AWAITING_XMIT 1
#define TQ_AWAITING_ACK  2
#define TQ_AWAITING_CONF 3
#define TQ_FINISHED      4

/* Transmitter states */
#define TX0 0
#define TX1 1
#define TX2 2
#define TX3 3

/* DMA Channel Assignments */
#define FW_ISORCV_DMA_CHAN      0
#define FW_ISOXMT_DMA_CHAN      1
#define FW_SIDRCV_DMA_CHAN      2
#define FW_ASYRCV_DMA_CHAN      3
#define FW_ASYXMT_DMA_CHAN      4

#define FW_ISORCV_DMA_PCL_INT DMA0_PCL
#define FW_ISOXMT_DMA_PCL_INT DMA1_PCL
#define FW_SIDRCV_DMA_PCL_INT DMA2_PCL
#define FW_ASYRCV_DMA_PCL_INT DMA3_PCL
#define FW_ASYXMT_DMA_PCL_INT DMA4_PCL

#define FW_ISORCV_DMA_HLT_INT DMA0_HLT
#define FW_ISOXMT_DMA_HLT_INT DMA1_HLT
#define FW_SIDRCV_DMA_HLT_INT DMA2_HLT
#define FW_ASYRCV_DMA_HLT_INT DMA3_HLT
#define FW_ASYXMT_DMA_HLT_INT DMA4_HLT

#define MAX_INTEREST_PER_DEVICE 16
#define MAX_ASYRECV_TRANSACTIONS_PENDING 16

/* XXX: Not sure if you need these Bits in asy_rw_state */
#define ASY_RW_NEED_WRITE_COMPLETE 1
#define ASY_RW_NEED_READ_RESPONSE  2SY_RW_NEED_READ_RESPONSE
#define ASY_RW_IN_USE              4


/* This structure contains the self id data for one phy_ID */
typedef struct self_id_record_s {
    int link_active;
    int gap_cnt;
    int sp;
    int del;
    int c;
    int pwr;
    signed char port_status[27];
    int i;
    int m;
    signed char port_connection[27];
    long long Vendor_ID;
    long long Device_ID;
    long long UID;
}self_id_record_t;


/* This structure contains the self id data for an entire device tree */
typedef struct self_id_db_s {
    long long           bus_reset_sequence_number;
    int                 n_self_ids;
    int                 self_id_invalid;
    int                 our_phy_ID;
    int                 IRM_phy_ID; /* The current IRM phy_ID */
    self_id_record_t    self_id_tab[64];
} self_id_db_t;

/* Representation of a transaction */
typedef struct ieee1394_transaction_s {
    long long destination_offset;
    int       destination_id;
    int       source_id;
    int       *block_data_p;
    struct    ieee1394_transaction_s *next;
    struct    ieee1394_transaction_s *prev;
    int       stat;
    int       busy;
    int       state;
    int       data_length;
    int       tlabel;
    int       tcode;
    int       priority;
    int       extended_tcode;
    int       ntimeout;
} ieee1394_transaction_t;

typedef struct msg_queue_entry_s {
  long long bus_reset_sequence_no;
  void (*post_funp)(int op);
  int sid_rcv_tailp;
} msg_queue_entry_t;

typedef struct selfid_msg_queue_s {
  int headp;
  int tailp;
  msg_queue_entry_t entry[2];
} selfid_msg_queue_t;

/* Callback function structure (UpdateInterestTable(), thread_asyrecv() */
typedef struct callback_s {
    int callback_code; /* -1 for DEVICE_DISAPPEARED, tcode otherwise */
    int app_id;
    int source_id;
    int destination_id;
    long long destination_offset;
    int tlabel;
    int data_length;  /* valid only for block payload transactions */
    int extended_tcode;
    int payload[128]; /* valid only for block payload transactions */
} callback_t;

typedef struct asyrecv_msg_q_s {
    int headp;
    int tailp;
    callback_t entry[MAX_ASYRECV_TRANSACTIONS_PENDING];
} asyrecv_msg_q_t;

/* Info stored for each vertex attached */
typedef struct info_s {
    vertex_hdl_t parent_vertex;
} info_t;

/*****************************************************
	 InterestTable definitions 
*****************************************************/
#define FATAL_ERROR -1
#define INVALID_UID -2
#define INVALID_FP -3
#define INVALID_INTEREST_PARAMS -4
#define MAX_INTEREST_EXCEEDED -5
#define SUCCESS 0

typedef int (*fun_t)(void*); /* fun_t is typed as function pointer */

typedef struct node_s {
  int app_id; /* A unique id for each upper level driver app */
  long long begaddr;
  long long endaddr;
  fun_t fp;
  struct node_s *next;
} node_t;

typedef struct UID_node_s {
  long long Vendor_ID;
  long long Device_ID;
  long long UID;
  int CurrentInterest;
  node_t *next;
} UID_node_t;

typedef struct InterestTableEntry_s {
  int nodevalid_flag;
  int UIDvalid_flag;
  UID_node_t *nodep;
  UID_node_t *prevnodep;
} InterestTableEntry_t;

typedef struct InterestTable_s {
  long long seq_no; /* same as tree_db.bus_reset_sequence_number after
                       update_table() is called. */
  int nentries;     /* no of valid nodes in the table */
  InterestTableEntry_t table[64];
} InterestTable_t;

/**********************************************************
         Shared variables with lower-level driver
**********************************************************/

/* Required to dynamically call the correct lower-level driver  */
int (*board_driver_readreg)(int offset);
void (*board_driver_writereg)(int offset, int val);
int (*board_driver_get_mmapsize)(void);
void (*board_driver_link_request)(ieee1394_transaction_t *t);
int (*board_driver_get_headerlist_info)(int op, int index, int **hdr, 
                int **cyclecounter, int **len, int **datap);
int (*board_driver_get_isorcv_info)(int *tailp);
int (*board_driver_get_isoxmt_info)(int *headp);
void (*board_driver_ReadOurSelfIDInfo)(int *PhyID, int *gap_cnt,
		int *sp, int *c, signed char port_status[27]);

/* XXX: Only experimental. Please delete later. */
void (*board_driver_ioctl_test_isoxmt)(void *arg);

int shared_init_called=0;
mutex_t shared_init_lock;
int     reset_state;
int     bus_reset_state;
mutex_t bus_reset_lock;
sv_t    bus_reset_sv;
self_id_db_t tree_db;
mutex_t selfid_thread_started_lock;
sv_t    selfid_thread_started_sv;
int     selfid_thread_started;

/*******************************************************
	ieee1394 global definitions 
*******************************************************/
int ieee1394_devflag = D_MP;
char *ieee1394_mversion = M_VERSION;

/* Required for asychronous transactions */
int tlabel;
int selfid_reset_state;

/* Required by the transaction layer*/
sv_t tq_sv;
mutex_t tq_lock;

sema_t selfid_thread_sema;
selfid_msg_queue_t selfid_msg_q;
mutex_t selfid_msg_q_lock;

sema_t asyrecv_thread_sema;
asyrecv_msg_q_t asyrecv_msg_q;
mutex_t asyrecv_msg_q_lock;

ieee1394_transaction_t ieee1394_ioctl_quadread_transaction;
int ieee1394_ioctl_quadread_word;
ieee1394_transaction_t ieee1394_ioctl_quadwrite_transaction;
int ieee1394_ioctl_quadwrite_word;
ieee1394_transaction_t ieee1394_ioctl_quadlock_transaction;
int ieee1394_ioctl_quadlock_word[2];
ieee1394_transaction_t ieee1394_ioctl_blockread_transaction;
/*XXX: There will probably be a blockread_words here */
ieee1394_transaction_t ieee1394_ioctl_blockwrite_transaction;
/* ieee1394_get_UID() */
int UIDword; 
/* ieee1394_quadread() */
ieee1394_transaction_t ieee1394_quadread_transaction;

/* Pointer to ti_lynx_state_ptr->sidrcv_HeadP passed as param1 in
   ieee1394_interrupt() 
   int *sidrcv_HeadPP; */

/* Global variables required for InterestTable */
InterestTable_t InterestTable;
mutex_t InterestTable_lock;

/**********************************************************
	 Transaction Layer 
**********************************************************/
int tx_state=0;
int tq_rotor_active=0;
int tq_rescan=0;
ieee1394_transaction_t *tq_head;
ieee1394_transaction_t *tq_tail;
ieee1394_transaction_t *tq_rotor;
ieee1394_transaction_t *tq_transaction_p;

char *normal_acks[16]= {
        "Reserved",
        "ack_complete",
        "ack_pending",
        "Reserved",
        "ack_busy_X",
        "ack_busy_A",
        "ack_busy_B",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "ack_data_error",
        "ack_type_error",
        "Reserved"
};

char *special_acks[16]= {
        "Link reported a Retry Overrun",
        "Link reported a Timeout",
        "Link reported a FIFO underrun",
        "Reserved",
        "Reserved",
        "No expected End of receive Packet",
        "Piplined Async Transmit Command encountered a command other than another Async Transmit",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Link reported a corrupted header before the packet was transmitted",
        "Reserved"
};

