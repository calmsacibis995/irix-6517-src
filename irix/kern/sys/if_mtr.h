/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE USER DEFINITIONS                                                */
/*      ====================                                                */
/*                                                                          */
/*      FTK_USER.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains ALL the definitions and structures required by */
/* any  user  of the FTK driver. Any user of the FTK need only include this */
/* definitions header file in order to use the FTK.                         */
/*                                                                          */
/* IMPORTANT : Some structures used within the FTK  need to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_USER.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_USER_H 223


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_NODE_ADDRESS              NODE_ADDRESS;
typedef union  UNION_MULTI_ADDRESS              MULTI_ADDRESS;
typedef struct STRUCT_STATUS_INFORMATION        STATUS_INFORMATION;
typedef struct STRUCT_ERROR_LOG                 ERROR_LOG;
typedef struct STRUCT_PROBE                     PROBE;
typedef struct STRUCT_PREPARE_ARGS              PREPARE_ARGS, *PPREPARE_ARGS;
typedef struct STRUCT_START_ARGS                START_ARGS, *PSTART_ARGS;
typedef struct STRUCT_TR_OPEN_DATA              TR_OPEN_DATA, *PTR_OPEN_DATA;


/****************************************************************************/
/*                                                                          */
/* Function declarations                                                    */
/*                                                                          */
/* Routines  in the FTK are either local to a module, or they are exported. */
/* Exported routines are entry points to the  user  of  a  module  and  the */
/* routine  has  an  'extern' definition in an appropriate header file (see */
/* FTK_INTR.H and FTK_EXTR.H). A user of the FTK may wish  to  follow  this */
/* method of function declarations using the following definitions.         */
/*                                                                          */

#define local   static
#define export


/****************************************************************************/
/*                                                                          */
/* Basic types : BYTE, WORD, DWORD and BOOLEAN                              */
/*                                                                          */
/* The basic types used throughout the FTK, and for passing  parameters  to */
/* it,  are  BYTE  (8  bit unsigned), WORD (16 bit unsigned), DWORD (32 bit */
/* unsigned) and BOOLEAN (16 bit unsigned). A BOOLEAN variable should  take */
/* the value TRUE or FALSE.                                                 */
/*                                                                          */
/* Note  that  none  of  the FTK code makes an explicit check for the value */
/* TRUE (it only checks for FALSE which must be zero) and  hence  TRUE  can */
/* have any non-zero value.                                                 */
/*                                                                          */

typedef unsigned char           BYTE;           /* 8 bits                   */

typedef unsigned short 		WORD;           /* 16 bits                  */

typedef unsigned int       	DWORD;          /* 32 bits                  */

typedef unsigned int       	ULONG;

typedef WORD                    WBOOLEAN;

typedef unsigned int            UINT;

#define	VOID			void

#define FALSE                   0
#define TRUE                    1

#if !defined(max)
#define max(a,b) ((a) < (b) ? (b) : (a))
#endif

#if !defined(min)
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif


#ifdef FMPLUS

/****************************************************************************/
/*                                                                          */
/* Variables : Fmplus download image                                       */
/*                                                                          */
/* The following variables are exported  by  FMPLUS.C  which  contains  the */
/* binary image for FastmacPlus in a 'C' format BYTE array. These variables */
/* will  be  needed  by  a  user  of  the  FTK in order to download Fastmac */
/* Plus  (fmplus_image),   display  the  Fastmac  Plus version  number  and */
/* copyright  message  (fmplus_version  and fmplus_copyright_msg) and check */
/* that  the   FTK   version   number   is   that   required   by   Fastmac */
/* (ftk_version_for_fmplus).  The variables  concerned with the size of the */
/* Fastmac Plus binary (sizeof_fmplus_array and recorded_size_fmplus_array) */
/* can  be  used  to  check  for corruption of the Fasmtac image array. The */
/* checksum byte (fmplus_checksum) can also be used for this purpose.       */
/*                                                                          */

extern char fmplus_version[];

extern char fmplus_copyright_msg[];

extern WORD ftk_version_for_fmplus;

extern WORD sizeof_fmplus_array;

extern WORD recorded_size_fmplus_array;

extern BYTE fmplus_checksum;

#else

/****************************************************************************/
/*                                                                          */
/* Variables : Fastmac download image                                       */
/*                                                                          */
/* The following variables are exported by  FASTMAC.C  which  contains  the */
/* binary image for Fastmac in a 'C' format  BYTE  array.  These  variables */
/* will  be  needed  by  a  user  of  the  FTK in order to download Fastmac */
/* (fastmac_image),  display  the  Fastmac  version  number  and  copyright */
/* message  (fastmac_version  and fastmac_copyright_msg) and check that the */
/* FTK    version    number     is     that     required     by     Fastmac */
/* (ftk_version_for_fastmac).  The variables concerned with the size of the */
/* Fastmac binary  (sizeof_fastmac_array  and  recorded_size_fastmac_array) */
/* can  be  used  to  check  for corruption of the Fasmtac image array. The */
/* checksum byte (fastmac_checksum) can also be used for this purpose.      */
/*                                                                          */

extern WORD fastmac_version;

extern char fastmac_copyright_msg[];

extern WORD ftk_version_for_fastmac;

extern WORD sizeof_fastmac_array;

extern WORD recorded_size_fastmac_array;

extern BYTE fastmac_checksum;

#endif

/****************************************************************************/
/*                                                                          */
/* Values : Pointers                                                        */
/*                                                                          */
/* For a near pointer, (one that points to a location in DGROUP), the value */
/* NULL  (must equal 0) is used to specify that it is yet to be assigned or */
/* an attempt to assign to it was unsuccessful.  For example, an attempt to */
/* allocate memory via a system specific call to which a near pointer is to */
/* point, eg. sys_alloc_init_block, should  return  NULL  if  unsuccessful. */
/* Similarly,  when  a  DWORD  is  used  as  a pointer to a 32 bit physical */
/* address pointer, the value NULL_PHYSADDR (must equal 0L)  is  used.   It */
/* should be returned by sys_alloc fastmac buffer routines if unsuccessful. */
/*                                                                          */

#if !defined(NULL)
#define NULL                    0
#endif

#define NULL_PHYSADDR           0L


/****************************************************************************/
/*                                                                          */
/* Type : ADAPTER_HANDLE                                                    */
/*                                                                          */
/* An element of this type is returned by driver_prepare_adapter  in  order */
/* to  identify a particular adapter for all subsequent calls to the driver */
/* module of the FTK.                                                       */
/*                                                                          */

typedef WORD                    ADAPTER_HANDLE;


/****************************************************************************/
/*                                                                          */
/* Type : DOWNLOAD_IMAGE                                                    */
/*                                                                          */
/* A pointer  to  a  download  image  must  be  supplied  by  the  user  to */
/* driver_prepare_adapter. This download image should be Fastmac.           */
/*                                                                          */

typedef BYTE                    DOWNLOAD_IMAGE;


/****************************************************************************/
/*                                                                          */
/* The following structures represent data strcutures on the adapter and    */
/* must be byte packed.                                                     */
/*                                                                          */

#ifdef FTK_PACKING_ON
#pragma FTK_PACKING_ON
#else
#pragma pack(1)
#endif


/****************************************************************************/
/*                                                                          */
/* Structure type : NODE_ADDRESS                                            */
/*                                                                          */
/* A node address may be supplied  by the user to driver_prepare_adapter or */
/* driver_open_adapter.  The  permanent  node  address  of  the  adapter is */
/* returned by driver_start_adapter. A node address is a 6 byte value.  For */
/* Madge adapters the bytes would be 0x00, 0x00, 0xF6, ... etc.             */
/*                                                                          */

struct STRUCT_NODE_ADDRESS
    {
    BYTE        byte[6];
    };


/****************************************************************************/
/*                                                                          */
/* Union type : MULTI_ADDRESS                                               */
/*                                                                          */
/* A   multicast   address   may   be   supplied    by    the    user    to */
/* driver_set_group_address    or    driver_set_functional_address.     The */
/* multicast address is the final 4 bytes of a 6  byte  node  address.  The */
/* first  2  bytes  are  determined  by  whether it is a group address or a */
/* functional address.                                                      */
/*                                                                          */

union UNION_MULTI_ADDRESS
    {
    DWORD       all;
    BYTE        byte[4];
    };


/****************************************************************************/
/*                                                                          */
/* Type : LONG_ADDRESS                                                      */
/*                                                                          */
/* A LONG_ADDRESS is a 64 bit address. Some architectures (e.g. Alpha) use  */
/* 64 bit physical addresses.                                               */
/*                                                                          */

union STRUCT_LONG_ADDRESS
    {
    BYTE  bytes[8];
    WORD  words[4];
    DWORD dwords[2];
    };

typedef union STRUCT_LONG_ADDRESS LONG_ADDRESS;


/****************************************************************************/
/*                                                                          */
/* Structure type : TR_OPEN_DATA                                            */
/*                                                                          */
/* The TR_OPEN_DATA structure is used to pass  to  the  Open SRB and to the */
/* driver_start_adapter  functions  all  the  addressing details that could */
/* usefully set. This  is  especially  useful  for  restoring the card to a */
/* prior state after a reset.                                               */
/*                                                                          */

struct STRUCT_TR_OPEN_DATA
    {
    WORD                                open_options;
    NODE_ADDRESS                        opening_node_address;
    ULONG                               group_address;
    ULONG                               functional_address;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : ERROR_LOG                                               */
/*                                                                          */
/* This  is   part   of   the   information   returned   by   a   call   to */
/* driver_get_adapter_status. The error log contains the information from a */
/* READ_ERROR_LOG  SRB  call. All the MAC level error counters are reset to */
/* zero after they are read.                                                */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-112 MAC 000A READ.ERROR.LOG Command                        */
/*                                                                          */

struct STRUCT_ERROR_LOG
    {
    BYTE        line_errors;
    BYTE        reserved_1;
    BYTE        burst_errors;
    BYTE        ari_fci_errors;
    BYTE        reserved_2;
    BYTE        reserved_3;
    BYTE        lost_frame_errors;
    BYTE        congestion_errors;
    BYTE        frame_copied_errors;
    BYTE        reserved_4;
    BYTE        token_errors;
    BYTE        reserved_5;
    BYTE        dma_bus_errors;
    BYTE        dma_parity_errors;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : STATUS_INFORMATION                                      */
/*                                                                          */
/* The status information returned by a call  to  driver_get_status         */
/* includes  whether the adapter is currently open, the current ring status */
/* and the MAC level error log information.                                 */
/*                                                                          */

struct STRUCT_STATUS_INFORMATION
    {
    WBOOLEAN            adapter_open;
    WORD                ring_status;
    ERROR_LOG           error_log;
    };


/****************************************************************************/
/*                                                                          */
/* Values : STATUS_INFORMATION - WORD ring_status                           */
/*                                                                          */
/* These are the  possible  ring  status  values  returned  by  a  call  to */
/* driver_get_adapter_status.                                               */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-61 4.12.2 RING.STATUS                                      */
/*                                                                          */

#define RING_STATUS_SIGNAL_LOSS         0x8000
#define RING_STATUS_HARD_ERROR          0x4000
#define RING_STATUS_SOFT_ERROR          0x2000
#define RING_STATUS_TRANSMIT_BEACON     0x1000
#define RING_STATUS_LOBE_FAULT          0x0800
#define RING_STATUS_AUTO_REMOVAL        0x0400
#define RING_STATUS_REMOVE_RECEIVED     0x0100
#define RING_STATUS_COUNTER_OVERFLOW    0x0080
#define RING_STATUS_SINGLE_STATION      0x0040
#define RING_STATUS_RING_RECOVERY       0x0020


/****************************************************************************/
/*                                                                          */
/* Values : WORD open_options                                               */
/*                                                                          */
/* The     open_options    parameter    to    driver_prepare_adapter    and */
/* driver_open_adapter has the following bit fields defined.                */
/*                                                                          */
/* WARNING : The FORCE_OPEN option is a special Fastmac  option  that  will */
/* open  an  adapter  onto any ring - even if the adapter and ring speed do */
/* not match! Use it with caution.                                          */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Open Adapter SRB                           */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-71 MAC 0003 OPEN command                                   */
/*                                                                          */

#define OPEN_OPT_WRAP_INTERFACE                 0x8000
#define OPEN_OPT_DISABLE_HARD_ERROR             0x4000
#define OPEN_OPT_DISABLE_SOFT_ERROR             0x2000
#define OPEN_OPT_PASS_ADAPTER_MACS              0x1000
#define OPEN_OPT_PASS_ATTENTION_MACS            0x0800
#define OPEN_OPT_FORCE_OPEN                     0x0400      /* Fastmac only */
#define OPEN_OPT_CONTENDER                      0x0100
#define OPEN_OPT_PASS_BEACON_MACS               0x0080
#define OPEN_OPT_EARLY_TOKEN_RELEASE            0x0010
#define OPEN_OPT_COPY_ALL_MACS                  0x0004
#define OPEN_OPT_COPY_ALL_LLCS                  0x0002


/****************************************************************************/
/*                                                                          */
/* Values : WORD dtr_open_options                                           */
/*                                                                          */
/* The dtr_open_options parameter driver_start_adapter has the following    */
/* bit fields defined.                                                      */
/*                                                                          */

#define DTR_OPEN_OPT_ALLOW_CLASSIC              0x0001
#define DTR_OPEN_OPT_ALLOW_DTR                  0x0002


/****************************************************************************/
/*                                                                          */
/* Values : UINT open_mode                                                  */
/*                                                                          */
/* The open_mode value returned from drier_start_adapter in auto open mode  */
/* has the following values.                                                */
/*                                                                          */

#define TR_OPEN_MODE_CLOSED                     0x0000
#define TR_OPEN_MODE_CLASSIC                    0x0001
#define TR_OPEN_MODE_DTR                        0x0002


/****************************************************************************/
/*                                                                          */
/* Values : WORD adapter_card_bus_type                                      */
/*                                                                          */
/* The  following  adapter  card bus types are defined and can be passed to */
/* driver_start_adapter.  Different  adapter  card  bus  types   apply   to */
/* different adapter cards :                                                */
/*                                                                          */
/* ADAPTER_CARD_ISA_BUS_TYPE    16/4 PC or 16/4 AT                          */
/* ADAPTER_CARD_MC_BUS_TYPE     16/4 MC or 16/4 MC 32                       */
/* ADAPTER_CARD_EISA_BUS_TYPE   16/4 EISA mk1 or mk2                        */
/*                                                                          */

#define ADAPTER_CARD_ATULA_BUS_TYPE                 1
#define ADAPTER_CARD_MC_BUS_TYPE                    2
#define ADAPTER_CARD_EISA_BUS_TYPE                  3
#define ADAPTER_CARD_PCI_BUS_TYPE                   4
#define ADAPTER_CARD_SMART16_BUS_TYPE               5
#define ADAPTER_CARD_PCMCIA_BUS_TYPE                6
#define ADAPTER_CARD_PNP_BUS_TYPE                   7
#define ADAPTER_CARD_TI_PCI_BUS_TYPE                8
#define ADAPTER_CARD_PCI2_BUS_TYPE                  9
#define ADAPTER_CARD_CARDBUS_BUS_TYPE               10


/****************************************************************************/
/*                                                                          */
/* Values : WORD transfer_mode, WORD interrupt_number                       */
/*                                                                          */
/* If  POLLING_INTERRUPTS_MODE  is  given  as  the  interrupt   number   to */
/* driver_start_adapter, then polling is assumed to be used.                */
/*                                                                          */
/* NOTE  :  If  using  the DOS example system specific code, then note that */
/* PIO_DATA_TRANSFER_MODE is defined in SYS_IRQ.ASM and  SYS_DMA.ASM        */
/* resepctively.  The  value used here must be, and is, identical.          */
/*                                                                          */

#define PIO_DATA_TRANSFER_MODE          0
#define DMA_DATA_TRANSFER_MODE          1
#define MMIO_DATA_TRANSFER_MODE         2
#define POLLING_INTERRUPTS_MODE         0


/****************************************************************************/
/*                                                                          */
/* Values : Returned from driver_transmit_frame (or some such)              */
/*                                                                          */
/* The value returned by driver_transmit_frame  indicates  how far the code */
/* got with transmitting  the  frame.  FAIL  and  SUCCEED are obvious, WAIT */
/* means  that  the caller should not assume the frame has been transmitted */
/* until some later indication.                                             */
/*                                                                          */

#define DRIVER_TRANSMIT_FAIL 0
#define DRIVER_TRANSMIT_WAIT 1
#define DRIVER_TRANSMIT_SUCCEED 2


/****************************************************************************/
/*                                                                          */
/* Values : Returned from user_receive_frame                                */
/*                                                                          */
/* The value returned by a call to the user_receive_frame routine indicates */
/* whether  the  user wishes to keep the frame in the Fastmac buffer or has */
/* dealt with it (decided it can be thrown away or copied it elsewhere). In */
/* the latter case the frame  can  be  removed  from  the  Fastmac  receive */
/* buffer.                                                                  */
/*                                                                          */

#define DO_NOT_KEEP_FRAME       0
#define KEEP_FRAME              1


/****************************************************************************/
/*                                                                          */
/* Type : card_t                                                            */
/*                                                                          */
/* To support large model compilation,   certain type casts have to be made */
/* to evade compilation errors. The card_t type is used to convert pointers */
/* to  structures  on  the adapter card into unsigned integers so that they */
/* can be truncated to 16 bits without warnings.                            */
/*                                                                          */
/*                                                                          */

#ifndef FTK_SMALL_MODEL
typedef DWORD card_t;
#else
typedef WORD card_t;
#endif


/****************************************************************************/
/*                                                                          */
/* The following structures do not need to be byte packed.                  */
/*                                                                          */

#ifdef FTK_PACKING_OFF
#pragma FTK_PACKING_OFF
#else
#pragma pack(0)
#endif

/****************************************************************************/
/*                                                                          */
/* Values : PROBE_FAILURE                                                   */
/*                                                                          */
/* This value is returned by the driver_probe_adapter function if an error  */
/* occurs.                                                                  */
/*                                                                          */

#define PROBE_FAILURE           0xffff


/****************************************************************************/
/*                                                                          */
/* Values : FTK_UNDEFINED                                                   */
/*                                                                          */
/* This value means that a value is not defined or not used.                */
/*                                                                          */

#define FTK_UNDEFINED           0xeeff


/****************************************************************************/
/*                                                                          */
/* Structure type : PROBE                                                   */
/*                                                                          */
/* The probe structure can be filled in with card details by a call to      */
/* driver_probe_adapter. This is the way the user of the FTK should obtain  */
/* hardware resource information (DMA channel, IRQ number etc) about an     */
/* adapter before calling driver_prepare_adapter and driver_start_adapter.  */
/*                                                                          */

struct STRUCT_PROBE
{
    WORD                   socket;
    UINT                   adapter_card_bus_type;
    UINT                   adapter_card_type;
    UINT                   adapter_card_revision;
    UINT                   adapter_ram_size; 
    WORD                   io_location;
    WORD                   interrupt_number;
    WORD                   dma_channel; 
    UINT                   transfer_mode;
    DWORD                  mmio_base_address;
    DWORD                  pci_handle;
}; 


/****************************************************************************/
/*                                                                          */
/* Types : PREPARE_ARGS                                                     */
/*                                                                          */
/* The driver_prepare_adapter function takes a collection of arguments. An  */
/* instance of this structure is used to pass the arguments.                */ 
/*                                                                          */

struct STRUCT_PREPARE_ARGS
{
    /* User's private information, not interpreted by the FTK. */

    void           * user_information;
                        
#ifdef FMPLUS

    /* Number of FastMAC Plus receive and transmit slots. */

    WORD             number_of_rx_slots;
    WORD             number_of_tx_slots;

#else

    /* Size of the FastMAC receive and transmit buffers. */

    WORD             receive_buffer_byte_size;
    WORD             transmit_buffer_byte_size;

#endif

    /* Requested maximum frame size. */

    WORD             max_frame_size;

    /* Should the watchdog timer be used? */
    
    WBOOLEAN         use_watchdog;
}; 


/****************************************************************************/
/*                                                                          */
/* Types : START_ARGS                                                       */
/*                                                                          */
/* The driver_start_adapter function takes a collection of arguments. An    */
/* instance of this structure is used to pass the arguments. Note that some */ 
/* of the structure fields are filled in on return from                     */
/* driver_start_adapter.                                                    */
/*                                                                          */

struct STRUCT_START_ARGS
{
    /* Adapter family. */

    UINT             adapter_card_bus_type;

    /* Hardware resource details. */

#ifdef PCMCIA_POINT_ENABLE
    UINT             socket;
#endif
    WORD             io_location;
    WORD             dma_channel;
    UINT             transfer_mode; 
    WORD             interrupt_number;

    /* Override DMA/IRQ values on soft programmable adapters? */

    WBOOLEAN         set_dma_channel;
    WBOOLEAN         set_interrupt_number;

    /* Force ring speed to this if possible. 4, 16 or 0 for default. */

    UINT             set_ring_speed;

    /* Base Address for MMIO */

    DWORD            mmio_base_address;

    /* Force PIO transfers to be 16 bit. */

    WBOOLEAN         force_16bit_pio;

    /*
     * Disable bus parity checking?
     */
     
    WBOOLEAN         disable_parity;
     
    /*
     * Used for the Ti PCI ASIC which in hwi_install needs to access PCI
     * Config space.
     */

    DWORD            pci_handle;

    /* Actual maximum frame size. Set on return. */

    WORD             max_frame_size;

    /* Auto open the adapter? */

    WBOOLEAN         auto_open_option;

    /* Open options and addresses for auto open mode. If 
       opening_node_address == 000000000000 the the BIA address 
       is used. */

    WORD             open_options;

    NODE_ADDRESS     opening_node_address;
    ULONG            opening_group_address;
    ULONG            opening_functional_address;

    /* Open options for DTR/Classic mode. */

    WORD             dtr_open_options;

    /* Pointer to the adapter download image. */

    DOWNLOAD_IMAGE * code;

    /* The open mode of the adapter on return. */

    UINT             open_mode;

    /* The open status of the adapter on return. */

    UINT             open_status;

#ifdef FMPLUS

    /* Size of the RX/TX buffers on the adapter. */

    WORD             rx_tx_buffer_size;

#endif

#ifdef FMPLUS
    /*
     * FMPLUS tx_ahead parameter. Top bit (0x8000) must be set if the low
     * 15 bits contain a value which is to be used to override the internal
     * default. If the top bit is clear, the FTK internal default values
     * are used.
     */

    WORD             tx_ahead;

#endif

};


/****************************************************************************/
/*                                                                          */
/* Constants : PCI                                                          */
/*                                                                          */
/* The following constants describe PCI buses and configuration space.      */
/*                                                                          */

#define FTK_PCI_MAX_BUS                0xff
#define FTK_PCI_MAX_DEVICE             0x1f
#define FTK_PCI_MAX_FUNC               0x07
#define FTK_PCI_MAX_DEVICE_AND_FUNC    0xff

#define FTK_PCI_CONFIG_DEVID_VENDORID  ((WORD) 0x00)
#define FTK_PCI_CONFIG_IO_BASE         ((WORD) 0x10)
#define FTK_PCI_CONFIG_MMIO_BASE       ((WORD) 0x14)
#define FTK_PCI_CONFIG_IRQ_LINE        ((WORD) 0x3c)

#define FTK_PCI_IO_BASE_MASK           ((DWORD) 0xfffffffeL)
#define FTK_PCI_MMIO_BASE_MASK         ((DWORD) 0xfffffff0L)


/****************************************************************************/
/*                                                                          */
/* Macros : PCI                                                             */
/*                                                                          */
/* The following macros are used to pack and unpack the bus number,         */
/* device number and function number in a PCI handle.                       */
/*                                                                          */

#define FTK_PCI_PACK_HANDLE(bus_no, device_no, func_no) \
    (WORD) ((((WORD) (bus_no)    & 0x00ff) << 8) |      \
            (((WORD) (device_no) & 0x001f) << 3) |      \
            (((WORD) (func_no)   & 0x0007)))

#define FTK_PCI_PACK_DEVICE_AND_FUNC(bus_no, device_func_no) \
    (WORD) ((((WORD) (bus_no)         & 0x00ff) << 8) |      \
            (((WORD) (device_func_no) & 0x00ff)))

#define FTK_PCI_UNPACK_BUS(handle)    \
    (WORD) (((handle) >> 8) & 0x00ff)

#define FTK_PCI_UNPACK_DEVICE(handle) \
    (WORD) (((handle) >> 3) & 0x001f)

#define FTK_PCI_UNPACK_FUNC(handle)   \
    (WORD) (((handle)     ) & 0x0007)

#define FTK_PCI_UNPACK_DEVICE_AND_FUNC(handle) \
    (WORD) (((handle)     ) & 0x00ff)

/*                                                                          */
/*                                                                          */
/************** End of FTK_USER.H file **************************************/
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE FASTMAC DEFINITIONS                                             */
/*      =======================                                             */
/*                                                                          */
/*      FTK_FM.H : Part of the FASTMAC TOOL-KIT (FTK)                       */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains  the  structures,  constants  etc.,  that  are */
/* relevant  to  Fastmac  and  its  use  by  the  FTK  and are not included */
/* elsewhere. This includes the Fastmac  status  block  structure  and  the */
/* Fastmac use of the EAGLE SIFINT register.                                */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#ifdef FTK_PACKING_ON
#pragma FTK_PACKING_ON
#else
#pragma pack(1)
#endif

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_FM.H belongs :                   */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_FM_H 223


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_FASTMAC_STATUS_BLOCK        FASTMAC_STATUS_BLOCK;
#ifdef FMPLUS
typedef struct STRUCT_RX_SLOT                     RX_SLOT;
typedef struct STRUCT_TX_SLOT                     TX_SLOT;
#endif

/****************************************************************************/
/*                                                                          */
/* Structure type : FASTMAC_STATUS_BLOCK                                    */
/*                                                                          */
/* Fastmac  maintains  a  status  block  that  includes the pointers to the */
/* receive and transmit buffers, as well as the ring status and  a  boolean */
/* flag to say if the adapter is open.                                      */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - Status Block                                               */
/*                                                                          */

struct STRUCT_FASTMAC_STATUS_BLOCK
    {
    WORD         reserved_1;
    WORD         signature;
    WBOOLEAN     adapter_open;                  /* TRUE when open           */
    WORD         open_error;                    /* open error code          */
    WORD         tx_adap_ptr;                   /* transmit buffer pointers */
    WORD         tx_host_ptr;
    WORD         tx_wrap_ptr;
    WORD         rx_adap_ptr;                   /* receive buffer pointers  */
    WORD         rx_wrap_ptr;
    WORD         rx_host_ptr;
    NODE_ADDRESS permanent_address;             /* BIA PROM node address    */
    NODE_ADDRESS open_address;                  /* opening node address     */
    WORD         tx_dma_count;
    WORD         timestamp_ptr;
    WORD         rx_internal_buffer_size;
    WORD         rx_total_buffers_avail;
    WORD         rx_buffers_in_use;
    WORD         rx_frames_lost;
    WORD         watchdog_timer;
    WORD         ring_status;                   /* current ring status      */
    WORD         tx_discarded;
#ifdef FMPLUS
    WORD         rx_slot_start;                 /* where to find rx slots   */
    WORD         tx_slot_start;                 /* where to find tx slots   */
#endif
/*    WORD         reserved_2[1];         old 222 version */
    WORD     frames_repaired;		/* Aborted frame count	    */

    WORD         rxdesc_host_ptr;
    DWORD        rxdesc_queue[1];

    WORD	 tx2_slot_start;
    WORD	 tx2_frames_repaired;
    WORD	 tx2_adap_ptr;
    WORD	 tx2_host_ptr;

    WORD	 reserved_3[4];

    WORD	 enq_vec[2];			/* Enquiry vector           */
    BYTE	 enq_signature[4];		/* Enquiry signature        */
    WORD	 enq_io_location;		/* Enquiry info             */
    WORD	 enq_slot_number;		/* Enquiry info             */
    WORD	 enq_interrupt_number;		/* Enquiry info             */
    WORD	 enq_dma_channel;		/* Enquiry info             */
    WORD	 enq_board_type;		/* Enquiry info             */
    WORD	 enq_board_rev;			/* Enquiry info             */
    WORD	 enq_ram_size;			/* Enquiry info             */
    WORD	 enq_transfer_mode;		/* Enquiry info             */
    BYTE	 enq_product_id[18];		/* Enquiry info             */
    WORD	 enq_ring_speed;		/* Enquiry info             */
    };


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac buffer sizes                                            */
/*                                                                          */
/* The Fastmac receive  and  transmit  buffers  have  minimum  and  maximum */
/* allowable sizes.  The minimum size allows the buffer to contain a single */
/* 1K frame.                                                                */
/*                                                                          */

#define FASTMAC_MAXIMUM_BUFFER_SIZE             0xFF00
#define FASTMAC_MINIMUM_BUFFER_SIZE             0x0404


/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC SIF INTERRUPT (SIFCMD-SIFSTS) REGISTER BITS             */
/*                                                                          */
/* When Fastmac generates an interrupt (via the  SIF  interrupt  register), */
/* the  value  in  the register will indicate the reason for the interrupt. */
/* Also, when the user interrupts Fastmac  (again  via  the  SIF  interrupt */
/* register), the value in the register indicates the reason.               */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - The Interrupt Register                                     */
/*                                                                          */

#define DRIVER_SIFINT_IRQ_FASTMAC       0x8000         /* interrupt Fastmac */

#define DRIVER_SIFINT_FASTMAC_IRQ_MASK  0x00FF

#define DRIVER_SIFINT_SSB_FREE          0x4000
#define DRIVER_SIFINT_SRB_COMMAND       0x2000
#define DRIVER_SIFINT_ARB_FREE          0x1000

#define DRIVER_SIFINT_ACK_SSB_RESPONSE  0x0400
#define DRIVER_SIFINT_ACK_SRB_FREE      0x0200
#define DRIVER_SIFINT_ACK_ARB_COMMAND   0x0100


#define FASTMAC_SIFINT_IRQ_DRIVER       0x0080         /* interrupt driver  */

#define FASTMAC_SIFINT_ADAPTER_CHECK    0x0008
#define FASTMAC_SIFINT_SSB_RESPONSE     0x0004
#define FASTMAC_SIFINT_SRB_FREE         0x0002
#define FASTMAC_SIFINT_ARB_COMMAND      0x0001

#define FASTMAC_SIFINT_RECEIVE          0x0000


/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC DIO LOCATIONS                                           */
/*                                                                          */
/* There  are certain fixed locations in DIO space containing pointers that */
/* are accessed by the driver to determine DIO locations where  the  driver */
/* must  read  or  store  Fastmac  information. These pointers identify the */
/* location of such things as the SRB and status block (STB).  The pointers */
/* are at DIO locations 0x00011000 - 0x00011008. The values  defined  below */
/* give the location of the pointers within the EAGLE DATA page 0x00010000. */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Shared RAM Format                                          */
/*                                                                          */

#define DIO_LOCATION_SSB_POINTER        0x1000
#define DIO_LOCATION_SRB_POINTER        0x1002
#define DIO_LOCATION_ARB_POINTER        0x1004
#define DIO_LOCATION_STB_POINTER        0x1006          /* status block     */
#define DIO_LOCATION_IPB_POINTER        0x1008          /* init block       */
#define DIO_LOCATION_DMA_CONTROL        0x100A
#define DIO_LOCATION_DMA_POINTER        0x100C


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac product id string                                       */
/*                                                                          */
/* The product id string for Fastmac that is used by certain management MAC */
/* frames.  If the Fastmac auto-open feature is used then the product id is */
/* always "THE MADGE FASTMAC". If an OPEN_ADAPTER SRB then the FTK  product */
/* id is "FTK MADGE FASTMAC".                                               */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Open Adapter SRB                           */
/*                                                                          */

#define SIZEOF_PRODUCT_ID       18
#ifdef FMPLUS
#define FASTMAC_PRODUCT_ID      "FTK MADGE FM PLUS"
#else
#define FASTMAC_PRODUCT_ID      "FTK MADGE FASTMAC"
#endif

/****************************************************************************/
/*                                                                          */
/* Global variable : ftk_product_inst_id                                    */
/*                                                                          */
/* Value of the product ID strings set when an open adapter SRB             */
/* is generated. This is set to FASTMAC_PRODUCT_ID in DRV_SRB.C.            */
/*                                                                          */

extern char ftk_product_inst_id[SIZEOF_PRODUCT_ID];

/****************************************************************************/
/*                                                                          */
/* Values : Fastmac buffer format                                           */
/*                                                                          */
/* The format in which frames are kept in the Fastmac  buffers  includes  a */
/* header  to  the  frame  containing  the  length of the entire header and */
/* frame, and a timestamp.                                                  */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - The Fastmac Algorithm                                      */
/*                                                                          */


#define FASTMAC_BUFFER_HEADER_SIZE      4

#define FASTMAC_HEADER_LENGTH_OFFSET    0
#define FASTMAC_HEADER_STAMP_OFFSET     2


#ifdef FMPLUS
/****************************************************************************/
/*                                                                          */
/* Structure type : RX_SLOT                                                 */
/*                                                                          */
/* Fastmac Plus  maintains  a  slot  structure on the card for each receive */
/* buffer on the host.  These include the address of the buffer, the length */
/* of any frame in it,  and the receive  status of any frame there.  When a */
/* frame is received, Fastmac Plus updates the length and status fields.    */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Receive Details: Slot Structure                            */
/*                                                                          */

struct STRUCT_RX_SLOT
	{
	WORD        buffer_len;
	WORD        reserved;
	WORD        buffer_hiw;
	WORD        buffer_low;
	WORD        rx_status;
	WORD        next_slot;
	};


/****************************************************************************/
/*                                                                          */
/* Structure type : TX_SLOT                                                 */
/*                                                                          */
/* Fastmac Plus maintains a number of slot structures on the card, to allow */
/* transmit pipelining. Each of these structures includes  two  fields  for */
/* host buffers and lengths - one is for a small buffer, less than the size */
/* of a buffer on the adapter card, and the other is for a large buffer, up */
/* to the maximum frame size. There is also a status field so that the host */
/* transmit code can monitor the progress of the transmit.                  */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Transmit Details: Slot Structure                           */
/*                                                                          */

struct STRUCT_TX_SLOT
	{
	WORD        tx_status;
	WORD        small_buffer_len;
	WORD        large_buffer_len;
	WORD        reserved[2];
	WORD        small_buffer_hiw;
	WORD        small_buffer_low;
	WORD        next_slot;
	WORD        large_buffer_hiw;
	WORD        large_buffer_low;
	};


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac Plus min/max slot numbers                               */
/*                                                                          */
/* Fastmac Plus places certain restrictions on the numbers of transmit  and */
/* receive slots that can be allocated. These constants specify the values. */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Initialisation : TMS Load Parms                            */
/*                                                                          */

#define FMPLUS_MAX_RX_SLOTS     32
#define FMPLUS_MIN_RX_SLOTS     4

#define FMPLUS_MAX_TX_SLOTS     32
#define FMPLUS_MIN_TX_SLOTS     4


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac Plus Receive Status Mask                                */
/*                                                                          */
/* By  bitwise  AND-ing the mask here with the receive status read from the */
/* receive slot,  code can  determine whether the received frame is good or */
/* not.  If the result is zero,  the frame is good,  otherwise it is a junk */
/* frame.                                                                   */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Receive Status Processing                                  */
/*                                                                          */

#define GOOD_RX_FRAME_MASK  ((WORD) 0x5E00)


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac Plus Transmit Status Mask And Values                    */
/*                                                                          */
/* By  bitwise  AND-ing the good frame mask with the transmit status read   */
/* from the  receive  slot,  code  can  determine  whether  the  frame  was */
/* transmitted properly or not.  If  more  detail  is required, the receive */
/* status mask can be used to check various conditions  in  the transmitted */
/* frame when it returned to the adapter.                                   */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Transmit Status Processing                                 */
/*                                                                          */

#define GOOD_TX_FRAME_MASK  ((WORD) 0x5F00)
#define GOOD_TX_FRAME_VALUE ((WORD) 0x0100)

#define TX_RECEIVE_STATUS_MASK    ((WORD) 0x0700)
#define TX_RECEIVE_LOST_FRAME     ((WORD) 0x0300)
#define TX_RECEIVE_CORRUPT_TOKEN  ((WORD) 0x0500)
#define TX_RECEIVE_IMPLICIT_ABORT ((WORD) 0x0700)


/****************************************************************************/
/*                                                                          */
/* Values : Fastmac Plus Zero Length Small Buffer value                     */
/*                                                                          */
/* When transmitting a frame that exists only in a large buffer,  a special */
/* non-zero value must be written to the small buffer length field  of  the */
/* receive slot (this is because Fastmac Plus uses zero there  to  indicate */
/* that a transmit has completed, and waits for it to change  before trying */
/* to transmit any more from that slot). This special value is defined here.*/
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Transmit Details                                           */
/*                                                                          */

#define FMPLUS_SBUFF_ZERO_LENGTH ((WORD)(0x8000))

#endif

#ifdef FTK_PACKING_OFF
#pragma FTK_PACKING_OFF
#else
#pragma pack(0)
#endif

/*                                                                          */
/*                                                                          */
/************** End of FTK_FM.H file ****************************************/
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE SRB DEFINITIONS                                                 */
/*      ===================                                                 */
/*                                                                          */
/*      FTK_SRB.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains all the definitions and  structures  that  are */
/* required for the SRB interface.                                          */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface                                              */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#ifdef FTK_PACKING_ON
#pragma FTK_PACKING_ON
#else
#pragma pack(1)
#endif

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_SRB.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_SRB_H 223


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_SRB_HEADER                   SRB_HEADER;

typedef union  UNION_SRB_GENERAL                   SRB_GENERAL;

typedef struct STRUCT_SRB_MODIFY_OPEN_PARMS        SRB_MODIFY_OPEN_PARMS;

typedef struct STRUCT_SRB_OPEN_ADAPTER             SRB_OPEN_ADAPTER;

typedef struct STRUCT_SRB_CLOSE_ADAPTER            SRB_CLOSE_ADAPTER;

typedef struct STRUCT_SRB_SET_MULTICAST_ADDR       SRB_SET_GROUP_ADDRESS;
typedef struct STRUCT_SRB_SET_MULTICAST_ADDR       SRB_SET_FUNCTIONAL_ADDRESS;

typedef struct STRUCT_SRB_READ_ERROR_LOG           SRB_READ_ERROR_LOG;

typedef struct STRUCT_SRB_SET_BRIDGE_PARMS         SRB_SET_BRIDGE_PARMS;

typedef struct STRUCT_SRB_SET_PROD_INST_ID         SRB_SET_PROD_INST_ID;

/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_HEADER                                              */
/*                                                                          */
/* All  SRBs  have  a  common  header.  With  Fastmac  all  SRBs   complete */
/* synchronously,  ie.  the return code is  never E_FF_COMMAND_NOT_COMPLETE */
/* and the correlator field is not used.                                    */
/*                                                                          */

struct STRUCT_SRB_HEADER
    {
    BYTE        function;
    BYTE        correlator;
    BYTE        return_code;
    BYTE        reserved;
    };


/****************************************************************************/
/*                                                                          */
/* Values : SRB_HEADER - BYTE function                                      */
/*                                                                          */
/* These are the SRBs currently supported by the FTK.                       */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface                                              */
/*                                                                          */

#define MODIFY_OPEN_PARMS_SRB           0x01
#define OPEN_ADAPTER_SRB                0x03
#define CLOSE_ADAPTER_SRB               0x04
#define SET_GROUP_ADDRESS_SRB           0x06
#define SET_FUNCTIONAL_ADDRESS_SRB      0x07
#define READ_ERROR_LOG_SRB              0x08
#define SET_BRIDGE_PARMS_SRB            0x09
#define FMPLUS_SPECIFIC_SRB             0xC3

#define SET_PROD_INST_ID_SUBCODE        4

/****************************************************************************/
/*                                                                          */
/* Values : SRB_HEADER - BYTE return_code                                   */
/*                                                                          */
/* These are defined in FTK_ERR.H                                           */
/*                                                                          */


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_MODIFY_OPEN_PARMS                                   */
/*                                                                          */
/* This SRB is issued to modify  the  open  options  for  an  adapter.  The */
/* adapter  can  be  in  auto-open  mode or have been opened by an SRB (see */
/* below).                                                                  */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Modify Open Parms SRB                      */
/*                                                                          */

struct STRUCT_SRB_MODIFY_OPEN_PARMS
    {
    SRB_HEADER          header;
    WORD                open_options;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_OPEN_ADAPTER                                        */
/*                                                                          */
/* This SRB is issued to open the adapter with the given node  address  and */
/* functional and group addresses.                                          */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Open Adapter SRB                           */
/*                                                                          */

struct STRUCT_SRB_OPEN_ADAPTER
    {
    SRB_HEADER          header;
    BYTE                reserved_1[2];
    WORD                open_error;             /* secondary error code     */
    WORD                open_options;           /* see USER.H for options   */
    NODE_ADDRESS        open_address;
    DWORD               group_address;
    DWORD               functional_address;
    WORD                reserved_2;
    WORD                reserved_3;
    WORD                reserved_4;
    BYTE                reserved_5;
    BYTE                reserved_6;
    BYTE                reserved_7[10];
    char                product_id[SIZEOF_PRODUCT_ID];  /* network managers */
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_CLOSE_ADAPTER                                       */
/*                                                                          */
/* The SRB for closing the adapter consists of just an SRB header.          */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Close Adapter SRB                          */
/*                                                                          */

struct STRUCT_SRB_CLOSE_ADAPTER
    {
    SRB_HEADER          header;
    };


/****************************************************************************/
/*                                                                          */
/* Structure types : SRB_SET_GROUP_ADDRESS                                  */
/*                   SRB_SET_FUNCTIONAL_ADDRESS                             */
/*                                                                          */
/* This  structure  is  used  for  SRBs for setting both the functional and */
/* group addresses of an adapter.                                           */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Set Group/Functional Address SRB           */
/*                                                                          */

struct STRUCT_SRB_SET_MULTICAST_ADDR
    {
    SRB_HEADER          header;
    WORD                reserved;
    MULTI_ADDRESS       multi_address;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_READ_ERROR_LOG                                      */
/*                                                                          */
/* This SRB is used to get MAC  error  log  counter  information  from  the */
/* adapter. The counters are reset to zero as they are read.                */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Read Error Log SRB                         */
/*                                                                          */

struct STRUCT_SRB_READ_ERROR_LOG
    {
    SRB_HEADER          header;
    WORD                reserved;
    ERROR_LOG           error_log;              /* defined in FTK_USER.H    */
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_SET_BRIDGE_PARMS                                    */
/*                                                                          */
/* This  SRB  is  used to configure the TI Source Routing Accelerator (SRA) */
/* ASIC.  The adapter must be open for this SRB to work.                    */
/* The order for the fields in the options word is :                        */
/*     Bit 15 (MSB) : single-route-broadcast                                */
/*     Bit 14 - 10  : reserved (all zero)                                   */
/*     Bit  9 -  4  : maximum route length                                  */
/*     Bit  3 -  0  : number of bridge bits                                 */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface : Set Bridge Parms SRB                       */
/*                                                                          */

struct STRUCT_SRB_SET_BRIDGE_PARMS
    {
    SRB_HEADER      header;
	WORD            options;
	UINT			this_ring;
	UINT			that_ring;
	UINT			bridge_num;
    };

#define SRB_SBP_DFLT_BRIDGE_BITS 4
#define SRB_SBP_DFLT_ROUTE_LEN   18


struct STRUCT_SRB_SET_PROD_INST_ID
	{
	SRB_HEADER       header;
	WORD             subcode;
	BYTE             product_id[SIZEOF_PRODUCT_ID];
	};

/****************************************************************************/
/*                                                                          */
/* Structure type : SRB_GENERAL                                             */
/*                                                                          */
/* This SRB structure is a union of all the possible SRB structures used by */
/* the FTK. Included in the union is an SRB header structure  so  that  the */
/* header of an SRB can be accessed without knowing the type of SRB.        */
/*                                                                          */

union UNION_SRB_GENERAL
    {
    SRB_HEADER                  header;
    SRB_MODIFY_OPEN_PARMS       mod_parms;
    SRB_OPEN_ADAPTER            open_adap;
    SRB_CLOSE_ADAPTER           close_adap;
    SRB_SET_GROUP_ADDRESS       set_group;
    SRB_SET_FUNCTIONAL_ADDRESS  set_func;
    SRB_READ_ERROR_LOG          err_log;
    SRB_SET_BRIDGE_PARMS        set_bridge_parms;
    SRB_SET_PROD_INST_ID        set_prod_inst_id;
    };

#ifdef FTK_PACKING_OFF
#pragma FTK_PACKING_OFF
#else
#pragma pack(0)
#endif

/*                                                                          */
/*                                                                          */
/************** End of FTK_SRB.H file ***************************************/
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE INITIALIZATION BLOCK DEFINITIONS                                */
/*      ====================================                                */
/*                                                                          */
/*      FTK_INIT.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for the structures that go  to */
/* make  the  initialization block that is needed in order to initialize an */
/* adapter card that is in use by the FTK.                                  */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#ifdef FTK_PACKING_ON
#pragma FTK_PACKING_ON
#else
#pragma pack(1)
#endif

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_INIT.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_INIT_H 223


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_INITIALIZATION_BLOCK        INITIALIZATION_BLOCK;
typedef struct STRUCT_TI_INIT_PARMS               TI_INIT_PARMS;
typedef struct STRUCT_MADGE_INIT_PARMS_HEADER     MADGE_INIT_PARMS_HEADER;
typedef struct STRUCT_SMART_INIT_PARMS            SMART_INIT_PARMS;
typedef struct STRUCT_FASTMAC_INIT_PARMS          FASTMAC_INIT_PARMS;

/****************************************************************************/
/*                                                                          */
/* Structure type : TI_INIT_PARMS                                           */
/*                                                                          */
/* The TI initialization parameters are exactly those  defined  by  TI  for */
/* initializing an adapter based on the EAGLE chipset except for a  special */
/* byte  of  16/4 MC 32 configuration information. This byte overrides a TI */
/* initialization block field not used by Madge adapter cards.              */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-42 4.6 Adapter Initialization                              */
/*                                                                          */

struct STRUCT_TI_INIT_PARMS
    {
    WORD        init_options;
    WORD        madge_mc32_config;              /* special MC 32 data       */
    BYTE        reserved[4];                    /* ignored by Madge cards   */
    WORD        rx_burst;
    WORD        tx_burst;
    BYTE        parity_retry;
    BYTE        dma_retry;
    DWORD       scb_addr;                       /* 32 bit phys host addr    */
    DWORD       ssb_addr;                       /* 32 bit phys host addr    */
    };


/****************************************************************************/
/*                                                                          */
/* Values : TI_INIT_PARMS - WORD init_options                               */
/*                                                                          */
/* The init_options are set up for burst mode DMA.                          */
/*                                                                          */

#define TI_INIT_OPTIONS_BURST_DMA               0x9F00


/****************************************************************************/
/*                                                                          */
/* Values : TI_INIT_PARMS - WORD madge_mc32_config                          */
/*                                                                          */
/* This value is used to configure MC and ISA CLIENT cards.                 */
/*                                                                          */

#define MC_AND_ISACP_USE_PIO                    0x0040


/****************************************************************************/
/*                                                                          */
/* Values : TI_INIT_PARMS - BYTE parity_retry, BYTE dma_retry               */
/*                                                                          */
/* A default value is used by the FTK for the parity and dma retry counts.  */
/*                                                                          */

#define TI_INIT_RETRY_DEFAULT           5


/****************************************************************************/
/*                                                                          */
/* Structure type : MADGE_INIT_PARMS_HEADER                                 */
/*                                                                          */
/* This is the common header to all  Madge  smart  software  initialization */
/* parameter  blocks  -  that  is, in this case, the header for the general */
/* smart software MAC level parameters and the Fastmac specific parameters. */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Bring-Up and Initialization                                */
/*                                                                          */


struct STRUCT_MADGE_INIT_PARMS_HEADER
    {
    WORD                length;                 /* byte length of parms     */
    WORD                signature;              /* parms specific           */
    WORD                reserved;               /* must be 0                */
    WORD                version;                /* parms specific           */
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : SMART_INIT_PARMS                                        */
/*                                                                          */
/* This   structure   contains   general  MAC  level  parameters  for  when */
/* downloading any Madge smart software.                                    */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Bring-Up and Initialization                                */
/*                                                                          */

struct STRUCT_SMART_INIT_PARMS
    {

    MADGE_INIT_PARMS_HEADER     header;

    WORD                reserved_1;             /* must be 0                */
    NODE_ADDRESS        permanent_address;      /* BIA PROM node address    */
    WORD                rx_tx_buffer_size;      /* 0 => default 1K-8 bytes  */
    DWORD               reserved_2;             /* must be 0                */
    WORD                dma_buffer_size;        /* 0 => no limit            */
    WORD                max_buffer_ram;         /* 0 => default 2MB         */
    WORD                min_buffer_ram;         /* 0 => default 10K         */
    WORD                sif_burst_size;         /* 0 => no limit            */
    WORD                reserved_3;             /* Must be zero             */
    };


/****************************************************************************/
/*                                                                          */
/* Values : SMART_INIT_PARMS    - header. WORD signature, WORD version      */
/*                                                                          */
/* The  values  for  the  header  of  the  general smart software MAC level */
/* paramters strcture.                                                      */
/*                                                                          */

#define SMART_INIT_HEADER_SIGNATURE     0x0007
#define SMART_INIT_HEADER_VERSION       0x0102
#ifdef FMPLUS
#define SMART_INIT_MIN_RAM_DEFAULT      0x0002
#endif

/****************************************************************************/
/*                                                                          */
/* Structure type : SMART_FASTMAC_INIT_PARMS                                */
/*                                                                          */
/* The  Fastmac  initialization  parameters  as  specified  in  the Fastmac */
/* documentation.                                                           */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - Initialization                                             */
/*                                                                          */


struct STRUCT_FASTMAC_INIT_PARMS
    {

    MADGE_INIT_PARMS_HEADER     header;

    WORD		        feature_flags;
    WORD		        int_flags;
			
    WORD                open_options;           /* only for auto_open       */
    NODE_ADDRESS        open_address;           /* only for auto_open       */
    DWORD               group_address;          /* only for auto_open       */
    DWORD               functional_address;     /* only for auto_open       */

    DWORD               rx_buf_physaddr;        /* set to zero for FMPlus   */
    WORD                rx_buf_size;            /* (see rx_bufs/rx_slots)   */
    WORD                rx_buf_space;

    DWORD               tx_buf_physaddr;        /* set to zero for FMPlus   */
    WORD                tx_buf_size;            /* (see tx_bufs/tx_slots)   */
    WORD                tx_buf_space;

    WORD                max_frame_size;         /* for both rx and tx       */
    WORD                size_rxdesc_queue;      /* set to zero for FMPlus   */
    WORD                max_rx_dma;             /* set to zero for FMPlus   */

    WORD                group_root_address;     /* only for auto_open       */
#ifdef FMPLUS
    WORD                rx_bufs;                /* # of internel rx buffers */
    WORD                tx_bufs;                /* # of internal tx buffers */
    WORD                rx_slots;               /* # of host rx buffers     */
    WORD                tx_slots;               /* # of host tx buffers     */
    WORD                tx_ahead;               /* Leave as zero            */
    
    WORD                second_tx_slots;        /* second transmit queue    */
    WORD                second_tx_ahead;
    WORD                ratio;
    DWORD               second_tx_buf_physaddr;
    
    WORD                dtr_open_options;       /* open options for DTR     */
#endif
    };


/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC_INIT_PARMS - header. WORD signature, WORD version       */
/*                                                                          */
/* The values  for  the  header  of  the  Fastmac  specific  initialization */
/* parameter block.                                                         */
/*                                                                          */

#ifdef FMPLUS
#define FMPLUS_INIT_HEADER_SIGNATURE   0x000E
#define FMPLUS_INIT_HEADER_VERSION     0x0200   /* NOT Fastmac version!     */
#else
#define FASTMAC_INIT_HEADER_SIGNATURE  0x0005
#define FASTMAC_INIT_HEADER_VERSION    0x0405   /* NOT Fastmac version!     */
#endif

/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC_INIT_PARMS  - WORD feature_flags                        */
/*                                                                          */
/* The feature flag bit signifcant  values  as  described  in  the  Fastmac */
/* specification document.                                                  */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - Initialization                                             */
/*                                                                          */

#define FEATURE_FLAG_AUTO_OPEN                  0x0001
#define FEATURE_FLAG_NOVELL                     0x0002
#define FEATURE_FLAG_SELL_BY_DATE               0x0004
#define FEATURE_FLAG_PASS_RX_CRC                0x0008
#define FEATURE_FLAG_WATCHDOG_TIMER             0x0020
#define FEATURE_FLAG_DISCARD_BEACON_TX          0x0040
#define FEATURE_FLAG_TRUNCATE_DMA               0x0080
#define FEATURE_FLAG_DELAY_RX                   0x0100
#define FEATURE_FLAG_ONE_INT_PER_RX             0x0200
#define FEATURE_FLAG_NEW_INIT_BLOCK             0x0400
#define FEATURE_FLAG_AUTO_OPEN_ON_OPEN          0x0800
#define FEATURE_FLAG_DISABLE_TX_FAIRNES         0x1000
#ifdef FMPLUS
#define FEATURE_FLAG_FMPLUS_ALWAYS_SET          0x0000
#endif

/* Yes, the FMPLUS_ALWAYS_SET bit is ZERO, because in fact it must NOT      */
/* always be set! This is an unfortunate historical legacy...               */


/****************************************************************************/
/*                                                                          */
/* Values : FASTMAC_INIT_PARMS  - WORD int_flags                            */
/*                                                                          */
/* The interrupt flag bit significant values as  described  in the  Fastmac */
/* Plus specification document.                                             */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Initialization : TMS Load Parms                            */
/*                                                                          */

#define INT_FLAG_TX_BUF_EMPTY           0x0001
#define INT_FLAG_TIMER_TICK_ARB         0x0002
#define INT_FLAG_RING_STATUS_ARB        0x0004
#ifdef FMPLUS
#define INT_FLAG_LARGE_DMA              0x0008
#define INT_FLAG_RX                     0x0010
#endif

#ifdef FMPLUS
/****************************************************************************/
/*                                                                          */
/* Values : Magic Fastmac Plus numbers to do with buffers on the adapter    */
/*                                                                          */
/* The size  of  buffers  on  the adapter card can be set with in the init. */
/* block with  the  rx_tx_buffer_size field.  The minimum value and default */
/* values are specified here. Also, there are  numbers giving the amount of */
/* memory (in bytes) available for buffers on  adapter cards of various RAM */
/* sizes.                                                                   */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Plus Programming Specification             */
/*             - Initialization : SMTMAC Load Parms                         */
/*                                                                          */

#define FMPLUS_MIN_TXRX_BUFF_SIZE       97

#define FMPLUS_DEFAULT_BUFF_SIZE_SMALL  504     /* For EISA/MC32 cards      */
#define FMPLUS_DEFAULT_BUFF_SIZE_LARGE  1016    /* For all other cards      */

#define FMPLUS_MAX_BUFFMEM_IN_128K       60056  /* Bytes available for buffs*/
#define FMPLUS_MAX_BUFFMEM_IN_256K      190104  /* on cards of 128K,256K, & */
#define FMPLUS_MAX_BUFFMEM_IN_512K      450200  /* 512K RAM.                */
#endif

/****************************************************************************/
/*                                                                          */
/* Structure type : INITIALIZATION_BLOCK                                    */
/*                                                                          */
/* The  initialization  block  consists  of  3  parts  -  22  bytes  of  TI */
/* intialization parameters, general smart  software  MAC level parameters, */
/* and the Fastmac specific parameters.                                     */
/*                                                                          */

struct STRUCT_INITIALIZATION_BLOCK
    {
    TI_INIT_PARMS               ti_parms;
    SMART_INIT_PARMS            smart_parms;
    FASTMAC_INIT_PARMS          fastmac_parms;
    };


#ifdef FTK_PACKING_OFF
#pragma FTK_PACKING_OFF
#else
#pragma pack(0)
#endif

/*                                                                          */
/*                                                                          */
/************** End of FTK_INIT.H file **************************************/
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE ERROR DEFINITIONS                                               */
/*      =====================                                               */
/*                                                                          */
/*      FTK_ERR.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the structures associated with error  handling */
/* and all the possible error codes (types and values) produced by the FTK. */
/*                                                                          */
/* A string of text describing each of the possible error codes  (type  and */
/* value) can be found in the error tables in FTK_TAB.H.                    */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_ERR.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_ERR_H 223


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_ERROR_RECORD              ERROR_RECORD;
typedef struct STRUCT_ERROR_MESSAGE             ERROR_MESSAGE;
typedef struct STRUCT_ERROR_MESSAGE_RECORD      ERROR_MESSAGE_RECORD;


/****************************************************************************/
/*                                                                          */
/* Structure type : ERROR_MESSAGE_RECORD                                    */
/*                                                                          */
/* The error message tables (see FTK_TAB.H) are made up of elements of this */
/* structure.  Each  error  message  string has associated with it an error */
/* value of the type of error that the table is for.                        */
/*                                                                          */

struct STRUCT_ERROR_MESSAGE_RECORD
    {
    BYTE        value;
    char *      err_msg_string;
    };


/****************************************************************************/
/*                                                                          */
/* Structure type : ERROR_MESSAGE                                           */
/*                                                                          */
/* Associated with an adapter structure is an error message  for  the  last */
/* error  to  occur  on  the  adapter.  It  is  filled  in  by  a  call  to */
/* driver_explain_error and a pointer to it is returned to the user.        */
/*                                                                          */

#define MAX_ERROR_MESSAGE_LENGTH        600

struct STRUCT_ERROR_MESSAGE
    {
    char        string[MAX_ERROR_MESSAGE_LENGTH];
    };

/****************************************************************************/
/*                                                                          */
/* Structure type : ERROR_RECORD                                            */
/*                                                                          */
/* This structure is used for recording error  information.   There  is  an */
/* element  of  this structure, associated with every adapter, that is used */
/* to record the current error status of the adapter.                       */
/*                                                                          */

struct STRUCT_ERROR_RECORD
    {
    BYTE   type;
    BYTE   value;
    };


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - BYTE type                                        */
/*                                                                          */
/* The following lists the type of errors that can occur. Some of these are */
/* fatal in that an adapter for which they occur can  not  subsequently  be */
/* used. The value 0 (zero) is used to indicate no error has yet occured.   */
/*                                                                          */

#define ERROR_TYPE_NONE         (BYTE) 0x00     /* no error                 */
#define ERROR_TYPE_SRB          (BYTE) 0x01     /* non-fatal error          */
#define ERROR_TYPE_OPEN         (BYTE) 0x02     /* non-fatal error          */
#define ERROR_TYPE_DATA_XFER    (BYTE) 0x03     /* non-fatal error          */
#define ERROR_TYPE_DRIVER       (BYTE) 0x04     /* fatal error              */
#define ERROR_TYPE_HWI          (BYTE) 0x05     /* fatal error              */
#define ERROR_TYPE_BRING_UP     (BYTE) 0x06     /* fatal error              */
#define ERROR_TYPE_INIT         (BYTE) 0x07     /* fatal error              */
#define ERROR_TYPE_AUTO_OPEN    (BYTE) 0x08     /* fatal error              */
#define ERROR_TYPE_ADAPTER      (BYTE) 0x09     /* fatal error              */
#define ERROR_TYPE_CS           (BYTE) 0x0A     /* fatal error              */


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_SRB .  BYTE value                     */
/*          SRB_HEADER   - BYTE return_code                                 */
/*                                                                          */
/* The non-fatal SRB error type uses for error values the return  codes  in */
/* the  SRB  header.  For  the SRBs that are supported by the FTK there are */
/* only a limited number of possible error  values  that  can  occur.  Note */
/* however,  that a failing open adapter SRB call may cause OPEN error type */
/* errors and not just SRB error type errors (see ERROR_TYPE_OPEN below).   */
/*                                                                          */
/* REFERENCE : The Madge Fastmac Interface Specification                    */
/*             - SRB Interface                                              */
/*                                                                          */

#define SRB_E_00_SUCCESS                        (BYTE) 0x00
#define SRB_E_03_ADAPTER_OPEN                   (BYTE) 0x03
#define SRB_E_04_ADAPTER_CLOSED                 (BYTE) 0x04
#define SRB_E_06_INVALID_OPTIONS                (BYTE) 0x06
#define SRB_E_07_CMD_CANCELLED_FAIL             (BYTE) 0x07
#define SRB_E_32_INVALID_NODE_ADDRESS           (BYTE) 0x32


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_OPEN . BYTE value                     */
/*                                                                          */
/* Non-fatal  open  errors occur when an open adapter SRB returns with code */
/* SRB_E_07_CMD_CANCELLED_FAILED. In this case the error type is changed to */
/* ERROR_TYPE_OPEN and the error value is changed  to  show  that  an  open */
/* error has occured. The actual open error details are determined when the */
/* user calls driver_explain_error (see TMS Open Error Codes below).        */
/*                                                                          */

#define OPEN_E_01_OPEN_ERROR            (BYTE) 0x01


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_DATA_XFER . BYTE value                */
/*                                                                          */
/* There is only one possible non-fatal data transfer error. This occurs on */
/* an attempted transmit when the Fastmac transmit buffer is full.          */
/*                                                                          */

#define DATA_XFER_E_01_BUFFER_FULL      (BYTE) 0x01


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_DRIVER . BYTE value                   */
/*                                                                          */
/* The DRIVER part of the  FTK  can  generate  the  following  fatal  error */
/* values.  These can, for example, be caused by sys_alloc routines failing */
/* or passing an illegal adapter handle to a driver routine. See  FTK_TAB.H */
/* for more details.                                                        */
/*                                                                          */

#define DRIVER_E_01_INVALID_HANDLE      (BYTE) 0x01
#define DRIVER_E_02_NO_ADAP_STRUCT      (BYTE) 0x02
#define DRIVER_E_03_FAIL_ALLOC_STATUS   (BYTE) 0x03
#define DRIVER_E_04_FAIL_ALLOC_INIT     (BYTE) 0x04
#define DRIVER_E_05_FAIL_ALLOC_RX_BUF   (BYTE) 0x05
#define DRIVER_E_06_FAIL_ALLOC_TX_BUF   (BYTE) 0x06
#define DRIVER_E_07_NOT_PREPARED        (BYTE) 0x07
#define DRIVER_E_08_NOT_RUNNING         (BYTE) 0x08
#define DRIVER_E_09_SRB_NOT_FREE        (BYTE) 0x09
#define DRIVER_E_0A_RX_BUF_BAD_SIZE     (BYTE) 0x0A
#define DRIVER_E_0B_RX_BUF_NOT_DWORD    (BYTE) 0x0B
#define DRIVER_E_0C_TX_BUF_BAD_SIZE     (BYTE) 0x0C
#define DRIVER_E_0D_TX_BUF_NOT_DWORD    (BYTE) 0x0D
#define DRIVER_E_0E_BAD_RX_METHOD       (BYTE) 0x0E
#define DRIVER_E_0F_WRONG_RX_METHOD     (BYTE) 0x0F

#define DRIVER_E_10_BAD_RX_SLOT_NUMBER  (BYTE) 0x10
#define DRIVER_E_11_BAD_TX_SLOT_NUMBER  (BYTE) 0x11
#define DRIVER_E_12_FAIL_ALLOC_DMA_BUF  (BYTE) 0x12
#define DRIVER_E_13_BAD_FRAME_SIZE      (BYTE) 0x13


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_HWI . BYTE value                      */
/*                                                                          */
/* The HWI part of the FTK can generate the following fatal  error  values. */
/* Most  of  these  are  caused  by  the  user  supplying illegal values to */
/* driver_start_adapter. See FTK_TAB.H for more details.                    */
/*                                                                          */

#define HWI_E_01_BAD_CARD_BUS_TYPE      (BYTE) 0x01
#define HWI_E_02_BAD_IO_LOCATION        (BYTE) 0x02
#define HWI_E_03_BAD_INTERRUPT_NUMBER   (BYTE) 0x03
#define HWI_E_04_BAD_DMA_CHANNEL        (BYTE) 0x04
#define HWI_E_05_ADAPTER_NOT_FOUND      (BYTE) 0x05
#define HWI_E_06_CANNOT_USE_DMA         (BYTE) 0x06
#define HWI_E_07_FAILED_TEST_DMA        (BYTE) 0x07
#define HWI_E_08_BAD_DOWNLOAD           (BYTE) 0x08
#define HWI_E_09_BAD_DOWNLOAD_IMAGE     (BYTE) 0x09
#define HWI_E_0A_NO_DOWNLOAD_IMAGE      (BYTE) 0x0A
#define HWI_E_0B_FAIL_IRQ_ENABLE        (BYTE) 0x0B
#define HWI_E_0C_FAIL_DMA_ENABLE        (BYTE) 0x0C
#define HWI_E_0D_CARD_NOT_ENABLED       (BYTE) 0x0D
#define HWI_E_0E_NO_SPEED_SELECTED      (BYTE) 0x0E
#define HWI_E_0F_BAD_FASTMAC_INIT       (BYTE) 0x0F

#define HWI_E_10_BAD_TX_RX_BUFF_SIZE    (BYTE) 0x10
#define HWI_E_11_TOO_MANY_TX_RX_BUFFS   (BYTE) 0x11
#define HWI_E_12_BAD_SCB_ALLOC          (BYTE) 0x12
#define HWI_E_13_BAD_SSB_ALLOC          (BYTE) 0x13
#define HWI_E_14_BAD_PCI_MACHINE        (BYTE) 0x14
#define HWI_E_15_BAD_PCI_MEMORY         (BYTE) 0x15
#define HWI_E_16_PCI_3BYTE_PROBLEM      (BYTE) 0x16
#define HWI_E_17_BAD_TRANSFER_MODE      (BYTE) 0x17

/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_BRING_UP . BYTE value                 */
/*                                                                          */
/* During an attempt to perform bring-up of  an  adapter  card,  one  of  a */
/* number  of  fatal  error values may be produced. Bits 12-15 of the EAGLE */
/* SIFINT register contain the error value. These codes are used by the FTK */
/* to distinguish different bring-up errors. An extra error value  is  used */
/* for  the  case when no bring up code is produced within a timeout period */
/* (3 seconds).                                                             */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-40 4.5 Bring-Up Diagnostics - BUD                          */
/*                                                                          */

#define BRING_UP_E_00_INITIAL_TEST              (BYTE) 0x00
#define BRING_UP_E_01_SOFTWARE_CHECKSUM         (BYTE) 0x01
#define BRING_UP_E_02_ADAPTER_RAM               (BYTE) 0x02
#define BRING_UP_E_03_INSTRUCTION_TEST          (BYTE) 0x03
#define BRING_UP_E_04_INTERRUPT_TEST            (BYTE) 0x04
#define BRING_UP_E_05_FRONT_END                 (BYTE) 0x05
#define BRING_UP_E_06_SIF_REGISTERS             (BYTE) 0x06

#define BRING_UP_E_10_TIME_OUT                  (BYTE) 0x10


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_INIT . BYTE value                     */
/*                                                                          */
/* During  an attempt to perform adapter initialization, one of a number of */
/* fatal error values may be produced.  Bits  12-15  of  the  EAGLE  SIFINT */
/* regsiter  contain  the  error  value. These codes are used by the FTK to */
/* distinguish different initialization errors. An  extra  error  value  is */
/* used  for  the  case  when  no  initialization code is produced within a */
/* timeout period (11 seconds).                                             */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-47 4.6 Adapter Initialization                              */
/*                                                                          */

#define INIT_E_01_INIT_BLOCK            (BYTE) 0x01
#define INIT_E_02_INIT_OPTIONS          (BYTE) 0x02
#define INIT_E_03_RX_BURST_SIZE         (BYTE) 0x03
#define INIT_E_04_TX_BURST_SIZE         (BYTE) 0x04
#define INIT_E_05_DMA_THRESHOLD         (BYTE) 0x05
#define INIT_E_06_ODD_SCB_ADDRESS       (BYTE) 0x06
#define INIT_E_07_ODD_SSB_ADDRESS       (BYTE) 0x07
#define INIT_E_08_DIO_PARITY            (BYTE) 0x08
#define INIT_E_09_DMA_TIMEOUT           (BYTE) 0x09
#define INIT_E_0A_DMA_PARITY            (BYTE) 0x0A
#define INIT_E_0B_DMA_BUS               (BYTE) 0x0B
#define INIT_E_0C_DMA_DATA              (BYTE) 0x0C
#define INIT_E_0D_ADAPTER_CHECK         (BYTE) 0x0D
#define INIT_E_0E_NOT_ENOUGH_MEMORY     (BYTE) 0x0E

#define INIT_E_10_TIME_OUT              (BYTE) 0x10


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_AUTO_OPEN . BYTE value                */
/*                                                                          */
/* Auto-open  errors  are  fatal  -  there  is no chance to try to open the */
/* adapter again. The error value is usually  set  to  show  that  an  open */
/* adapter  error has occured. The details of the open error are determined */
/* when the user calls  driver_explain_error  (see  TMS  Open  Error  Codes */
/* below).  There  is  also an extra error value which is used for the case */
/* when no open adapter error code is produced within a timeout period  (40 */
/* seconds).                                                                */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-79 MAC 0003 OPEN command                                   */
/*                                                                          */

#define AUTO_OPEN_E_01_OPEN_ERROR       (BYTE) 0x01
#define AUTO_OPEN_E_80_TIME_OUT         (BYTE) 0x80


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_ADAPTER . BYTE value                  */
/*                                                                          */
/* An  adapter  check  interrupt  causes  an  adapter  check  fatal  error. */
/* Different types of adapter checks are not distinguished by the FTK.      */
/*                                                                          */

#define ADAPTER_E_01_ADAPTER_CHECK      (BYTE) 0x01


/****************************************************************************/
/*                                                                          */
/* Values : ERROR_RECORD - ERROR_TYPE_CS . BYTE value                       */
/*                                                                          */
/* These are possible errors return  from  calling  PCMCIA  Card  Services. */
/* These  errors  can only occur on 16/4 PCMCIA ringnode. Other adapters do */
/* not make calls to  PCMCIA  Card  Services.  To start up  a  16/4  PCMCIA */
/* Ringnode,  the  driver  first  registers with  PCMCIA Card Services as a */
/* client, gropes for the ringnode using Card Services calls, requests  I/O */
/* and  interrupt resources  from Card Services. If any of these operations */
/* fails, the driver will return following  errors.  Note  that  these  are */
/* fatal errors.                                                            */
/*                                                                          */

#define CS_E_01_NO_CARD_SERVICES        (BYTE) 0x01
#define CS_E_02_REGISTER_CLIENT_FAILED  (BYTE) 0x02
#define CS_E_03_REGISTRATION_TIMEOUT    (BYTE) 0x03
#define CS_E_04_NO_MADGE_ADAPTER_FOUND  (BYTE) 0x04
#define CS_E_05_ADAPTER_NOT_FOUND       (BYTE) 0x05
#define CS_E_06_SPECIFIED_SOCKET_IN_USE (BYTE) 0x06
#define CS_E_07_IO_REQUEST_FAILED       (BYTE) 0x07
#define CS_E_08_BAD_IRQ_CHANNEL         (BYTE) 0x08
#define CS_E_09_IRQ_REQUEST_FAILED      (BYTE) 0x09
#define CS_E_0A_REQUEST_CONFIG_FAILED   (BYTE) 0x0A


/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/* Values : TMS Open Error Codes                                            */
/*                                                                          */
/* When an E_01_OPEN_ERROR (either AUTO_OPEN or OPEN) occurs, more  details */
/* of  the  open  adapter  error are available by looking at the open_error */
/* field in the Fastmac status block. This open error is the same  as  that */
/* generated by a TI MAC 0003 OPEN command.                                 */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-79 MAC 0003 OPEN command                                   */
/*                                                                          */

#define TMS_OPEN_FAIL_E_40_OPEN_ADDR    0x4000
#define TMS_OPEN_FAIL_E_02_FAIL_OPEN    0x0200
#define TMS_OPEN_FAIL_E_01_OPEN_OPTS    0x0100

#define TMS_OPEN_PHASE_E_01_LOBE_TEST   0x0010
#define TMS_OPEN_PHASE_E_02_INSERTION   0x0020
#define TMS_OPEN_PHASE_E_03_ADDR_VER    0x0030
#define TMS_OPEN_PHASE_E_04_RING_POLL   0x0040
#define TMS_OPEN_PHASE_E_05_REQ_INIT    0x0050

#define TMS_OPEN_ERR_E_01_FUNC_FAIL     0x0001
#define TMS_OPEN_ERR_E_02_SIGNAL_LOSS   0x0002
#define TMS_OPEN_ERR_E_05_TIMEOUT       0x0005
#define TMS_OPEN_ERR_E_06_RING_FAIL     0x0006
#define TMS_OPEN_ERR_E_07_BEACONING     0x0007
#define TMS_OPEN_ERR_E_08_DUPL_ADDR     0x0008
#define TMS_OPEN_ERR_E_09_REQ_INIT      0x0009
#define TMS_OPEN_ERR_E_0A_REMOVE        0x000A


/*                                                                          */
/*                                                                          */
/************** End of FTK_ERR.H file ***************************************/
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE ADAPTER DEFINITIONS                                             */
/*      =======================                                             */
/*                                                                          */
/*      FTK_ADAP.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for  the  structure  which  is */
/* used  to  maintain  information  on an adapter that is being used by the */
/* FTK.                                                                     */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_ADAP.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_ADAP_H 223


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_ADAPTER                     ADAPTER;


/****************************************************************************/
/*                                                                          */
/* Structure type : ADAPTER                                                 */
/*                                                                          */
/* The adapter structure is used to maintain  all  the  information  for  a */
/* single adapter.  This  includes  information  on  the  Fastmac  for  the */
/* adapter. Most of the fields are filled in from the user supplied adapter */
/* information to driver_prepare_adapter and driver_start_adapter.          */
/*                                                                          */

struct STRUCT_ADAPTER
    {
    void                   (*set_dio_address) (struct STRUCT_ADAPTER*, DWORD);
    WBOOLEAN               (*interrupt_handler) (struct STRUCT_ADAPTER*);
    void                   (*remove_card) (struct STRUCT_ADAPTER*);
    UINT                   adapter_card_bus_type;
    UINT                   adapter_card_type;
    UINT                   adapter_card_revision;
    UINT                   adapter_ram_size;    /* Depends on card type.    */
#ifdef PCMCIA_POINT_ENABLE
    UINT                   socket;              /* Socket passed to point
                                                   enabler. */
    BOOLEAN                drop_int;            /* Flag used to stop a 
                                                   spurious interrupt being
                                                   claimed.                 */
#endif
    WORD                   io_location;
    WORD                   io_range;
    WORD                   interrupt_number;    /* 0 == Polling mode        */
    WBOOLEAN               edge_triggered_ints;
    WORD                   nselout_bits;        /* IRQ select on Smart16    */
    WORD                   dma_channel; 
    UINT                   transfer_mode;       /* DMA/MMIO/PIO             */
    WBOOLEAN               EaglePsDMA;
    WORD                   mc32_config;         /* special config info      */
    NODE_ADDRESS           permanent_address;   /* BIA PROM node address    */
    UINT                   ring_speed;
    WBOOLEAN               speed_detect;        /* Card is capable of detecting ring speed */
    WORD                   max_frame_size;      /* determined by ring speed */
    UINT                   set_ring_speed;      /* Force ring speed to this */
    DWORD                  mmio_base_address;   /* MMIO base address        */
    DWORD                  pci_handle;          /* PCI slot handle.         */

    WBOOLEAN               force_16bit_pio;     /* Force 16 bit PIO.        */
    WBOOLEAN               use_32bit_pio;       /* 32 bit PIO being used.   */
    WBOOLEAN               disable_parity;      /* Disable parity checking? */
    
    WORD                   sif_dat;             /* SIF register IO locations*/
    WORD                   sif_datinc;
    WORD                   sif_adr;
    WORD                   sif_int;
    WORD                   sif_acl;
    WORD                   sif_adr2;
    WORD                   sif_adx;
    WORD                   sif_dmalen;
    WORD                   sif_sdmadat;
    WORD                   sif_sdmaadr;
    WORD                   sif_sdmaadx;     
    BYTE		   atula_ctrl2;		/* Contents of ATULA ctrl 2 */
    WORD                   c46_bits;            /* Bits we must remember in */
                                                /* the AT93C46 control reg. */

    WORD                   seeprom_io;          /* I/O loc of the SEEPROM.  */
    BYTE                   e2sk_mask;           /* hc46 SEEPROM clock       */ 
    BYTE                   e2cs_mask;           /* hc46 SEEPROM chip select */
    BYTE                   e2di_mask;           /* hc46 SEEPROM data in     */
    BYTE                   e2do_mask;           /* hc46 SEEPROM data out    */
    BYTE                   e2do_posn;           /* hc46 SEEPROM data bit posn */
    BYTE                   register_bits_to_remember; /* SEEPROM control register */
    BYTE                   e2_bits_to_set;      /* ATULA EEPROM RSCTRL and.. */
    BYTE                   e2_bits_to_clear;    /* .. CLKDRV defaults */ 
    WORD                   at24_access_method;   /* pnp, pcit or pci2        */
    BYTE                   eeprom_byte_store;  /* PCIT - store 5 bits in at24_in */
    BYTE                   last_data_bit;        /* store data bit for PCIT */
    BYTE                   remembered_bits;    /* hc46 bits */

    WBOOLEAN               set_irq;             /* set IRQ if possible      */
    WBOOLEAN               set_dma;             /* set DMA if possible      */
    SRB_GENERAL            srb_general;         /* SRB for this adapter     */
    WORD                   size_of_srb;         /* size of current SRB      */
    DOWNLOAD_IMAGE *       download_image;      /* ptr Fastmac binary image */

    INITIALIZATION_BLOCK * init_block;          /* ptr Fastmac init block   */
    SRB_HEADER *           srb_dio_addr;        /* addr of SRB in DIO space */
    FASTMAC_STATUS_BLOCK * stb_dio_addr;        /* addr of STB in DIO space */
    WBOOLEAN               interrupts_on;       /* for this adapter         */
    WBOOLEAN               dma_on;              /* for this adapter         */
    WBOOLEAN               doing_pio;           /* TRUE when doing PIO      */
    UINT                   adapter_status;      /* prepared or running      */
    UINT                   srb_status;          /* free or in use           */
    ERROR_RECORD           error_record;        /* error type and value     */
    ERROR_MESSAGE          error_message;       /* error message string     */

    STATUS_INFORMATION *   status_info;         /* ptr adapter status info  */

    void *                 user_information;    /* User's private data.     */

    ADAPTER_HANDLE         adapter_handle;

    DWORD                  dma_test_buf_phys;
    DWORD                  dma_test_buf_virt;

#ifdef FMPLUS

    RX_SLOT *              rx_slot_array[FMPLUS_MAX_RX_SLOTS];  
                                                /* Rx slot DIO addresses    */
    TX_SLOT *              tx_slot_array[FMPLUS_MAX_TX_SLOTS];  
                                                /* Tx slot DIO addresses    */

    void *                 rx_slot_mgmnt;       /* pointer to user slot     */
    void *                 tx_slot_mgmnt;       /* management structures.   */

#else
  
    DWORD                  rx_buffer_phys;     /* RX buffer physical address*/
    DWORD                  rx_buffer_virt;     /* RX buffer virtual address */
    DWORD                  tx_buffer_phys;     /* TX buffer physical address*/
    DWORD                  tx_buffer_virt;     /* TX buffer virtual address */

#endif
    };


/****************************************************************************/
/*                                                                          */
/* Values : ADAPTER - WORD adapter_card_type                                */
/*                                                                          */
/* The following are the different types of adapter cards supported by  the */
/* FTK (and their subtypes).                                                */
/*                                                                          */

#define ADAPTER_CARD_TYPE_16_4_AT       2

#define ADAPTER_CARD_16_4_PC            0
#define ADAPTER_CARD_16_4_MAXY          1
#define ADAPTER_CARD_16_4_AT            2
#define ADAPTER_CARD_16_4_FIBRE         3
#define ADAPTER_CARD_16_4_BRIDGE        4
#define ADAPTER_CARD_16_4_ISA_C         5
#define ADAPTER_CARD_16_4_AT_P_REV      6
#define ADAPTER_CARD_16_4_FIBRE_P       7
#define ADAPTER_CARD_16_4_ISA_C_P       8
#define ADAPTER_CARD_16_4_AT_P          9
#define ADAPTER_CARD_16_4_AT_PNP        10

#define ADAPTER_CARD_TYPE_16_4_MC       3

#define ADAPTER_CARD_TYPE_16_4_MC_32    4

#define ADAPTER_CARD_TYPE_16_4_EISA     5

#define ADAPTER_CARD_16_4_EISA_MK1      1
#define ADAPTER_CARD_16_4_EISA_MK2      2
#define ADAPTER_CARD_16_4_EISA_BRIDGE   3
#define ADAPTER_CARD_16_4_EISA_MK3      4

#define ADAPTER_CARD_TYPE_SMART_16      6
#define ADAPTER_CARD_SMART_16           1

#define ADAPTER_CARD_TYPE_16_4_PCI      7
#define ADAPTER_CARD_16_4_PCI           0

#define ADAPTER_CARD_TYPE_16_4_PCMCIA   8
#define ADAPTER_CARD_16_4_PCMCIA        1

#define ADAPTER_CARD_TYPE_16_4_PNP      9
#define ADAPTER_CARD_PNP                0

#define ADAPTER_CARD_TYPE_16_4_PCIT     10
#define ADAPTER_CARD_16_4_PCIT          0

#define ADAPTER_CARD_TYPE_16_4_PCI2     11
#define ADAPTER_CARD_16_4_PCI2          0

#define ADAPTER_CARD_TYPE_16_4_CARDBUS  12
#define ADAPTER_CARD_16_4_CARDBUS       0

#define ADAPTER_CARD_UNKNOWN            255

/****************************************************************************/
/*                                                                          */
/* Values : ADAPTER - WORD adapter_status                                   */
/*                                                                          */
/* These values are for the different required states of the  adapter  when */
/* using the FTK. The order is important.                                   */
/*                                                                          */

#define ADAPTER_PREPARED_FOR_START      0
#define ADAPTER_RUNNING                 1


/****************************************************************************/
/*                                                                          */
/* Values : ADAPTER - WORD srb_status                                       */
/*                                                                          */
/* These  values  are  for  the  different  required  states  of  the  SRB, */
/* associated with an adapter, when using the FTK.                          */
/*                                                                          */

#define SRB_ANY_STATE           0
#define SRB_FREE                1
#define SRB_NOT_FREE            2


/****************************************************************************/
/*                                                                          */
/* Value : Number of Adapters supported                                     */
/*                                                                          */
/* The FTK supports a specified maximum number  of  adapters.  The  smaller */
/* this  value  is,  the  less memory that is used. This is especially true */
/* when considering system specific parts such  as  the  DOS  example  code */
/* within  this  FTK.  It  uses  the  maximum  number of adapters value for */
/* determining  the  size  of  static  arrays  of  adapter  structures  and */
/* initialization  blocks.  It  also  uses it for determining the number of */
/* interrupt stubs required given that :                                    */
/*                                                                          */
/* NOTE : If using the DOS example system specific code, then  it  must  be */
/* the    case    that    MAX_NUMBER_OF_ADAPTERS    defined   here   equals */
/* MAX_NUMBER_OF_ADAPTERS as defined in SYS_IRQ.ASM.                        */
/*                                                                          */

#define MAX_NUMBER_OF_ADAPTERS  9


#define ISA_IO_LOCATIONS        4
#define MAX_ISA_ADAPATERS       ISA_IO_LOCATIONS

#define MC_IO_LOCATIONS         8
#define MAX_MC_ADAPATERS        MC_IO_LOCATIONS

#define MC32_IO_LOCATIONS       8
#define MAX_MC32_ADAPATERS      MC32_IO_LOCATIONS


/****************************************************************************/
/*                                                                          */
/* Varaibles : adapter_record array                                         */
/*                                                                          */
/* The FTK maintains an array of pointers to the adapter structures used to */
/* maintain information on the different adapters being used. This array is */
/* exported by DRV_INIT.C.                                                  */
/*                                                                          */

extern ADAPTER *        adapter_record[MAX_NUMBER_OF_ADAPTERS];

/****************************************************************************/
/*                                                                          */
/* Macro: FTK_ADAPTER_USER_INFORMATION                                      */
/*                                                                          */
/* A macro to let FTK users get at their private adapter information from   */
/* an adapter handle.                                                       */
/*                                                                          */

#define FTK_ADAPTER_USER_INFORMATION(adapter_handle) \
    (adapter_record[(adapter_handle)]->user_information)    


/*                                                                          */
/*                                                                          */
/************** End of FTK_ADAP.H file **************************************/
/*                                                                          */
/*                                                                          */

/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE ADAPTER CARD DEFINITIONS : EAGLE (TMS 380 2nd GEN)              */
/*      ======================================================              */
/*                                                                          */
/*      FTK_CARD.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions specific to the TI EAGLE  (TMS */
/* 380 2nd generation) chipset and the bring-up, initialization and opening */
/* of  the  adapter.  It also contains the details of the BIA PROM on ATULA */
/* and MC cards.                                                            */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_CARD.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_CARD_H 223


/****************************************************************************/
/*                                                                          */
/* Values : EAGLE SIF REGISTERS                                             */
/*                                                                          */
/* The EAGLE SIF registers are in two groups -  the  normal  SIF  registers */
/* (those  from  the  old TI chipset) and the extended SIF registers (those */
/* particular  to  the  EAGLE).                                             */
/*                                                                          */
/* The definitions for the normal  SIF  registers  are  here  because  they */
/* appear  in  the  same  relative  IO locations for all adapter cards. The */
/* definitions for the extended SIF registers are in the  FTK_<card_type>.H */
/* files.                                                                   */
/*                                                                          */

#define EAGLE_SIFDAT            0               /* DIO data                 */
#define EAGLE_SIFDAT_INC        2               /* DIO data auto-increment  */
#define EAGLE_SIFADR            4               /* DIO address (low)        */
#define EAGLE_SIFINT            6               /* interrupt SIFCMD-SIFSTS  */

/* These definitions are for the case when the SIF registers are mapped     */
/* linearly. Otherwise, they will be at some extended location.             */

#define EAGLE_SIFACL            8
#define EAGLE_SIFADX            12

/* These definitions are for Eagle Pseudo DMA. Notice that they replace the */
/* registers above - this is controlled by SIFACL.                          */

#define EAGLE_SDMADAT           0
#define EAGLE_DMALEN            2
#define EAGLE_SDMAADR           4
#define EAGLE_SDMAADX           6

/****************************************************************************/
/*                                                                          */
/* Value : Number of IO locations for SIF registers                         */
/*                                                                          */
/* The number of SIF registers is only needed for  enabling  and  disabling */
/* ranges  of IO ports. For the ATULA and MC cards the SIF registers are in */
/* 2 pages only using  8  IO  ports.  However,  for  EISA  cards,  the  SIF */
/* registers  are  in a single page of 16 IO ports. Hence, 16 IO ports need */
/* to be enabled whenever accessing SIF registers.                          */
/*                                                                          */

#define SIF_IO_RANGE         16

/****************************************************************************/
/*                                                                          */
/* Value : Number of IO locations for adapter cards                         */
/*                                                                          */
/* The maximum IO range required for  the  register  map  of  any  type  of */
/* adapter card is that used by the EISA card. The ATULA based cards have   */
/* the largest contiguous IO range, however. The EISA range is split into   */
/* two, the upper range only being used during installation.                */
/*                                                                          */

#define MAX_CARD_IO_RANGE         ATULA_IO_RANGE


/****************************************************************************/
/*                                                                          */
/* Values : MAX FRAME SIZES SUPPORTED                                       */
/*                                                                          */
/* Depending on the ring speed (4 Mbit/s or 16  Mbit/s)  different  maximum */
/* frame  sizes  are  supported  as  defined in the ISO standards.  The ISO */
/* standards give the maximum size of the information field to which has to */
/* added the MAC header, SR info fields etc. to  give  real  maximum  token */
/* ring frame size.                                                         */
/*                                                                          */

#define MAC_FRAME_SIZE                          39

#define MIN_FRAME_SIZE                          (256 + MAC_FRAME_SIZE)
#define MAX_FRAME_SIZE_4_MBITS                  (4472 + MAC_FRAME_SIZE)
#define MAX_FRAME_SIZE_16_MBITS                 (17800 + MAC_FRAME_SIZE)


/****************************************************************************/
/*                                                                          */
/* Values : EAGLE ADAPTER CONTROL (SIFACL) REGISTER BITS                    */
/*                                                                          */
/* The bits in the EAGLE extended SIF register EAGLE_SIFACL can be used for */
/* general controlling of the adapter card.                                 */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-18 4.3.1 SIFACL - SIF Adapter Control Register             */
/*                                                                          */

#define EAGLE_SIFACL_SWHLDA         0x0800      /* for EAGLE pseudo-DMA     */
#define EAGLE_SIFACL_SWDDIR         0x0400      /* data transfer direction  */
#define EAGLE_SIFACL_SWHRQ          0x0200      /* DMA pending              */
#define EAGLE_SIFACL_PSDMAEN        0x0100      /* for EAGLE pseudo-DMA     */
#define EAGLE_SIFACL_ARESET         0x0080      /* adapter reset            */
#define EAGLE_SIFACL_CPHALT         0x0040      /* halt EAGLE               */
#define EAGLE_SIFACL_BOOT           0x0020      /* bootstrap                */
#define EAGLE_SIFACL_RESERVED1      0x0010      /* reserved                 */
#define EAGLE_SIFACL_SINTEN         0x0008      /* system interrupt enable  */
#define EAGLE_SIFACL_PARITY         0x0004      /* adapter parity enable    */
#define EAGLE_SIFACL_INPUT0         0x0002      /* reserved                 */
#define EAGLE_SIFACL_RESERVED2      0x0001      /* reserved                 */


/****************************************************************************/
/*                                                                          */
/* Values : DIO LOCATIONS                                                   */
/*                                                                          */
/* When initializing an adapter the initialization block must be downloaded */
/* to location 0x00010A00L in DIO  space.                                   */
/*                                                                          */
/* The  ring  speed,  from  which the maximum frame size is deduced, can be */
/* determined by the value in the ring speed register at DIO address 0x0142 */
/* in the EAGLE DATA page 0x00010000.                                       */
/*                                                                          */

#define DIO_LOCATION_INIT_BLOCK                 0x00010A00L

#define DIO_LOCATION_EAGLE_DATA_PAGE            0x00010000L
#define DIO_LOCATION_RING_SPEED_REG             0x0142

#define RING_SPEED_REG_4_MBITS_MASK             0x0400

#define DIO_LOCATION_EXT_DMA_ADDR               0x010E
#define DIO_LOCATION_DMA_ADDR                   0x0110

#define DIO_LOCATION_DMA_CONTROL                0x100A

/****************************************************************************/
/*                                                                          */
/* Values : EAGLE BRING UP INTERRUPT REGISTER VALUES                        */
/*                                                                          */
/* The code produced at the SIFSTS part of the SIF  interrupt  register  at */
/* bring up time indicates the success or failure  of  the  bring  up.  The */
/* success  or  failure  is  determined  by  looking  at the top nibble. On */
/* success, the INITIALIZE bit is set. On failure, the TEST and ERROR  bits */
/* are set and the error code is in the bottom nibble.                      */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-40 4.5 Bring-Up Diagnostics - BUD                          */
/*                                                                          */

#define EAGLE_SIFINT_SYSTEM             (UINT) 0x0080

#define EAGLE_BRING_UP_TOP_NIBBLE       (BYTE) 0xF0
#define EAGLE_BRING_UP_BOTTOM_NIBBLE    (BYTE) 0x0F

#define EAGLE_BRING_UP_SUCCESS          (BYTE) 0x40
#define EAGLE_BRING_UP_FAILURE          (BYTE) 0x30


/****************************************************************************/
/*                                                                          */
/* Values : EAGLE INITIALIZATION INTERRUPT REGISTER VALUES                  */
/*                                                                          */
/* The  code  0x9080  is  output  to the SIF interrupt register in order to */
/* start the initialization process. The code produced at the  SIFSTS  part */
/* of  the  SIF  interrupt  register  at  initialization time indicates the */
/* success or failure of the initialization.  On  success, the  INITIALIZE, */
/* TEST  and  ERROR bits are all zero. On failure, the ERROR bit is set and */
/* the error code is in the bottom nibble.                                  */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-46 4.6.2 Writing The Initialization Block                  */
/*                                                                          */

#define EAGLE_INIT_START_CODE           (WORD) 0x9080

#define EAGLE_INIT_TOP_NIBBLE           (BYTE) 0xF0
#define EAGLE_INIT_BOTTOM_NIBBLE        (BYTE) 0x0F

#define EAGLE_INIT_SUCCESS_MASK         (BYTE) 0x70
#define EAGLE_INIT_FAILURE_MASK         (BYTE) 0x10


/****************************************************************************/
/*                                                                          */
/* Values : SCB and SSB test patterns                                       */
/*                                                                          */
/* As a result of initialization, certain test patterns should be  left  in */
/* the SSB and SCB as pointed to by the TI initialization parameters.       */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-46 4.6.2 Writing The Initialization Block                  */
/*                                                                          */

#define SSB_TEST_PATTERN_LENGTH    8
#define SSB_TEST_PATTERN_DATA      {0xFF,0xFF,0xD1,0xD7,0xC5,0xD9,0xC3,0xD4}

#define SCB_TEST_PATTERN_LENGTH    6
#define SCB_TEST_PATTERN_DATA      {0x00,0x00,0xC1,0xE2,0xD4,0x8B}


/****************************************************************************/
/*                                                                          */
/* Value : EAGLE ARB FREE CODE - written to the EAGLE interrupt register to */
/*         indicate that the ARB is now free for use by the adapter.   This */
/*         is of most use at start time when  combined  with  the  DELAY_RX */
/*         FEATURE FLAG. This code can be used to enable receives.          */

#define EAGLE_ARB_FREE_CODE        0x90FF


/****************************************************************************/
/*                                                                          */
/* Values : EAGLE OPENING ERRORS                                            */
/*                                                                          */
/* On opening the adapter, the  success  or  failure  is  recorded  in  the */
/* open_error  field of the Fastmac status parameter block. The bottom byte */
/* has the error value on failure. On success, the word is clear. The  open */
/* error is that which would be given by a MAC 0003 OPEN command.           */
/*                                                                          */
/* REFERENCE : The TMS 380 Second-Generation Token_Ring User's Guide        */
/*             by Texas Instruments                                         */
/*             4-80 MAC 0003 OPEN command                                   */
/*                                                                          */

#define EAGLE_OPEN_ERROR_BOTTOM_BYTE    0x00FF

#define EAGLE_OPEN_ERROR_SUCCESS        0x0000


/****************************************************************************/
/*                                                                          */
/* Values : BIA PROM FORMAT                                                 */
/*                                                                          */
/* The  BIA  PROM  is  accessed in 2 pages.  With CTRLn_SIFSEL = 0 (n=7 for */
/* ATULA cards, n=0 for MC cards) or CTRL1_NRESET = 0, the  bit  CTRLn_PAGE */
/* selects  the different pages. With CTRLn_PAGE = 0, the id and board type */
/* are available; with CTRLn_PAGE = 1, the node address is available.       */
/*                                                                          */

#define BIA_PROM_ID_BYTE                0
#define BIA_PROM_ADAPTER_BYTE           1
#define BIA_PROM_REVISION_BYTE          2
#define BIA_PROM_FEATURES_BYTE          3
#define BIA_PROM_HWF2                   4
#define BIA_PROM_HWF3                   5

#define BIA_PROM_NODE_ADDRESS           1

/****************************************************************************/
/*                                                                          */
/* Values : Bits defined in the HW flags                                    */
/*                                                                          */
/* HWF2                                                                     */
#define  C30   0x1

/* HWF3                                                                     */
#define  RSPEED_DETECT  0x80


/****************************************************************************/
/*                                                                          */
/* Values : BIA PROM ADAPTER CARD TYPES                                     */
/*                                                                          */
/* The second byte in the first page of the BIA PROM  contains  an  adapter */
/* card type.                                                               */
/*                                                                          */

#define BIA_PROM_TYPE_16_4_AT           ((BYTE) 0x04)
#define BIA_PROM_TYPE_16_4_MC           ((BYTE) 0x08)
#define BIA_PROM_TYPE_16_4_PC           ((BYTE) 0x0B)
#define BIA_PROM_TYPE_16_4_MAXY         ((BYTE) 0x0C)
#define BIA_PROM_TYPE_16_4_MC_32        ((BYTE) 0x0D)
#define BIA_PROM_TYPE_16_4_AT_P         ((BYTE) 0x0E)

#define MAX_ADAPTER_CARD_AT_REV         6


/****************************************************************************/
/*                                                                          */
/* Values : BIA PROM FEATURES BYTE MASKS (and related values)               */
/*                                                                          */
/* The features byte in the BIA indicates certain hardware characteristics  */
/* of AT/P cards (and later cards).                                         */
/* Note that you can multiply the masked DRAM field by the DRAM_MULT value  */
/* to get the amount of RAM on the card (don't shift the field).            */
/*                                                                          */

#define BIA_PROM_FEATURE_SRA_MASK       ((BYTE) 0x01)
#define BIA_PROM_FEATURE_DRAM_MASK      ((BYTE) 0x3E)
#define BIA_PROM_FEATURE_CLKDIV_MASK    ((BYTE) 0x40)

#define DRAM_MULT                       64


/****************************************************************************/
/*                                                                          */
/* Values : MADGE ADAPTER CARD NODE ADDRESSES                               */
/*                                                                          */
/* The  first 3 bytes of the permanent node address for Madge adapter cards */
/* must have certain values. All Madge  node  addresses  are  of  the  form */
/* 0000F6xxxxxx.                                                            */
/*                                                                          */

#define MADGE_NODE_BYTE_0               ((BYTE) 0x00)
#define MADGE_NODE_BYTE_1               ((BYTE) 0x00)
#define MADGE_NODE_BYTE_2               ((BYTE) 0xF6)


/*                                                                          */
/*                                                                          */
/************** End of FTK_CARD.H file **************************************/
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE DOWNLOAD DEFINITIONS                                            */
/*      ========================                                            */
/*                                                                          */
/*      FTK_DOWN.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for  the  structure  which  is */
/* used  for downloading information on to an adapter that is being used by */
/* the FTK.                                                                 */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Downloading The Code                                       */
/*                                                                          */
/* IMPORTANT : All structures used within the FTK  need  to  be  packed  in */
/* order to work correctly. This means sizeof(STRUCTURE) will give the real */
/* size  in bytes, and if a structure contains sub-structures there will be */
/* no spaces between the sub-structures.                                    */
/*                                                                          */
/****************************************************************************/

#ifdef FTK_PACKING_ON
#pragma FTK_PACKING_ON
#else
#pragma pack(1)
#endif

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_DOWN.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_DOWN_H 223


/****************************************************************************/
/*                                                                          */
/* TYPEDEFs for all structures defined within this header file :            */
/*                                                                          */

typedef struct STRUCT_DOWNLOAD_RECORD             DOWNLOAD_RECORD;

/****************************************************************************/
/*                                                                          */
/* Structure type : DOWNLOAD_RECORD                                         */
/*                                                                          */
/* This  structure  gives the format of the records that define how data is */
/* downloaded into adapter DIO space. There are only 3 types of record that */
/* are used on EAGLEs when downloading. These are MODULE - a special record */
/* that starts a download image, DATA_32 - null terminated  data  with  DIO */
/* start  address  location, and FILL_32 - pattern with length to be filled */
/* in starting at given DIO location.                                       */
/*                                                                          */
/* Each download record is an array of words in Intel  byte  ordering  (ie. */
/* least significant byte first).                                           */
/*                                                                          */
/* REFERENCE : The Madge Smart SRB Interface                                */
/*             - Downloading The Code                                       */
/*                                                                          */

struct STRUCT_DOWNLOAD_RECORD
    {
    WORD        length;                         /* length of entire record  */
    WORD        type;                           /* type of record           */
    union
        {
        struct                                  /* MODULE                   */
            {
            WORD        reserved_1;
            WORD        download_features;
            WORD        reserved_2;
            WORD        reserved_3;
            WORD        reserved_4;
            BYTE        name[1];                /* '\0' ending module name  */
            } module;

        struct                                  /* DATA_32                  */
            {
            WORD        dio_addr_l;            /* 32 bit EAGLE address     */
            WORD        dio_addr_h;             /* 32 bit EAGLE address     */
	/*    DWORD       dio_addr;     */          /* 32 bit EAGLE address     */
            WORD        word_count;             /* number of words          */
            WORD        data[1];                /* null terminated data     */
            } data_32;

        struct                                  /* FILL_32                  */
            {
            WORD        dio_addr_l;         /* 32 bit EAGLE address     */
            WORD        dio_addr_h;         /* 32 bit EAGLE address     */
/*	    DWORD       dio_addr;        */       /* 32 bit EAGLE address     */
            WORD        word_count;             /* number of words          */
            WORD        pattern;                /* value to fill            */
            } fill_32;

        } body;

    };

/****************************************************************************/
/*                                                                          */
/* Values : DOWNLOAD_RECORD - WORD type                                     */
/*                                                                          */
/* These values are for the different types of download record.             */
/*                                                                          */

#define DOWNLOAD_RECORD_TYPE_DATA_32            0x04
#define DOWNLOAD_RECORD_TYPE_FILL_32            0x05
#define DOWNLOAD_RECORD_TYPE_MODULE             0x12


/****************************************************************************/
/*                                                                          */
/* Values : DOWNLOAD_RECORD - module. WORD download_features                */
/*                                                                          */
/* These  specify  some features of the module to be downloaded that may be */
/* checked for.                                                             */
/*                                                                          */

#define DOWNLOAD_FASTMAC_INTERFACE      0x0011
#define DOWNLOAD_BMIC_SUPPORT           0x4000  /* required for EISA cards  */

#ifdef FTK_PACKING_OFF
#pragma FTK_PACKING_OFF
#else
#pragma pack(0)
#endif

/*                                                                          */
/*                                                                          */
/************** End of FTK_DOWN.H file **************************************/
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (PCI CARDS)                      */
/*      ==============================================                      */
/*                                                                          */
/*      FTK_PCI2.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1994                              */
/*      Developed by PRR                                                    */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming Madge Smart    */
/* 16/4 PCI (BM) and Madge Smart 16/4 CardBus adapter cards, based on the   */
/* Madge bus interface ASICs.						    */
/*                                                                          */
/* The SIF registers are WORD aligned and start at offset 0x10 from the IO  */
/* space                                                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_PCI2.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_PCI2_H 223

/***************************************************************************/
/*                                                                         */
/* PCI-2 IO Map                                                            */
/* Offsets of register locations are from the start of the card's IO space.*/
/*                                                                         */

#define  PCI2_INTERRUPT_STATUS	0x01	   /* One byte. */
#define  PCI2_DMAING		 0x01	   /* Bit 0 - Dma in progress */
#define  PCI2_SINTR		 0x02	   /* Bit 1 - SIF int */
#define  PCI2_SWINT		 0x04	   /* Bit 2 - Software int */
#define  PCI2_PCI_INT		 0x80	   /* Bit 8 - Catastrophic error */

#define  PCI2_INTERRUPT_CONTROL	0x02	   /* One byte. */
#define  PCI2_SINTEN	         0x02	   /* Bit 1 - SIF int enable */
#define  PCI2_SWINTEN		 0x04	   /* Bit 2 - S/w int enable */
#define  PCI2_PCI_ERR_EN	 0x80	   /* Bit 9 - Catastrophic err en */

#define  PCI2_RESET		0x04	   /* One byte. */
#define  PCI2_CHIP_NRES		 0x01	   /* Bit 0 - Reset chip if zero */
#define  PCI2_FIFO_NRES		 0x02	   /* Bit 1 - Fifo reset if zero */
#define  PCI2_SIF_NRES		 0x04	   /* Bit 2 - SIF reset if zero */
#define  CBUS_POWERDOWN		 0x08	   /* Bit 3 - powerdown zero */

#define  PCI2_SEEPROM_CONTROL	0x07	   /* One byte. */

#define  PCI2_SIF_OFFSET	0x10

#define  PCI2_FIFO_THRESHOLD	0x21	   /* One byte. */

#define  CBUS_FN_R_EVENT	0x30	   /* Dword */
#define  CBUS_FN_R_EVENT_MASK	0x34	   /* Word */
#define  CBUS_FN_R_INTERRUPT	 0x8000    /* Bit 15 - interrupt enable */
#define  CBUS_FN_R_PRESENT_STATE 0x38	   /* Dword */
#define  CBUS_FN_R_FORCE_EVENT  0x3C	   /* Dword */

/* Locations 0x22 onwards are for ASIC debugging only */
#define  PCI2_IO_RANGE              0x36


/****************************************************************************/
/*                                                                          */
/* Usefule locations in the EEPROM                                          */
/*                                                                          */
/*                                                                          */
#define  PCI2_EEPROM_BIA_WORD0   9
#define  PCI2_EEPROM_BIA_WORD1   10
#define  PCI2_EEPROM_BIA_WORD2   11
#define  PCI2_EEPROM_RING_SPEED  12
#define  PCI2_EEPROM_RAM_SIZE    13
#define  PCI2_HWF1               14
#define  PCI2_HWF2               15


/****************************************************************************/
/*                                                                          */
/* Useful values in the EEPROM                                              */
/*                                                                          */
#define  PCI2_EEPROM_4MBITS   1
#define  PCI2_EEPROM_16MBITS  0


#define  PCI2_BUS_MASTER_ONLY    4
#define  PCI2_HW2_431_READY      0x10


/****************************************************************************/
/*                                                                          */
/* Useful locations in the PCI config space                                 */
/*                                                                          */
/*                                                                          */
#define  PCI_CONFIG_COMMAND             0x4
#define  PCI_CONFIG_STATUS    		0x6
#define  PCI_CONFIG_CACHE_LINE_SIZE	0xC

/****************************************************************************/
/*                                                                          */
/* The BUS Master DMA Enable bit in the CONFIG_COMMAND register             */

#ifndef PCI_CONFIG_BUS_MASTER_ENABLE
#define  PCI_CONFIG_BUS_MASTER_ENABLE  0x0004
#endif

#define  PCI_CONFIG_MEM_WR_INV_EN      0x0010
#define  PCI_CONFIG_PARITY_ENABLE      0x0040


/****************************************************************************/
/*                                                                          */
/* Bits in the status register                                              */
/*                                                                          */

#define PCI_STATUS_PERR            (WORD) 0x8000
#define PCI_STATUS_SERR            (WORD) 0x4000
#define PCI_STATUS_MASTER_ABORT    (WORD) 0x2000
#define PCI_STATUS_DMA_ABORT       (WORD) 0x1000
#define PCI_STATUS_TARGET_ABORT    (WORD) 0x0800
#define PCI_STATUS_PARITY          (WORD) 0x0100

#define PCI_STATUS_ERROR_MASK      (WORD) 0xf900

/****************************************************************************/
/*                                                                          */
/* Values required by Card Services RequestConfiguration for CardBus.       */
/*                                                                          */

#define CARDBUS_VCC			(BYTE) 33
#define CARDBUS_VPP1			(BYTE) 0
#define CARDBUS_VPP2			(BYTE) 0
#define CARDBUS_CONFIG_BASE		(DWORD) 0x00000081
#define CARDBUS_STATUS_REG_SETTING	(BYTE) 0
#define CARDBUS_PIN_REG_SETTING		(BYTE) 0
#define CARDBUS_COPY_REG_SETTING	(BYTE) 0
#define CARDBUS_OPTION_REG_SETTING	(BYTE) 1
#define CARDBUS_REGISTER_PRESENT	(BYTE) 0

#define CARDBUS_RAM_SIZE		512

/*                                                                          */
/*                                                                          */
/************** End of FTK_PCI2.H file ***************************************/
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (PCI CARDS)                      */
/*      ===================================================                 */
/*                                                                          */
/*      FTK_PCIT.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1994                              */
/*      Developed by PRR                                                    */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming Madge Smart    */
/* 16/4 PCI (T) adapter cards, ie based on the Ti PCI bus interface ASIC    */
/* The only IO registers are the SIF registers, all other control is        */
/* through PCI config space                                                 */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_PCIT.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_PCIT_H 223


/****************************************************************************/
/*                                                                          */
/* Values : PCI REGISTER MAP                                                */
/*                                                                          */
/* The Madge PCI Ringnode uses the following register layout.               */
/* N.B. The SIF registers are mapped linearly, with no overlaying.          */
/*                                                                          */
#define  PCIT_HANDSHAKE       0x100C
#define  PCIT_HANDSHAKE       0x100C


/****************************************************************************/
/*                                                                          */
/* Useful locations in the PCI config space                                 */
/*                                                                          */
/*                                                                          */
#define  EEPROM_OFFSET        0x48
#define  MISC_CONT_REG        0x40
#define  PCI_CONFIG_COMMAND   0x4
#define  CACHE_LINE_SIZE      0xC

/****************************************************************************/
/*                                                                          */
/* The BUS Master DMA Enable bit in the CONFIG_COMMAND register             */
#define  PCI_CONFIG_BUS_MASTER_ENABLE  0x0004
#define  PCI_CONFIG_IO_ENABLE          0x2
#define  PCI_CONFIG_MEM_ENABLE         0x1

/****************************************************************************/
/*                                                                          */
/* Useful locations in the EEPROM                                           */
/*                                                                          */
/*                                                                          */

#define PCIT_EEPROM_BIA_WORD0    9
#define PCIT_EEPROM_BIA_WORD1    10
#define PCIT_EEPROM_BIA_WORD2    11
#define PCIT_EEPROM_RING_SPEED   12
#define PCIT_EEPROM_RAM_SIZE     13
#define PCIT_EEPROM_HWF2         15

#define NSEL_4MBITS              3
#define NSEL_16MBITS             1

/****************************************************************************/
/*                                                                          */
/* Values in the EEPROM                                                     */
/*                                                                          */
#define  PCIT_EEPROM_4MBITS   1
#define  PCIT_EEPROM_16MBITS  0

#define  PCIT_BROKEN_DMA      0x20

/*
*  Value passed to the adapter in the mc32 byte to tell it to use the FMPLUS
*  code which supports broken DMA.
*/
#define  TRN_PCIT_BROKEN_DMA  0x200

/*                                                                          */
/*                                                                          */
/************** End of FTK_PCIT.H file ***************************************/
/*                                                                          */
/*                                                                          */

/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (PCI CARDS)                      */
/*      ===================================================                 */
/*                                                                          */
/*      FTK_PCI.H : Part of the FASTMAC TOOL-KIT (FTK)                      */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1994                              */
/*      Developed by PRR                                                    */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for programming Madge Smart    */
/* 16/4 PCI                                                                 */
/* adapter cards.  These adapter cards have a couple of control  registers, */
/* in addition to the SIF registers.  ALL bits in ALL control registers are */
/* defined by Madge Networks Ltd                                            */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_PCI.H belongs :                  */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_PCI_H 223


/****************************************************************************/
/*                                                                          */
/* Values : PCI REGISTER MAP                                                */
/*                                                                          */
/* The Madge PCI Ringnode uses the following register layout.               */
/* N.B. The SIF registers are mapped linearly, with no overlaying.          */
/*                                                                          */

#define PCI_GENERAL_CONTROL_REG         0
#define PCI_INT_MASK_REG                4
#define PCI_SEEPROM_CONTROL_REG         8
#define PCI_FIRST_SIF_REGISTER          0x20

#define PCI_IO_RANGE                    256

#define PCI1_SRESET		    1 /* Bit 0 of General Control Register  */
#define PCI1_RSPEED_4MBPS	0x200 /* Bit 9 of General Control Register  */
#define PCI1_RSPEED_VALID	0x400 /* Bit 10 of General Control Register */

#define PCI1_BIA_CLK           0x0001 /* Bit 0 of SEEPROM control word.     */
#define PCI1_BIA_DOUT          0x0002 /* Bit 1 of SEEPROM control word.     */
#define PCI1_BIA_ENA           0x0004 /* Bit 2 of SEEPROM control word.     */
#define PCI1_BIA_DIN           0x0008 /* Bit 3 of SEEPROM control word.     */

#define PCI1_ENABLE_MMIO       0x0080 /* MC32 config value to enable MMIO.  */

/****************************************************************************/
/*                                                                          */
/* Values : PCI SIF REGISTERS                                               */
/*                                                                          */
/* The EAGLE SIF registers are in two groups -  the  normal  SIF  registers */
/* (those  from  the  old TI chipset) and the extended SIF registers (those */
/* particular  to  the  EAGLE).                                             */
/*                                                                          */
/* The definitions for the normal  SIF  registers  are  here  because  they */
/* appear  in  the  same  relative  IO locations for all adapter cards.     */
/*                                                                          */

#define PCI_SIFDAT            0               /* DIO data                 */
#define PCI_SIFDAT_INC        4               /* DIO data auto-increment  */
#define PCI_SIFADR            8               /* DIO address (low)        */
#define PCI_SIFINT            12              /* interrupt SIFCMD-SIFSTS  */

/* These definitions are for the case when the SIF registers are mapped     */
/* linearly. Otherwise, they will be at some extended location.             */

#define PCI_SIFACL            16
#define PCI_SIFADX            24

/* These definitions are for Eagle Pseudo DMA. Notice that they replace the */
/* registers above - this is controlled by SIFACL.                          */

#define PCI_SDMADAT           0
#define PCI_DMALEN            4
#define PCI_SDMAADR           8
#define PCI_SDMAADX           12


/****************************************************************************/
/*                                                                          */
/* Value : Number of IO locations for SIF registers                         */
/*                                                                          */
/* The number of SIF registers is only needed for  enabling  and  disabling */
/* ranges  of IO ports. For the ATULA and MC cards the SIF registers are in */
/* 2 pages only using  8  IO  ports.  However,  for  EISA  cards,  the  SIF */
/* registers  are  in a single page of 16 IO ports. Hence, 16 IO ports need */
/* to be enabled whenever accessing SIF registers.                          */
/*                                                                          */

#define PCI_SIF_IO_RANGE         32

/****************************************************************************/
/*                                                                          */
/* Values : Locations of data in the serial EEPROM (in words)               */
/*                                                                          */
/*                                                                          */

#define PCI_EEPROM_BIA_WORD0    0
#define PCI_EEPROM_BIA_WORD1    1
#define PCI_EEPROM_BIA_WORD2    2
#define PCI_EEPROM_RING_SPEED   3
#define PCI_EEPROM_RAM_SIZE     4


/****************************************************************************/
/*                                                                          */
/* Values : Ring speed values stored in the serial EEPROM                   */
/*                                                                          */
/*                                                                          */

#define PCI_EEPROM_4MBS         1
#define PCI_EEPROM_16MBPS       0

/*                                                                          */
/*                                                                          */
/************** End of FTK_PCI.H file ***************************************/
/*                                                                          */
/*                                                                          */

/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (serial EEPROMs)                 */
/*      ===================================================                 */
/*                                                                          */
/*      FTK_EEPR.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1996                         */
/*      Developed by JEK                                                    */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions for the common serial EEPROM   */
/* access routines for MADGE adapter cards.                                 */
/* Each  adapter  card has a number of control and status                   */
/* registers. ALL bits in ALL registers are defined by Madge Networks  Ltd, */
/* however  only  a  restricted number are defined below as used within the */
/* FTK.  All other bits must NOT be changed and no support will be  offered */
/* for  any  application  that  does so or uses the defined bits in any way */
/* different to the FTK.                                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_EEPR.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_EEPR_H 223

/***************************************************************************/
/*                                                                         */
/*  Only compile necessary code - ie either at24 or hc46 depending on      */
/*  which sort of EEPROM used by card. Cards that do not contain EEPROM    */
/*  eg MC cards will have neither sets of routines compiled.               */

#ifndef FTK_NO_PCI
#define USE_EEPROM_HC46
#endif

#ifndef FTK_NO_PCMCIA
#define USE_EEPROM_HC46
#endif


#ifndef FTK_NO_PNP
#define USE_EEPROM_AT24
#endif

#ifndef FTK_NO_PCIT
#define USE_EEPROM_AT24
#endif

#ifndef FTK_NO_PCI2
#define USE_EEPROM_AT24
#endif

#ifndef FTK_NO_ATULA
#define USE_EEPROM_AT24
#define USE_EEPROM_HC46
#endif


/* Card types that use the AT24 serial EEPROM - used to distinguish PCIT */

#define AT24_PNP                0
#define AT24_PCI_T              1
#define AT24_PCI_A              2 /* PCI2 */

/****************************************************************************/
/*                                                                          */
/* AT24 EEPROM commands and definitions                                     */
/*                                                                          */
#define  AT24_WRITE_CMD    0xA0
#define  AT24_READ_CMD     0xA1

#define PNP_EEDO            0x0002  /* bit posn of IO_DATA */
#define PNP_EEDEN           0x0004  /* bit posn of IO_ENABLE */
#define PNP_SSK             0x0001  /* bit posn of IO_CLOCK */

/*                                                                          */
/*                                                                          */
/************** End of FTK_EEPR.H file **************************************/
/*                                                                          */
/*                                                                          */

/****************************************************************************/
/****************************************************************************/
/*                                                                          */
/*      THE MADGE ADAPTER CARD DEFINITIONS (EISA CARDS)                     */
/*      ===============================================                     */
/*                                                                          */
/*      FTK_EISA.H : Part of the FASTMAC TOOL-KIT (FTK)                     */
/*                                                                          */
/*      Copyright (c) Madge Networks Ltd. 1991-1994                         */
/*      Developed by MF                                                     */
/*      CONFIDENTIAL                                                        */
/*                                                                          */
/*                                                                          */
/****************************************************************************/
/*                                                                          */
/* This header file contains the definitions  for  programming  Madge  EISA */
/* adapter  cards.   Each  adapter  card has a number of control and status */
/* registers. ALL bits in ALL registers are defined by Madge Networks  Ltd, */
/* however  only  a  restricted number are defined below as used within the */
/* FTK.  All other bits must NOT be changed and no support will be  offered */
/* for  any  application  that  does so or uses the defined bits in any way */
/* different to the FTK.                                                    */
/*                                                                          */
/****************************************************************************/

/****************************************************************************/
/*                                                                          */
/* VERSION_NUMBER of FTK to which this FTK_EISA.H belongs :                 */
/*                                                                          */

#define FTK_VERSION_NUMBER_FTK_EISA_H 223


/****************************************************************************/
/*                                                                          */
/* Values : EISA REGISTER MAP                                               */
/*                                                                          */
/* Madge EISA cards have the following register map.  All SIF registers are */
/* always visible.                                                          */
/*                                                                          */
/* NB. The IO registers are actually in two groups 0x0000-0x000F  (the  SIF */
/* registers) and 0x0C80-0x00C9F (the control type registers).              */
/*                                                                          */

#define EISA_IO_RANGE                   16

#define EISA_FIRST_SIF_REGISTER         0x0000

#define EISA_IO_RANGE2                  32

#define EISA_IO_RANGE2_BASE             0x0C80

#define EISA_ID_REGISTER_0              0x0C80
#define EISA_ID_REGISTER_1              0x0C82
#define EISA_CONTROLX_REGISTER          0x0C84

#define EISA_BMIC_REGISTER_3            0x0C90


/****************************************************************************/
/*                                                                          */
/* Values : MC POS_REGISTER_0                                               */
/*                                                                          */
/* These  are the required contents of the EISA ID registers for Madge 16/4 */
/* EISA mk1 and mk2 cards.                                                  */
/*                                                                          */

#define EISA_ID0_MDG_CODE           0x8734   /* 'MDG' encoded               */

#define EISA_ID1_MK1_MDG_CODE       0x0100   /* '0001' encoded              */
#define EISA_ID1_MK2_MDG_CODE       0x0200   /* '0002' encoded              */
#define EISA_ID1_BRIDGE_MDG_CODE    0x0300   /* '0003' encoded              */
#define EISA_ID1_MK3_MDG_CODE       0x0400   /* '0004' encoded              */


/****************************************************************************/
/*                                                                          */
/* Values : EISA CONTROLX_REGISTER                                          */
/*                                                                          */
/* These are the bit definitions for the expansion board  control  register */
/* on EISA cards.                                                           */
/*                                                                          */

#define EISA_CTRLX_CDEN     ((BYTE) 0x01)    /* card enabled                */


/****************************************************************************/
/*                                                                          */
/* Values : EISA BMIC_REGISTER_3                                            */
/*                                                                          */
/* These are the bit definitions for BMIC register 3 on EISA cards.         */
/*                                                                          */

#define EISA_BMIC3_IRQSEL   ((BYTE) 0x0F)    /* interrupt number (4 bits)   */
#define EISA_BMIC3_EDGE     ((BYTE) 0x10)    /* edge\level triggered ints   */
#define EISA_BMIC3_SPD      ((BYTE) 0x80)    /* any speed selected          */


/****************************************************************************/
/*                                                                          */
/* Values : EISA EXTENDED EAGLE SIF REGISTERS                               */
/*                                                                          */
/* The  EAGLE  SIF  registers  are in two groups - the normal SIF registers */
/* (those from the old TI chipset) and the extended  SIF  registers  (those */
/* particular to the EAGLE).  For Madge EISA adapter cards, both normal and */
/* extended SIF registers are always accessible.                            */
/*                                                                          */
/* The definitions for the normal SIF registers are  in FTK_CARD.H  because */
/* they appear in the same relative IO locations for all adapter cards. The */
/* extended  SIF  registers  are  here  because  they  appear  at different */
/* relative  IO  locations for different types of adapter cards.            */
/*                                                                          */

#define EISA_EAGLE_SIFACL       8               /* adapter control          */
#define EISA_EAGLE_SIFADR_2     10              /* copy of SIFADR           */
#define EISA_EAGLE_SIFADX       12              /* DIO address (high)       */
#define EISA_EAGLE_DMALEN       14              /* DMA length               */


/****************************************************************************/
/*                                                                          */
/* Values : VRAM enable on EIDA Mk3                                         */
/*                                                                          */

#define DIO_LOCATION_EISA_VRAM_ENABLE   ((DWORD) 0xC0000L)
#define EISA_VRAM_ENABLE_WORD           ((WORD)  0xFFFF)

/*                                                                          */
/*                                                                          */
/************** End of FTK_EISA.H file **************************************/
/*                                                                          */
/*                                                                          */
