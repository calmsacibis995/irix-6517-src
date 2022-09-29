/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kao                                                 */
/*    FILE: bps.h                                               */
/*    ASGN:                                                     */
/*    DATE: Mon Jul 10 16:50:02 1995                            */
/****************************************************************/


#ifndef BPS_HEADER
#define BPS_HEADER
#include <sys/time.h>
#ifdef __cplusplus
extern "C" {
   boolean_t uuid_equal (uuid_t *uuid1, uuid_t *uuid2, uint_t *status);
   void  uuid_create (uuid_t *uuid, uint_t *status);
   int uuid_compare (uuid_t *uuid1, uuid_t *uuid2, uint_t *status);
};
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* 
  Some basic typedefs
*/ 

typedef int bps_jobid_t; 
typedef pid_t bps_magic_cookie_t;
typedef int bool_t;
typedef int ncpu_t;
typedef unsigned long memory_t;
typedef uuid_t bps_policy_id_t;
typedef uuid_t bps_sspartition_id_t;
typedef int cpu_usage_t;
typedef void* bps_authentication_data_t;
typedef void* bps_userid_t;
typedef long bps_time_t;
#define BPS_DFLT_PORT 2500
#define BPS_THREADS 1
#define BPS_WINDOW_DFLT -1
#define BPS_POLICY "0996a492-5a34-101e-8ece-080069026881"
#define BPS_PARTITION "0996a7ad-5a34-101e-8ece-080069026881"

/*
   Error values returned by the system
*/
typedef enum BPS_ERR {
   BPS_ERR_OK,
   BPS_ERR_BAD_MSG_TYPE,
   BPS_ERR_INVALID_TIME_INTERVAL,
   BPS_ERR_COULD_NOT_CREATE_SSPART,
   BPS_ERR_MEMORY_USED,
   BPS_ERR_BAD_MAGIC_COOKIE,
   BPS_ERR_CPU_USED,
   BPS_ERR_BAD_POLICY,
   BPS_ERR_TOO_MANY_THREADS,
   BPS_ERR_BAD_SSPART_ID,
   BPS_ERR_OUT_OF_MEMORY,
   BPS_ERR_INVALID_TIME,
   BPS_ERR_BAD_SOCKET,
   BPS_ERR_BAD_READ,
   BPS_ERR_BAD_WRITE,
   BPS_ERR_NO_MATCH,
   BPS_ERR_NO_JOB,
   BPS_ERR_WINDOW_TOO_SMALL,
   BPS_ERR_WINDOW_TOO_LARGE,
   BPS_ERR_NOT_ENOUGH_CPUS,
   BPS_ERR_BAD_MSG,
   BPS_ERR_INTERVAL_BAD,
   BPS_ERR_DURATION_BAD,
   BPS_ERR_NCPU_BAD,
   BPS_ERR_MEMORY_BAD,
   BPS_ERR_INIT_FILE_BAD,
   BPS_ERR_BAD_DATA,
   BPS_ERROR
} error_t;


/*
  A request to submit/resubmit or cancel a job
*/
typedef struct bps_request {
   bps_authentication_data_t auth;
   bps_sspartition_id_t sspart;
   bps_magic_cookie_t job_id;
   bps_policy_id_t policy;
   ncpu_t ncpus;
   ncpu_t multiple; 
   memory_t memory;
   bps_time_t toc;
   bps_time_t window;
} bps_request_t;

/*
  The structur returned as a reply
*/
typedef struct bps_reply {
   error_t err;
   bps_time_t toc;
   bps_magic_cookie_t cookie;
   bps_sspartition_id_t sspart;
} bps_reply_t;
  

/*
  What kind of query is to be performed
*/
typedef enum BPS_QUERY_FLAGS {
   BPS_QUERY_SSPART,
   BPS_QUERY_SYS,
} bps_query_t;

typedef struct bps_info_request {
   bps_authentication_data_t auth;
   bps_query_t flag;
   bps_sspartition_id_t id;
   bps_time_t start;
   bps_time_t end;
} bps_info_request_t;   


/*
  These are variable length arrays used to transmit
  data about partitions
*/
typedef struct _cpus_used {
   int size;
   ncpu_t cpu_usage[1];
} cpus_used_t;

typedef struct _memory{
   int size;
   memory_t memory_usage[1];
}memory_used_t;


typedef struct _partition_id {
   int size;
   bps_sspartition_id_t partition_ids[1];
} partition_ids_t;


/*
  The info reply structure
*/
typedef struct bps_info_reply {
   bps_time_t start; 
   bps_time_t end; /* end of the interval */
   cpus_used_t* cpu_usage; 
   memory_used_t* memory_usage;
   partition_ids_t* partition_ids;
} bps_info_reply_t;


typedef enum BPS_AUTHENTICATION {
   BPS_AUTH_ADD_GROUP,
   BPS_AUTH_REMOVE_GROUP,
   BPS_AUTH_MODIFY_GROUP_PERMISSIONS,
} bps_auth_msg_t;

typedef struct bps_tune_data {
   bps_sspartition_id_t part_id;
   caddr_t data;
   unsigned long size;
} bps_tune_data_t;

typedef  struct  bps_permissions_data {
   caddr_t data;
   unsigned long size;
} bps_permissions_data_t;

typedef struct bps_permissions_msg{
   bps_sspartition_id_t part_id;
   bps_auth_msg_t msg;
   bps_permissions_data_t data;
}  bps_permissions_msg_t;

typedef struct bps_sspartdata {
   bps_sspartition_id_t id;
   bps_time_t interval;
   bps_time_t duration;
   ncpu_t ncpus;
   memory_t memory;
} bps_sspartdata_t;

typedef struct bps_sspartinit_data {
   bps_sspartition_id_t type_id;
   bps_sspartition_id_t part_id;
   char* filename;
} bps_sspartinit_data_t;

bps_reply_t bps_submit(bps_request_t*, char* bps_job_name);
int bps_cancel(bps_magic_cookie_t magic_cookie);
bps_info_reply_t* bps_query(bps_info_request_t* req);
int bps_tune_thr_cre();
int bps_tune_thr_del();

/****
  Utility functions 
***/


void   bps_policy_cpy(bps_policy_id_t* id_to, bps_policy_id_t* id_from);
bool_t bps_policy_equal(bps_policy_id_t* id_to, bps_policy_id_t* id_from);
void   bps_sspartid_cpy(bps_sspartition_id_t* id_to, bps_sspartition_id_t* id_from);
int  bps_magic_cookie_cmp(bps_magic_cookie_t* id_to, bps_magic_cookie_t* id_from);
int bps_sspartid_equal(bps_sspartition_id_t* id_to, bps_sspartition_id_t* id_with);
int bps_sspartid_greaterthan(bps_sspartition_id_t* id_to,
                             bps_sspartition_id_t* id_from);
void bps_sspartid_destroy(bps_sspartition_id_t* id);

void bps_cookie_create(bps_magic_cookie_t* cookie);
void bps_error_print(error_t error);
void initInfoReply(bps_info_reply_t** reply);
void bps_inforeply_free(bps_info_reply_t** reply);
void bps_partitionids_alloc(partition_ids_t** array, int size);
void bps_cpusused_alloc(cpus_used_t** array, int size);
void bps_memoryused_alloc(memory_used_t** array, int size);

#ifdef __cplusplus
};
#endif
#endif












