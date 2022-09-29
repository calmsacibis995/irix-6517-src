#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/include/RCS/configmon_api.h,v 1.3 1999/05/08 10:13:42 tjm Exp $"

#define CM_TEMP 1
#define CM_PERM 2

/*
 * Handle to use with various configmon objects
 */
typedef void * cm_hndl_t;

/* 
 * configmon return value that can contain all data types (pointers,
 * char, long, etc.).
 */
typedef unsigned long long cm_value_t;

/* Data types
 */
#define VOID_POINTER			1   /* For private data structs */
#define STRING_POINTER			2   /* Pointer to character string */
#define CHARACTER               3	/* character */
#define SIGNED_CHAR             4	/* 8-bit signed value */
#define UNSIGNED_CHAR           5	/* 8-bit unsigned value */
#define SHORT                   6	/* 16-bit signed value */
#define UNSIGNED_SHORT          7	/* 16-bit unsigned value */
#define SIGNED    			    8	/* 32-bit signed value */
#define UNSIGNED    		    9	/* 32-bit unsigned value */
#define LONG       			   10	/* 64-bit signed value */
#define UNSIGNED_LONG		   11	/* 64-bit unsigned value */
#define INVALID_TYPE		   12	/* one past last valid type */

/* Data conversion macros
 */
#define CM_VOID_PTR(x) ((void *)(x))
#define CM_STRING(x) ((char *)(x))
#define CM_CHARACTER(x) ((char)(x))
#define CM_SIGNED_CHAR(x) ((signed char)(x))
#define CM_UNSIGNED_CHAR(x) ((unsigned char)(x))
#define CM_SHORT(x) ((short)(x))
#define CM_UNSIGNED_SHORT(x) ((unsigned short)(x))
#define CM_SIGNED(x) ((int)(x))
#define CM_UNSIGNED(x) ((unsigned int)(x))
#define CM_LONG(x) ((long long)(x))
#define CM_UNSIGNED_LONG(x) ((unsigned long long)(x))

/* Configmon Event Types (it's possible for more than one type of
 * change to occur with a single event).
 */
#define CONFIGMON_INIT      	 0x01
#define SYSINFO_CHANGE         	 0x02
#define HARDWARE_INSTALLED       0x04
#define HARDWARE_DEINSTALLED     0x08
#define SOFTWARE_INSTALLED       0x10
#define SOFTWARE_DEINSTALLED     0x20
#define SYSTEM_CHANGE            0x40
#define ALL_CM_EVENTS			 0xff

/* Configmon change_item types 
 */
#define SYSINFO_CURRENT     		1
#define SYSINFO_OLD         		2
#define HW_INSTALLED        		3
#define HW_DEINSTALLED      		4
#define SW_INSTALLED        		5
#define SW_DEINSTALLED      		6
	
/* Configmon item types. Each type coresponds to a private data structure.
 */
#define LIST_TYPE       			1
#define CM_EVENT_TYPE       		2
#define CHANGE_ITEM_TYPE        	3
#define SYSINFO_TYPE            	4
#define HWCOMPONENT_TYPE        	5
#define SWCOMPONENT_TYPE        	6

/* Defines for get_component() function
 */
#define CM_FIRST					1
#define CM_LAST						2
#define CM_NEXT						3
#define CM_PREV						4
#define CM_NEXT_PEER				5
#define CM_PREV_PEER				6
#define CM_LEVEL_UP					7
#define CM_LEVEL_DOWN				8

/* CM_EVENT_TYPE data tags
 */
#define CE_SYS_ID					0   /* LONG */
#define CE_TIME						1  	 /* UNSIGNED */
#define CE_TYPE						2   /* SIGNED */
#define CE_INVALID					3   /* one past last valid tag */

/* CHANGE_ITEM_TYPE data tags
 */
#define CI_SYS_ID					0  /* LONG */
#define CI_TIME						1  /* UNSIGNED */
#define CI_TYPE						2  /* SIGNED */
#define CI_TID						3  /* SIGNED */
#define CI_REC_KEY					4  /* SIGNED */
#define CI_INVALID					5  /* one past last valid tag */

/* SYSINFO_TYPE data tags
 */
#define SYS_REC_KEY					0	/* SIGNED */
#define SYS_SYS_ID					1	/* LONG */
#define SYS_SYS_TYPE				2	/* SIGNED */
#define SYS_SERIAL_NUMBER			3	/* STRING_POINTER */
#define SYS_HOSTNAME				4	/* STRING_POINTER */
#define SYS_IP_ADDRESS				5	/* STRING_POINTER */
#define SYS_ACTIVE					6	/* SHORT */
#define SYS_LOCAL					7	/* SHORT */
#define SYS_TIME					8	/* UNSIGNED */
#define SYS_INVALID					9	/* one past last valid tag */

/* HWCOMPONENT_TYPE data tags
 */
#define HW_SEQ  					0	/* SIGNED */
#define HW_LEVEL					1	/* SIGNED */
#define HW_REC_KEY					2	/* SIGNED */
#define HW_PARENT_KEY				3	/* SIGNED */
#define HW_SYS_ID					4	/* LONG */
#define HW_CATEGORY					5	/* SIGNED */
#define HW_TYPE						6	/* SIGNED */
#define HW_INV_CLASS				7	/* SIGNED */
#define HW_INV_TYPE					8	/* SIGNED */
#define HW_INV_CONTROLLER			9	/* SIGNED */
#define HW_INV_UNIT				   10	/* SIGNED */
#define HW_INV_STATE			   11	/* SIGNED */
#define HW_LOCATION				   12	/* STRING_POINTER */
#define HW_NAME					   13	/* STRING_POINTER */
#define HW_SERIAL_NUMBER	   	   14	/* STRING_POINTER */
#define HW_PART_NUMBER		       15	/* STRING_POINTER */
#define HW_REVISION			       16	/* STRING_POINTER */
#define HW_ENABLED			       17	/* SIGNED */
#define HW_INSTALL_TIME		       18	/* UNSIGNED */
#define HW_DEINSTALL_TIME	       19	/* UNSIGNED */
#define HW_DATA_TABLE	   	       20	/* SIGNED */
#define HW_DATA_KEY      	       21	/* SIGNED */
#define HW_INVALID			       22	/* one past last valid tag */

/* HW_CATEGORY values (which can be used to control the type of
 * hardware components printed out (partially borrowed from the 
 * KLIB header file kl_hwconfig.h).
 */
#define HWC_SYSTEM            	    0 /* The root of the hwconfig tree */
#define HWC_PARTITION         	    1
#define HWC_MODULE            	    2
#define HWC_POWER_SUPPLY      		3
#define HWC_PCI               		4
#define HWC_SYSBOARD          	    5 /* motherboard, backplane, etc. */
#define HWC_BOARD             	    6

/* Note that all sub-component values must be greater than the 
 * value of HWC_BOARD 
 */

/* SWCOMPONENT_TYPE data tags
 */
#define SW_NAME					    0	/* STRING_POINTER */
#define SW_SYS_ID				    1	/* LONG */
#define SW_VERSION				    2	/* UNSIGNED */
#define SW_DESCRIPTION			    3	/* STRING_POINTER */
#define SW_INSTALL_TIME			    4	/* UNSIGNED */
#define SW_DEINSTALL_TIME		    5	/* UNSIGNED */
#define SW_REC_KEY				    6	/* UNSIGNED */
#define SW_INVALID				    7	/* one past last valid tag */

/* Operator defines
 */
#define OP_EQUAL				    0
#define OP_LESS_THAN			    1
#define OP_GREATER_THAN			    2
#define OP_LESS_OR_EQUAL		    3
#define OP_GREATER_OR_EQUAL		    4
#define OP_NOT_EQUAL			    5
#define OP_ORDER_BY				    6

/* Flags for selectively accessing active and archive tables via 
 * cm_select_list()
 */
#define ARCHIVE_FLG				    1
#define ACTIVE_FLG				    2
#define ALL_FLG					    3

extern char *operator[];

/**
 ** Function prototypes for configmon API
 **/

int cm_init();

void cm_free();

cm_hndl_t cm_alloc_config(
	char *			/* database name */,
	int				/* dso flag */);

void cm_free_config(
	cm_hndl_t 		/* config_rec handle */);

int cm_get_sysconfig(
	cm_hndl_t  		/* config_rec handle */,
	cm_value_t 		/* sys_id */,
	time_t			/* time */);

cm_hndl_t cm_sysinfo(
	cm_hndl_t 		/* config handle */);

cm_hndl_t cm_first_hwcmp(
	cm_hndl_t		/* config_rec handle */);

cm_hndl_t cm_get_hwcmp(
	cm_hndl_t 		/* current hwcomponent handle */,
	int				/* operation tag */);

cm_hndl_t cm_first_swcmp(
	cm_hndl_t		/* current swcomponent handle*/);

cm_hndl_t cm_get_swcmp(
	cm_hndl_t 		/* Current swcomponent handle */,
	int 			/* operation tag */);

int cm_field_type(
	int 			/* structure type */, 
	int 			/* field data_tag */);

cm_value_t cm_field(
	cm_hndl_t 		/* component handle */, 
	int 			/* component type tag */, 
	int 			/* field data tag */);

int cm_change_item_type(
    cm_hndl_t     	/* ITEM handle */);

cm_hndl_t cm_event_history(
	cm_hndl_t,
	unsigned long long,
	time_t,
	time_t,
	int);

cm_hndl_t cm_change_items(
	cm_hndl_t, 
	time_t);

cm_hndl_t cm_item_history(
	cm_hndl_t, 
	time_t, 
	time_t);

cm_hndl_t cm_select_list(
	cm_hndl_t 		/* dbcmd handle */,
	int				/* flag (ARCHIVE_FLG, ACTIVE_FLG, ALL_FLG) */);

void cm_free_list(
	cm_hndl_t 		/* config_rec handle */, 
	cm_hndl_t 		/* list handle */);

int cm_list_count(
	cm_hndl_t 		/* item handle */);

cm_hndl_t cm_item(
	cm_hndl_t 		/* item list handle */, 
	int 			/* operation tag */);

cm_hndl_t cm_get_item(
	cm_hndl_t , 
	cm_hndl_t );

cm_hndl_t cm_get_change_item(
	cm_hndl_t , 
	cm_hndl_t );

void cm_free_item(
	cm_hndl_t 		/* config_rec handle */,
	cm_hndl_t 		/* item pointer */,
	int 			/* item type */);

cm_hndl_t cm_alloc_dbcmd(
	cm_hndl_t,
	int);

void cm_free_dbcmd(
	cm_hndl_t 		/* database command handle */);
