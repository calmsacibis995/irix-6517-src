/****************************************************************
*    NAME:                                                     
*    ACCT: kostadis                                            
*    FILE: bps.c                                               
*    ASGN:                                                     
*    DATE: Sun Jul 16 16:53:46 1995                            
****************************************************************/




#include <bps.h>
#include <sys/uuid.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
void bps_policy_cpy(bps_policy_id_t* id_to, bps_policy_id_t* id_from){
   assert(id_to != 0);
   assert(id_from != 0);
   assert(sizeof(*id_to) == sizeof(uuid_t));
   memcpy(id_to, id_from, sizeof(uuid_t));
   return;
}

bool_t bps_policy_equal(bps_policy_id_t* id_to, bps_policy_id_t* id_from){
   uint_t status;
   assert(id_to != 0);
   assert(id_from != 0);

   return (uuid_equal(id_to,id_from,&status) == B_TRUE);

};

void initInfoReply(bps_info_reply_t** reply){
   int size = 1;
   *reply = (bps_info_reply_t*) malloc(sizeof(bps_info_reply_t));
   (*reply)->partition_ids = (partition_ids_t*)malloc(sizeof(partition_ids_t) - 1);
   (*reply)->partition_ids->size  = 1;
   (*reply)->cpu_usage = (cpus_used_t *) malloc(sizeof(cpus_used_t) - 1);
   (*reply)->cpu_usage->size  = 1;
    (*reply)->memory_usage = (memory_used_t*)malloc(sizeof(memory_used_t) -1 );
   (*reply)->memory_usage->size  = 1;
};

int bps_magic_cookie_cmp(bps_magic_cookie_t* id_to,
                         bps_magic_cookie_t* id_from){
   return *id_to > *id_from;
};
   
int bps_sspartid_equal(bps_sspartition_id_t* id_to,
                       bps_sspartition_id_t* id_with)
{
   uint_t status;
   return uuid_equal(id_to,id_with, &status);
}

int bps_sspartid_greaterthan(bps_sspartition_id_t* id_to,
                             bps_sspartition_id_t* id_with)
{
   uint_t status;
   if(uuid_compare(id_to,id_with,&status) == -1){
      return 1;
   }
   return 0;
}

void bps_error_print(error_t error){
   switch(error){
   case BPS_ERR_OK:
      fprintf(stderr,"No error!\n");
      break;
   case BPS_ERR_BAD_MSG_TYPE:
      fprintf(stderr,"Bad message type.\n");
      break;
   case BPS_ERR_INVALID_TIME_INTERVAL:
      fprintf(stderr,"The time interval specified is invalid.\n");
      break;
   case BPS_ERR_COULD_NOT_CREATE_SSPART:
      fprintf(stderr,"The ssp could not be created.\n");
      break;
   case BPS_ERR_MEMORY_USED:
      fprintf(stderr,"The memory to be modified is being used.\n");
      break;
   case BPS_ERR_CPU_USED:
      fprintf(stderr,"The cpus to be modified are being used.\n");
      break;
   case BPS_ERR_BAD_POLICY:
      fprintf(stderr,"The policy id is invalid.\n");
      break;
   case BPS_ERR_TOO_MANY_THREADS:
      fprintf(stderr,"Too many threads specified.\n");
      break;
   case BPS_ERR_BAD_SSPART_ID:
      fprintf(stderr,"The ssp id is invalid.\n");
      break;
   case BPS_ERR_OUT_OF_MEMORY:
      fprintf(stderr,"The ssp or system is out of memory.\n");
      break;
   case BPS_ERR_INVALID_TIME:
      fprintf(stderr,"The time specified is invalid.\n");
      break;
   case BPS_ERR_BAD_SOCKET:
      fprintf(stderr,"The socket is bad.\n");
      break;
   case BPS_ERR_BAD_READ:
      fprintf(stderr,"Could not read from the socket.\n");
      break;
   case BPS_ERR_BAD_WRITE:
      fprintf(stderr,"Could not write to the socket.\n");
      break;
   case BPS_ERR_NO_MATCH:
      fprintf(stderr,"Could not find an allocation.\n");
      break;
   case BPS_ERR_NO_JOB:
      fprintf(stderr,"No job could be created from the allocation.\n");
      break;
   case BPS_ERR_WINDOW_TOO_SMALL:
      fprintf(stderr,"Desired window is too small.\n");
      break;
   case BPS_ERR_NOT_ENOUGH_CPUS:
      fprintf(stderr,"Not enough cpus could be found to allocate.\n");
      break;
   case BPS_ERR_BAD_MSG:
      fprintf(stderr,"The message received is corrupted.\n");
      break;
   case BPS_ERROR:
      fprintf(stderr,"An error occured.\n");
      break;
   case BPS_ERR_WINDOW_TOO_LARGE:
      fprintf(stderr,"Window specified too large.\n");
      break;
   default:
      fprintf(stderr,"UNKOWN ERROR.\n");
      break;
   }
   return;
};
         
         
      
void bps_partitionids_alloc(partition_ids_t** array, int size){
   *array = (partition_ids_t*)malloc(sizeof(partition_ids_t)*size  -1);
   (*array)->size  = size;
};
void bps_cpusused_alloc(cpus_used_t** array, int size){
   *array = (cpus_used_t *) malloc(sizeof(cpus_used_t)*size -1);
   (*array)->size  = size;
};
void bps_memoryused_alloc(memory_used_t** array, int size){
   *array = (memory_used_t*)malloc(sizeof(memory_used_t)*size-1);
   (*array)->size  = size;
};
      
void bps_inforeply_free(bps_info_reply_t** reply){
   free((*reply)->memory_usage);
   free((*reply)->cpu_usage);
   free((*reply)->partition_ids);
   free(*reply);
};
