#ident  "$Header: /proj/irix6.5.7m/isms/eoe/lib/sss/libconfigmon/include/RCS/dbaccess.h,v 1.3 1999/10/21 21:16:19 leedom Exp $"


/* Structure that contains data about a single change event (more than
 * one type of change can occur during a change event.
 */
typedef struct cm_event_s {
	k_uint_t      			sys_id;
	time_t					time;
	int						type;
} cm_event_t;

/* Structures that maps change_item_table in the SSS database
 */
typedef struct change_item_s {
	k_uint_t      			sys_id;
	time_t					time;
	int						type;
	int						tid;
	int						rec_key;
} change_item_t;

/* Index values for the individual table types. The table index
 * value will be used as a table handle by higher level functions. 
 * Note that the first section of table types will be present on 
 * all systems. the rest of the table types may only be relevant 
 * for a particular IP type.
 */
#define CONFIGMON_TBL       0
#define SYSTEM_TBL			1
#define SYSARCHIVE_TBL		2
#define CM_EVENT_TBL    	3
#define CHANGE_ITEM_TBL    	4
#define HWCOMPONENT_TBL		5	 
#define HWARCHIVE_TBL		6
#define SWCOMPONENT_TBL		7
#define SWARCHIVE_TBL		8

/* Private data tables
 */
#define CPU_TBL		    	9
#define NODE_TBL		   10
#define MEMBANK_TBL		   11
#define IOA_TBL		       12
#define PROCESSOR_TBL      13

/* MAX_TABLES is always 1 greater than last data table 
 */
#define MAX_TABLES		   14 

#define VALID_TBL			1

struct database_s;

typedef struct table_rec_s {
	int					 state;
	char				*name;
	void				*ptr; 				/* Array of table records */
	int					 num_records;
	int					 num_fields;
	int					 next_field;
	int					 record_count;  	/* Number of records in array */
	int					 next_record;		/* Index of next record in array */
	int					 next_key;			/* Next key to assign */
} table_rec_t;

#include <sss/ssdbapi.h>
#include <sss/ssdberr.h>

typedef struct database_s {
	char				   	*dbname;
	int					 	 bounds;
	int					 	 state;
	int					 	 flags;	 
	int						 dsoflg;
	unsigned long long	 	 sys_id; /* sys_id (0 if multi-sys DB) */
	int					 	 sys_type; /* system IP type (0 if multi-sys) */
	int					 	 valid_tables;
	table_rec_t			 	 table_array[MAX_TABLES];

	/* Fields for SSDB 
	 */
	ssdb_Client_Handle       ssdb_client;
	ssdb_Connection_Handle   ssdb_connection;
	ssdb_Request_Handle      ssdb_req;
	ssdb_Error_Handle        ssdb_error;
	char 					*sqlbuf;
} database_t;

/* Some state flags
 */
#define DATABASE_EXISTS		1
#define TABLES_EXIST 		2

typedef struct system_config_s {
	unsigned long long	 sys_id;	/* ID of system configuration is for */
	int					 sys_type;	/* IP type of system configuration is for */
	int					 source;	/* (SC_LIVE/SC_DATABASE) */
	time_t				 time;		/* date (and time) configuration is valid */
	sysconfig_t			*sysconfig;	/* hw and sw component trees, etc. */
	database_t			*database;	/* pointer to database control struct */
} system_config_t;

typedef struct table_data_s {
	int             	 tid;
	char           	 	*table_name;
	int             	 next_key;
} table_data_t;

typedef struct configmon_rec_s {
	int					 table_id;
	char				*table_name;
	int					 next_key;
} configmon_rec_t;

typedef struct condition_s {
	int					 field;
	int					 operator;
	cm_value_t			 value;
} condition_t;

#define MAX_CONDITIONS	10
#define LAST_CONDITION	(MAX_CONDITIONS - 1)

#define CHAR_STRING		 	1
#define HEX_STRING		 	2
#define DECIMAL			 	3
#define DECIMAL_UNSIGNED 	4

typedef struct db_query_s {
	int						 flags;
	k_uint_t			 	 sys_id;
	int					 	 key;
	time_t				 	 time;
} db_query_t;

/* Some db_query flag values
 */
#define LOCALHOST		0x1 /* lookup sysinfo for localhost */
#define PARENT_KEY		0x2	/* Treat key as parent key */
#define TIME          	0x4 /* Treat time as generic time */
#define DEINSTALL_TIME	0x8 /* Treat time as deinstall time */

typedef struct db_cmd_s {
	database_t 			    *dbp;
	int						 command_type;
	int					 	 tid;
	int						 data_type;
	db_query_t				 query;			/* XXX -- delete this field */
	int						 next_condition;
	condition_t			 	 condition[MAX_CONDITIONS];
	char 					*cmdstr;
	ssdb_Client_Handle       client;
	ssdb_Connection_Handle   connection;
	ssdb_Request_Handle      request;
	ssdb_Error_Handle        error;
} db_cmd_t;

#define db_flags query.flags
#define db_sys_id query.sys_id
#define db_key query.key
#define db_time query.time

#define CONFIGMON_FP(d) (d)->table_array[CONFIGMON_TBL].fp
#define SYSTEM_FP(d) (d)->table_array[SYSTEM_TBL].fp
#define SYSARCHIVE_FP(d) (d)->table_array[SYSARCHIVE_TBL].fp
#define CM_EVENT_FP(d) (d)->table_array[CM_EVENT_TBL].fp
#define CHANGE_FP(d) (d)->table_array[CHANGE_ITEM_TBL].fp
#define HWCOMPONENT_FP(d) (d)->table_array[HWCOMPONENT_TBL].fp
#define HWARCHIVE_FP(d) (d)->table_array[HWARCHIVE_TBL].fp
#define SWCOMPONENT_FP(d) (d)->table_array[SWCOMPONENT_TBL].fp
#define SWARCHIVE_FP(d) (d)->table_array[SWARCHIVE_TBL].fp
#define CPU_FP(d) (d)->table_array[CPU_TBL].fp
#define NODE_FP(d) (d)->table_array[NODE_TBL].fp
#define MEMBANK_FP(d) (d)->table_array[MEMBANK_TBL].fp
#define IOA_FP(d) (d)->table_array[IOA_TBL].fp
#define PROCESSOR_FP(d) (d)->table_array[PROCESSOR_TBL].fp

#define TABLE(d, i) (d)->table_array[i]
#define NUM_RECORDS(d, i) TABLE(d, i).num_records
#define NUM_FIELDS(d, i) TABLE(d, i).num_fields
#define NEXT_FIELD(d, i) TABLE(d, i).next_field
#define RECORD_COUNT(d, i) (d)->table_array[i].record_count
#define TABLE_STATE(d, i) (d)->table_array[i].state
#define TABLE_NAME(d, i) (d)->table_array[i].name
#define TABLE_FP(d, i) (d)->table_array[i].fp
#define DATA_PTR(d, i) (d)->table_array[i].ptr
#define NEXT_RECORD(d, i) (d)->table_array[i].next_record
#define PUTFUNC(d, i) (d)->table_array[i].putfunc
#define GETFUNC(d, i) (d)->table_array[i].getfunc

#define DATA_ARRAY(d, i) ((void**)DATA_PTR(d, i))

#define CONFIGMON_ARRAY(d, i) \
	((table_data_t **)(d)->table_array[CONFIGMON_TBL].ptr)
#define SYSTEM_ARRAY(d, i) \
	((system_info_t **)(d)->table_array[SYSTEM_TBL].ptr)
#define SYARCHIVE_ARRAY(d, i) \
	((system_info_t **)(d)->table_array[SYSARCHIVE_TBL].ptr)
#define CM_EVENT_ARRAY(d, i) \
	((cm_event_t **)(d)->table_array[CM_EVENT_TBL].ptr)
#define CHANGE_ARRAY(d, i) \
	((change_item_t **)(d)->table_array[CHANGE_ITEM_TBL].ptr)
#define HWCOMPONENT_ARRAY(d) \
	((hw_component_t**)(d)->table_array[HWCOMPONENT_TBL].ptr)
#define HWARCHIVE_ARRAY(d, i) \
	((hw_component_t **)(d)->table_array[HWARCHIVE_TBL].ptr)
#define SWCOMPONENT_ARRAY(d) \
	((sw_component_t **)(d)->table_array[SWCOMPONENT_TBL].ptr)
#define SWARCHIVE_ARRAY(d, i) \
	((sw_component_t **)(d)->table_array[SWARCHIVE_TBL].ptr)
#define CPU_ARRAY(d) \
	((cpu_data_t**)(d)->table_array[CPU_TBL].ptr)
#define NODE_ARRAY(d) \
	((node_data_t**)(d)->table_array[NODE_TBL].ptr)
#define MEMBANK_ARRAY(d) \
	((membank_data_t**)(d)->table_array[MEMBANK_TBL].ptr)
#define IOA_ARRAY(d) \
	((ioa_data_t**)(d)->table_array[IOA_TBL].ptr)
#define PROCESSOR_ARRAY(d) \
	((processor_data_t**)(d)->table_array[PROCESSOR_TBL].ptr)

/* Function prototypes
 */
database_t *open_database(	
	char *				/* database name */, 
	int 				/* bounds */,
	int 				/* alloc flag */,
	int					/* dso flag (if 0, call db init/finish foutines */); 

void close_database(database_t *);
char *table_name(int, int, int);
int table_exists(database_t *, int);
int table_record_count(database_t *, int);
int setup_table(database_t *, int, int);
int setup_tables(database_t *, int, int);
int create_table(database_t *, int, int);
int next_table_key(database_t *, int);
void init_tables(database_t *);
void update_configmon_table(database_t *);
void clean_table_rec(database_t *, int);
void free_table_rec(database_t *, int);

db_cmd_t *alloc_dbcmd(database_t *);

void setup_dbcmd(db_cmd_t *, int, int, char *);

void clean_dbcmd(db_cmd_t *);

void free_dbcmd(db_cmd_t *);

int setup_select_str(db_cmd_t *);

int add_condition(db_cmd_t *, int, int, cm_value_t);

int setup_dbselect(db_cmd_t *);

int setup_dbquery(db_cmd_t *, int, k_uint_t, int, int, time_t);

int db_select_records(db_cmd_t *);

int select_records(
	db_cmd_t *        /* pointer to database command information */);

int insert_record(database_t *, int, void *);
int delete_records(database_t *, int, k_uint_t, int);

void free_record_memory(database_t *, int, k_ptr_t);
void *find_record(
	db_cmd_t *        /* pointer to query information */);

void *next_record(database_t *, int);
int next_key(database_t *, int);

char *add_str(char *, char *);
char *add_comma(char *);

configmon_rec_t *get_configmon_data(db_cmd_t *);

void free_configmon_rec(configmon_rec_t *);

system_info_t *get_system_data(
	db_cmd_t *		/* pointer to query information */);

int put_system_data(database_t *, int, system_info_t *);
hw_component_t *get_hwcomponent(db_cmd_t *);
int put_hwcomponent(database_t *, int, hw_component_t *);
sw_component_t *get_swcomponent(db_cmd_t *);
int put_swcomponent(database_t *, int, sw_component_t *);

cpu_data_t *get_cpu_data(
	db_cmd_t *		/* pointer to query information */);

int put_cpu_data(database_t *, cpu_data_t *);

node_data_t *get_node_data(
	db_cmd_t *		/* pointer to query information */);

int put_node_data(database_t *, node_data_t *);

membank_data_t *get_membank_data(
	db_cmd_t *		/* pointer to query information */);

int put_membank_data(database_t *, membank_data_t *);

ioa_data_t *get_ioa_data(
	db_cmd_t *		/* pointer to query information */);

int put_ioa_data(database_t *, ioa_data_t *);

void *get_next_record(
	db_cmd_t *        /* pointer to query information */);

k_uint_t record_sys_id(int, void *);
int record_key(int, void *);
change_item_t *alloc_change_item(k_uint_t, time_t, int, int, int, int);
change_item_t *get_change_item(db_cmd_t *);
int put_change_item(database_t *, change_item_t *);
int insert_change_item(database_t *, k_uint_t, time_t, int, int, int);
cm_event_t *alloc_cm_event(k_uint_t, time_t, int, int);
cm_event_t *get_cm_event(db_cmd_t *);
int put_cm_event(database_t *, cm_event_t *);
int insert_cm_event(database_t *, k_uint_t, time_t, int, int);

