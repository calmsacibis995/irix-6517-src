/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSPrivate.H                                        */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 15:18:48 1995                            */
/****************************************************************/


#ifndef BPSPRIVATE_HEADER
#define BPSPRIVATE_HEADER
#include "bps.h"

typedef enum BPS_MSG_ID {
   BPS_MSG_QUERY,
   BPS_MSG_CANCEL,
   BPS_MSG_SUBMIT,
   BPS_MSG_SYSTEM,
   BPS_MSG_ERROR,
} bps_msg_id_t;

typedef enum BPS_SYSTEM_MSG {
   BPS_SYS_SSPART_CREATE,
   BPS_SYS_SSPART_DELETE,
   BPS_SYS_THREAD_KILL,
   BPS_SYS_THREAD_CREATE,
   BPS_SYS_CHANGE_DEFAULT,
   BPS_SYS_MODIFY_PARTITION,
   BPS_SYS_CHANGE_PERMISSIONS,
} bps_system_msg_t;

typedef struct bps_system_message {
   bps_system_msg_t msg;
   union bps_msg_data_t {
      long num_threads;
      bps_sspartition_id_t uuid;
      bps_tune_data_t tune_data;
      bps_sspartinit_data_t init_data;
      bps_permissions_msg_t perm_msg;
   } bps_msg_data;
} bps_system_message_t;

typedef struct bps_message {
   bps_msg_id_t msg;
   union  bps_msg_data_t {
      bps_info_request_t info;
      bps_request_t req;
      bps_system_message_t sys_msg;
      bps_info_reply_t info_reply;
      bps_reply_t reply;
      } bps_msg_data;
} bps_message_t ;


typedef struct bps_job {
   bps_sspartition_id_t id;
   bps_time_t end;
   bps_time_t length;
   ncpu_t cpus;
   memory_t memory;
} bps_job_t;

/*
  This stuff here is the 'marshalling and unmarshalling'
  code. It really should be replaced by a more advanced IDL
  compiler!
*/
error_t bps_messageserver_send(int sockfd, bps_message_t* msg);
error_t bps_messageclient_recv(int sockfd, bps_message_t*);
error_t bps_messageclient_send(int sockfd, bps_message_t* msg);
error_t bps_messageserver_recv(int sockfd, bps_message_t*);
error_t bps_inforeply_send(int sockfd, bps_info_reply_t* info_reply);
error_t bps_inforeply_recv(int sockfd, bps_info_reply_t* reply);
error_t bps_reply_send(int sockfd, bps_reply_t* reply);
error_t bps_reply_recv(int sockfd, bps_reply_t* reply);
error_t bps_sysmsg_send(int sockfd, bps_system_message_t* msg);
error_t bps_sysmsg_recv(int sockfd, bps_system_message_t* msg);
error_t bps_partinit_send(int sockfd, bps_system_message_t* msg);
error_t bps_partinit_recv(int sockfd, bps_system_message_t* msg);
error_t bps_threadmod_send(int sockfd, bps_system_message_t* msg);
error_t bps_threadmod_recv(int sockfd, bps_system_message_t* msg);
error_t bps_changedefault_send(int sockfd, bps_system_message_t* msg);
error_t bps_changedefault_recv(int sockfd, bps_system_message_t* msg);
error_t bps_partmod_send(int sockfd, bps_system_message_t* msg);
error_t bps_partmod_recv(int sockfd, bps_system_message_t* msg);
error_t bps_permmsg_send(int sockfd, bps_system_message_t* msg);
error_t bps_permmsg_recv(int sockfd, bps_system_message_t* msg);
error_t bps_inforeq_send(int sockfd, bps_info_request_t* msg);
error_t bps_inforeq_recv(int sockfd, bps_info_request_t* msg);
error_t bps_request_send(int sockfd, bps_request_t* msg);
error_t bps_request_recv(int sockfd, bps_request_t* msg);
#endif





