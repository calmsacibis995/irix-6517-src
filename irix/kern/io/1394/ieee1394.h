#define _IEEE1394H

/* Ioctl commands: Application => ieee1394.c */

#define FW_READREG 1
#define FW_WRITEREG 2
#define FW_GET_MMAPSIZE 3
#define FW_INVAL 4
#define FW_QUADREAD 6
#define FW_QUADWRITE 7
#define FW_GET_TREE 9
#define FW_BLOCKREAD 10
#define FW_BLOCKWRITE 11
#define FW_QUADLOCK 12
#define FW_PRINTINTERESTTABLE 13
#define FW_TEST_ISOXMT 14

/* TCODES */
#define IEEE1394_WRITE_REQUEST_QUAD         0
#define IEEE1394_WRITE_REQUEST_BLOCK        1
#define IEEE1394_WRITE_RESPONSE             2
#define IEEE1394_READ_REQUEST_QUAD          4
#define IEEE1394_READ_REQUEST_BLOCK         5
#define IEEE1394_READ_RESPONSE_QUAD         6
#define IEEE1394_READ_RESPONSE_BLOCK        7
#define IEEE!394_CYCLE_START                8
#define IEEE1394_LOCK_REQUEST               9
#define IEEE1394_ISOCHRONOUS_BLOCK          0xa
#define IEEE1394_LOCK_RESPONSE              0xb

/* Transaction status' */
#define TQ_STATUS_SUCCESS 0
#define TQ_STATUS_AWAITING -1
#define TQ_STATUS_RESERVED -2
#define TQ_STATUS_FAILED_CRC_OR_DATA_LENGTH_ERROR -3
#define TQ_STATUS_INVALID_TRANSACTION_ERROR -4
#define TQ_STATUS_SPECIAL_RESERVED -5
#define TQ_STATUS_CONFLICT_ERROR -6
#define TQ_STATUS_DATA_UNAVAILABLE_ERROR -7
#define TQ_STATUS_INVALID_ADDRESS_ERROR -8


/* XXX: Not sure if this should be in this file. 
   Tree ID stuff */

#define NOT_PRESENT		0
#define CONNECTED_TO_NOTHING	1
#define CONNECTED_TO_PARENT	2
#define CONNECTED_TO_CHILD	3

typedef struct ioctl_quadwrite_s {
    long long	seqnum;
    long long	destination_offset;
    int		destination_id;
    int		quadlet_data;
    int 	status;
} ioctl_quadwrite_t;

typedef struct ioctl_quadlock_s {
    long long   seqnum;
    long long   destination_offset;
    int         destination_id;
    int         quadlet_arg_value;
    int         quadlet_data_value;
    int         status;
    long long   old_value;
} ioctl_quadlock_t;

typedef struct ioctl_quadread_s {
    long long	seqnum;
    long long	destination_offset;
    int		destination_id;
    int		quadlet_data;
    int 	status;
} ioctl_quadread_t;

typedef struct ioctl_blockread_s {
    short     destination_id;
    char      tcode;
    char      priority;
    long long destination_offset;
    short     data_length;
    char      extended_tcode;
    int       payload[128];
}ioctl_blockread_t;

typedef struct ioctl_blockwrite_s {
    short     destination_id;
    char      tcode;
    char      priority;
    long long destination_offset;
    short     data_length;
    char      extended_tcode;
    int       payload[128];
}ioctl_blockwrite_t;

/************************************************************/
/* XXX: Please delete the self_id_* structs. They are
        defined in ieee1394_kern.h and have been duplicated
        here temporarily */

#ifndef _IEEE1394_KERN
#ifndef _TI_LYNX

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

#endif
#endif
/***********************************************************************/
