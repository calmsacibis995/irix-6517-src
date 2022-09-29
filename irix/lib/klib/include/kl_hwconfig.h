#ident "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/include/RCS/kl_hwconfig.h,v 1.5 1999/10/19 15:40:09 lucc Exp $"

#define IS_EVEREST ((K_IP == 19) || (K_IP == 21) || (K_IP == 25)) 

#define EV_CFGINFO_ADDR 0xa800000000002000

/* Private data structure that contains much of the information contained
 * in the inventory_s struct (except for the next field).
 */
typedef struct invent_data_s {
	int 				inv_class;
	int 				inv_type;
	major_t 			inv_controller;
	minor_t 			inv_unit;
	int 				inv_state;
	int					inv_vhndl; 	/* handle of vertex w/ _invent info */
} invent_data_t;

/* Every discrete part of a system is considered a component. The 
 * hw_component_s struct is a generic structure that contains information
 * common to most types of hardware components. It contains next and prev 
 * pointers so that components can (optionally) be put onto doubly linked 
 * lists. It may also contain a pointer to a component-specific private 
 * data structure.
 */
typedef struct hw_component_s {

	/* The order of the first six (6) fields MUST NOT be changed as they
	 * match the fields in a htnode_s struct.
	 */
	struct hw_component_s	*c_next;		 /* Must be first */
	struct hw_component_s	*c_prev;		 /* Must be second */
	struct hw_component_s   *c_parent;		 /* parent to this components */
	struct hw_component_s   *c_cmplist;		 /* linked list of sub-components */
	int                      c_seq;			 /* sequence number */
	int						 c_level;		 /* level in hwconfig tree */
	int                      c_rec_key;		 /* DB record key */
	int                      c_parent_key;	 /* DB record key of parent */
	k_uint_t				 c_sys_id;		 /* system id */
	invent_data_t 			 c_invent; 		 /* invent_data_s struct */
	int                 	 c_category;     /* component category */
	int					     c_type;		 /* component type by category */
	char					*c_location;     /* if board, contains slot name */
	char               		*c_name;         /* name for printing */ 
	char               		*c_serial_number; 
	char               		*c_part_number;   
	char               		*c_revision;      
	int						 c_enabled;      /* -1 if not relevant */
	time_t					 c_install_time;
	time_t					 c_deinstall_time;
	int						 c_data_table;   /* index of table holding data */
	int						 c_data_key;	 /* DB record key of spec data */
	void				    *c_data;		 /* pointer to type specific data */
	int						 c_vhndl;		 /* hwgraph vertex handle */
	int						 c_flags;		 /* various flag values */
	int						 c_state;		 /* used when comparing hwconfigs */
} hw_component_t;

#define c_inv_class 		c_invent.inv_class
#define c_inv_type 			c_invent.inv_type
#define c_inv_controller 	c_invent.inv_controller
#define c_inv_unit 			c_invent.inv_unit
#define c_inv_state 		c_invent.inv_state
#define c_inv_vhndl 		c_invent.inv_vhndl

/* Hardware Component Categories
 */
#define HW_SYSTEM			 0	/* The root of the hwconfig tree */
#define HW_PARTITION	     1
#define HW_MODULE		     2  /* For IP27 systems only */
#define HW_POWER_SUPPLY      3
#define HW_PCI             	 4
#define HW_SYSBOARD			 5  /* motherboard, backplane, midplane, etc. */
#define HW_BOARD		     6

#define HW_CPU  		    10
#define HW_MAIN_MEM		    11
#define HW_MEMBANK		    12
#define HW_IOA		     	13

/* Use the values stored in c_inv_class and c_inv_type (obtained from
 * the header file sys/invent.h).
 */
#define HW_INVENTORY        99

/* HW_SYSBOARD Types
 */
#define SBD_MOTHERBOARD      1
#define SBD_O200MB           2
#define SBD_BACKPLANE        3
#define SBD_MIDPLANE         4
#define SBD_FRONTPLANE       5

 /* HW_Board Category Types
  */
#define BD_NOBOARD           0
#define BD_NODE              1   /* REALLY AN IP27 */
#define BD_ROUTER            2
#define BD_METAROUTER        3
#define BD_BASEIO            4
#define BD_HIPPI_SERIAL      5
#define BD_PCI_XIO           6
#define BD_MSCSI             7
#define BD_GE14_2			 8
#define BD_GE14_4            9
#define BD_GE16_4           10
#define BD_FIBRE_CHANNEL    11
#define BD_IP19             12
#define BD_IP21             13
#define BD_IP25             14
#define BD_IP30             15  /* Octane System Module (System Board) */
#define BD_IP31             16
#define BD_IO4              17
#define BD_MC3              18
#define BD_MIO              19
#define BD_KTOWN            20
#define BD_PM10             21  /* Octane Processor Module (1 CPU) */
#define BD_PM20             22  /* Octane Processor Module (2 CPU) */
#define BD_GM10             23  /* Octane SI (Solid Impact) Graphics */
#define BD_GM20             24  /* Octane SII (Super Solid Impact) Graphics */
#define BD_MOT10            25  /* Octane Graphics */
#define BD_MOT20            26  /* Octane Graphics */
#define BD_GALILEO15		27  /* Video board */
#define BD_PRESENTER		28  
#define BD_PCI_ENET			39
#define BD_XTALK_PCI		30
#define BD_QUAD_ATM			31
#define BD_VME_XTOWN_9U		32
#define BD_MENET			33
#define BD_IP31PIMM			34  /* PIMM that plugs into the IP31 */
#define BD_MCO			    35
#define BD_PIMM_1XT5_1MB	36  /* IP29 (O200) PIMM board w/1 CPU */
#define BD_PIMM_2XT5_1MB	37  /* IP29 (O200) PIMM board w/2 CPUs */

#define BD_GFX				95  /* GFX Sub-Component board */
#define BD_PIMM				96  /* Generic O200 PIMM (placeholder) */
#define BD_PROCESSOR		97  /* Generic Processor Board (placeholder) */
#define BD_GRAPHICS			98  /* Generic Graphics Board (placeholder) */
#define BD_UNKNOWN          99

/* HW_CPU Category Types
 */
#define CPU_R2000            1
#define CPU_R2000A           2
#define CPU_R3000            3
#define CPU_R3000A           4
#define CPU_R4000          	 5
#define CPU_R4400            6
#define CPU_R4600            7
#define CPU_R4650            8
#define CPU_R4700            9
#define CPU_R5000           10
#define CPU_R6000           11
#define CPU_R6000A          12
#define CPU_R8000           13
#define CPU_R10000          14
#define CPU_R12000          15
#define CPU_UNKNOWN         99

/* HW_IOA Category Types
 */
#define IOA_NULL			 0
#define IOA_SCSI			 1
#define IOA_EPC			  	 2
#define IOA_FCHIP			 3
#define IOA_SCIP			 4

/* HW_IOA types representing adapters that can connect to the FCHIP
 */
#define IOA_VMECC			 5
#define IOA_HIPPI			 6
#define IOA_FCG				 7
#define IOA_DANG	         8
#define IOA_GIOCC	         9
#define IOA_HIP1A	        10
#define IOA_HIP1B	        11
#define IOA_UNKNOWN	        99

/* HW_PCI Category Types
 */
#define PCI_XTALKPCI		 1
#define PCI_MODULE			98  /* Generic (placeholder */

/* HW_POWER_SUPPLY Category Types
 */
#define PWR_S2            	 1
#define PWR_SPLY            98  /* Generic Power Supply (placeholder) */

/* Private data structure that contains information about the various
 * components contained on a node board. Note that detailed information
 * about each component is contained in seperate hw_component_s structs
 * that are linked into the node hw_component_s struct via the cmplist
 * field.
 */
typedef struct node_data_s {
	k_uint_t       		sys_id;		    /* system id */	
	uint         		comp_rec_key;   /* component record key */
	uint         		rec_key;   		/* node_data record key */
	uint				n_cpu_count;	/* number of cpus on node */
	uint				n_memory;		/* node memory in MB */
} node_data_t;

/* Private data structure that contains information about how many
 * cpus are installed on a processor board (e.g., IP19, IP21, etc.)
 * Note that detailed information about each of the cpus is contained
 * in seperate hw_component_s structs that are linked into the processor
 * hw_component_s struct via the cmplist field.
 */
typedef struct processor_data_s {
	k_uint_t       		sys_id;		    /* system id */	
	uint         		comp_rec_key;   /* component record key */
	uint                n_cpu_count;    /* number of cpus on board */
} processor_data_t;

/* Private data structure that contains information about a 
 * particular io adapter.
 */
typedef struct ioa_data_s {
	k_uint_t       		sys_id;		    /* system id */	
	uint         		comp_rec_key;   /* component record key */
	uint				ioa_rec_key;
	uint				ioa_enable;
	uint				ioa_type;
	uint				ioa_virtid;
	uint				ioa_subtype;
} ioa_data_t;

/* Private data structure that contains information about a 
 * particular cpu.
 */
typedef struct cpu_data_s {
	k_uint_t       		sys_id;		    /* system id */	
	uint         		comp_rec_key;   /* component record key */
	uint         		cpu_rec_key;	/* cpu record key */
	uint				cpu_module;
	uint				cpu_slot;
	uint  				cpu_slice;
	uint  				cpu_id;			/* virtual id of cpu */
	uint  				cpu_speed;		/* clock speed */
	uint				cpu_flavor;		/* differentiate processor */
	uint  				cpu_cachesz;	/* secondary cache size */
	uint				cpu_cachefreq;	/* speed of the secondary cache */
	uint  				cpu_promrev;
	uint				cpu_enabled;
} cpu_data_t;

/* Private data structure that contains information about memory 
 * installed in a given memory bank;
 */
typedef struct membank_data_s {
	k_uint_t       		sys_id;		    /* system id */	
	uint         		comp_rec_key;   /* component record key */
	uint				mb_rec_key;
	uint				mb_attr;
	uint				mb_flag;
	uint				mb_bnk_number;
	uint				mb_size;		
	uint				mb_enable;
} membank_data_t;

/* Macros for typecasting various private data types to the c_data 
 * pointer in the hw_component_s struct.
 */
#define NODE_DATA(c) 		((node_data_t *)(c->c_data))
#define PROCESSOR_DATA(c) 	((processor_data_t *)(c->c_data))
#define CPU_DATA(c) 		((cpu_data_t *)(c->c_data))
#define IOA_DATA(c) 		((ioa_data_t *)(c->c_data))
#define MEMBANK_DATA(c) 	((membank_data_t *)(c->c_data))

extern unsigned long MemSizes[];

/* Header struct for hwconfig data
 */
typedef struct hwconfig_s {
	int                     h_flags;      /* K_PERM/K_TEMP, etc. */
	k_uint_t                h_sys_id;     /* system ID (can be zero) */
	int                     h_sys_type;   /* IP type of system */
	time_t                  h_date;       /* if 0 then hwconfig is current */
	hw_component_t         *h_hwcmp_root;
	int                     h_hwcmp_cnt;
	string_table_t         *h_st;
} hwconfig_t;

#define HWAFLG(hcp) ALLOCFLG(hcp->h_flags)
#define STRINGTAB_FLG	0x4	 /* must coexist with K_TEMP/K_PERM */

/* Structs for printing out inventory_s structures
 */
typedef struct invrec_s {
    k_ptr_t         ptr;
    kaddr_t         addr;
    path_rec_t     *path;
} invrec_t;

typedef struct inventory_rec_s {
    int             count;
    invrec_t      **table;
} inventory_rec_t;

/* Structs for use when building a hwconfig tree
 */
typedef struct invstruct_s {
    invent_data_t  *ptr;
    path_rec_t     *path;
} invstruct_t;

typedef struct invent_rec_s {
    int             count;
    invstruct_t   **table;
} invent_rec_t;

typedef struct invent_level_s {
    struct invent_level_s   *next;
    struct invent_level_s   *prev;
    int                      level;
    int                      vhndl;
    hw_component_t          *hwcp;
    invstruct_t             *invp;
	int						 invp_free;	 /* if non-zero, free invp */
} invent_level_t;

#define INSERT_AFTER        1

#define INVENT_SCSICONTROL	0x00000001
#define INVENT_SCSIDRIVE	0x00000002
#define INVENT_SCSITAPE		0x00000004
#define INVENT_PCIADAP		0x00000008
#define INVENT_CPUCHIP  	0x00000010
#define INVENT_FPUCHIP  	0x00000020
#define INVENT_MEM_MAIN		0x00000040
#define INVENT_NETWORK		0x00000080
#define INVENT_MISC			0x00000100
#define INVENT_ALL			0xffffffff

#define INVENT_SCSI_ALL	(INVENT_SCSICONTROL|INVENT_SCSIDRIVE|INVENT_SCSITAPE)

typedef unsigned char byte;
 
/*
 * INQUIRY response 
 * X3T9.2 Project 375D Revision 10L 07-Sep-93 SCSI-2
 * Chapter 8 Table 45
 */
typedef struct scsi_inq_s {
	/* 0 */ byte periph_qual    : 4,
				 periph_devtype : 4;
	 
	/* 1 */ byte rmb            : 1,  /* removeable media                   */
				 device_type    : 7;  /* used in SCSI-1 for vendor specific */
									  /* codes                              */
	/* 2 */ byte AENC           : 1,
				 TrmIOP         : 1,
				 reserved0      : 2,
				 resp_data_form : 4;
	 
	/* 3 */ byte version_ISO    : 2,
				 version_ECMA   : 3,
				 version_ANSI   : 3;
	 
	/* 4 */ byte add_len;              /* (n -4)                             */
	/* 5 */ byte reserved1;
	/* 6 */ byte reserved2;
	 
	/* 7 */ byte RelAdr         : 1,
				 WBus32         : 1,
				 WBus16         : 1,
				 Sync           : 1,
				 Linked         : 1,
				 reserved3      : 1,
				 CmdQue         : 1,
				 SftRe          : 1;
	 
	/* 8 */ byte vendor_id  [ 8];
	/*16 */ byte product_id [16];
	/*32 */ byte product_rev[ 4];
	/*36 */ byte vendor_spec[20];
} scsi_inq_t;

/** Function prototypes
 **/
int kl_get_hwconfig(
	hwconfig_t *		/* hwconfig_s pointer */,
	int					/* flag */);

/* Extract various bits of information from a NIC string. In the
 * event that more than one entry is in a particular NIC string, the
 * s parameter will be loaded with the the first token in the next NIC
 * entry.
 */
char *kl_get_mfg_info(
	char**      /* Pointer to where Part string should be placed */,
	char**      /* Pointer to where Name string should be placed */,
	char**      /* Pointer to where Serial Number string should be placed */,
	char**      /* Pointer to where Revision string should be placed */,
	char**      /* Pointer to where Group string should be placed */,
	char**      /* Pointer to where Capability string should be placed */,
	char**      /* Pointer to where Variety string should be placed */,
	char**      /* Pointer to where Laser string should be placed */,
	char*       /* Pointer to next token */);

invent_rec_t *get_invent_table(
	int					/* Module, 0 for all BUG module, -1 for everything */, 
	int					/* invent type flag */,
	int					/* block allocation flag */);

void free_invent_table(
	invent_rec_t *		/* pointer to invent_table */);

path_rec_t *find_parent(
	path_rec_t *		/* pointer to linked list of path records */, 
	int 				/* block allocation flag */);
