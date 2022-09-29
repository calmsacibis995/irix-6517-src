#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/include/RCS/configmon.h,v 1.2 1999/05/08 04:08:30 tjm Exp $"

#include "configmon_api.h"
#include "sysinfo.h"
#include "dbaccess.h"

#define STRTOULL(s) (strtoull(s, (char **)NULL, 16))
#define NEXTPTR(s) (&(s)[strlen(s)])

typedef struct configmon_s {
	int					 flags;
	int					 state;
	time_t				 curtime;
	int					 event_type;
	system_info_t		*sysinfo;
	database_t			*database;
	sysconfig_t			*sysconfig;
	sysconfig_t			*db_sysconfig;
	hw_component_t		*hwcmp_archive;
	sw_component_t		*swcmp_archive;
} configmon_t;

#define SYS_ID(cmp) (cmp)->sysinfo->sys_id
#define SYS_TYPE(cmp) (cmp)->sysinfo->sys_type
#define LOCAL(cmp) (cmp)->sysinfo->local

/* Some flags for the state field
 */
#define HW_DIFFERENCES		1
#define SW_DIFFERENCES		2

#include "swconfig.h"
#include "hwconfig.h"

#define LIST_MAGIC 0xbaddcadd

typedef struct list_s {
	int					list_magic;
	int					list_type;
	int					item_type;	
	time_t				start_time;
	time_t				end_time;
	int					index;
	int					count;
	void          	  **item; 		/* Array of item specific structs */
} list_t;

/* Some defines for the list_type field
 */
#define ITEM_LIST				1
#define ITEM_HISTORY_LIST		2
#define CHANGE_HISTORY_LIST		3

/** Function prototypes
 **/
configmon_t *alloc_configmon(
	char *              /* database name */,
	int                 /* bounds */,
	int                 /* flags */,
	int					/* dso flag */);

int setup_configmon(
	configmon_t *		/* pointer to configmon_s struct */,
	k_uint_t 			/* system ID */);

void free_configmon(
	configmon_t *		/* pointer to configmon_s struct */);

int get_system_info(
	configmon_t *		/* pointer to configmon_s struct */, 
	k_uint_t 			/* system ID */);

int db_sysconfig(
	configmon_t *		/* pointer to configmon_s struct */, 
	time_t 				/* time */);

int get_db_sysinfo(
	configmon_t *		/* pointer to configmon_s struct */, 
	k_uint_t 			/* system ID */,
	time_t 				/* time */);

int get_db_hwconfig(
	configmon_t *       /* pointer to configmon_s struct */,
	time_t              /* time */);

int get_db_swconfig(
	configmon_t *       /* pointer to configmon_s struct */,
	time_t              /* time */);

void free_system_info(
	system_info_t *		/* pointer to system_info_s struct */);
