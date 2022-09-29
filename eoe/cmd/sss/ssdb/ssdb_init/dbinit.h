#ifndef H_SSDBINIT_H
#define H_SSDBINIT_H

/* Structure used for determining system information */
typedef struct system{
        int rec_key;
        char sys_id[32];
	char dbsys_id[32];
        int sys_type;
        char sys_serial[32];
        char hostname[256];
        char ip_address[64];
        int active;
        int local;
        int time;
	int id_changed;
	int hostchange;
}SYSTEM_TABLE;


/* Structure used for building list of Class information */
typedef struct class_type{
        int class;
        char class_desc[255];
        int type;
        char type_desc[255];
        int throttled;
        int throttle_val;
        int threshold;
        int priority;
        int facility;
	int enabled;
	int action_enabled;
        struct class_type *next;
}CLASS_TYPE;

/* Structure used for building list of CLASS and TYPE from the database */
typedef struct db_list{
        char sys_id[20];
        int class;
        char class_desc[255];
        int type;
        char type_desc[255];
        struct db_list *next;
}DBLIST;

typedef struct tool_config{
	char tool_name[36];
	char tool_option[36];
	char option_default[256];
	struct tool_config *next;
}TOOLS;

#define TRUE 1
#define FALSE 0
int SQL_TYPE;

/* Function prototypes */

CLASS_TYPE * read_events(CLASS_TYPE *,FILE *fp);
CLASS_TYPE * add_to_list (CLASS_TYPE *head,CLASS_TYPE *newclass);
DBLIST * add_dblist(DBLIST *dbhead,DBLIST *dblistptr);
DBLIST * dbread (DBLIST *dbhead,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void printlist(CLASS_TYPE *head);
void printdblist(DBLIST *head);
int compare_lists(CLASS_TYPE *listhead,DBLIST *dbhead,SYSTEM_TABLE *system_info_ptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
int get_system_info(SYSTEM_TABLE *system_info_ptr);
int check_system_info(SYSTEM_TABLE *system_info_ptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
int create_system_record(SYSTEM_TABLE *system_info_ptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void free_eventlist(CLASS_TYPE *head);
int create_default_action(SYSTEM_TABLE *system_info_ptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void free_dblist(DBLIST *head);
int check_tool_table(ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
TOOLS * read_tools(void);
TOOLS * add_to_config(TOOLS *tool_ptr,TOOLS *newtool);
int compare_tools(TOOLS *tool_ptr,TOOLS *tool_dbptr,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
TOOLS * read_dbtools(ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
int check_tool_table(ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
int create_sssconfig(ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle,SYSTEM_TABLE *system_info_ptr);
void print_tools(TOOLS *head);
void free_toolslist(TOOLS *head);
void get_actual_host(SYSTEM_TABLE *ptr,char *name);
#endif /* #ifndef H_SSDBINIT_H */
