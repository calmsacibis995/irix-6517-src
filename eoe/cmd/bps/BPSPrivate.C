/****************************************************************
 *    NAME: 
 *    ACCT: kostadis
 *    FILE: BPSPrivate.C
 *    DATE: Mon Jul 10 17:50:15 PDT 1995
 ****************************************************************/

#include "BPSPrivate.H"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <Debug.H>
#include <malloc.h>
#include <BPS_lib.H>


error_t bps_messageserver_send(int sockfd, bps_message_t* msg){
   error_t err;
   if(write(sockfd,(void*) &msg->msg,sizeof(msg->msg)) < 0){
      return BPS_ERR_BAD_WRITE;
   };
   switch(msg->msg){
   case BPS_MSG_QUERY:
      err = bps_inforeply_send(sockfd,&msg->bps_msg_data.info_reply);
      return err;
   case BPS_MSG_CANCEL:
   case BPS_MSG_SUBMIT:
   case BPS_MSG_ERROR:
      err = bps_reply_send(sockfd,&msg->bps_msg_data.reply);
      return err;
   case BPS_MSG_SYSTEM:
      err = bps_sysmsg_send(sockfd,&msg->bps_msg_data.sys_msg);
   default:
      return BPS_ERR_BAD_WRITE;
   }
};
error_t bps_messageclient_recv(int sockfd, bps_message_t* msg)
{
   error_t err;
   if(read(sockfd,&msg->msg,sizeof(msg->msg)) < 0){
      return BPS_ERR_BAD_READ;
   }
   switch(msg->msg){
   case BPS_MSG_QUERY:
      err = bps_inforeply_recv(sockfd,&msg->bps_msg_data.info_reply);
      return err;
   case BPS_MSG_CANCEL:
   case BPS_MSG_SUBMIT:
   case BPS_MSG_ERROR:
      err = bps_reply_recv(sockfd,&msg->bps_msg_data.reply);
   case BPS_MSG_SYSTEM:
      err = bps_sysmsg_recv(sockfd,&msg->bps_msg_data.sys_msg);
   default:
      return BPS_ERR_BAD_READ;
   }
};
error_t bps_inforeply_send(int sockfd, bps_info_reply_t* info_reply)
{
   int size_of_block = 0;
   size_of_block += sizeof(info_reply->start);
   size_of_block += sizeof(info_reply->end);
   size_of_block += sizeof(info_reply->cpu_usage->size);
   size_of_block += (sizeof(info_reply->cpu_usage->cpu_usage[0]) * info_reply->cpu_usage->size);
   size_of_block += sizeof(info_reply->memory_usage->size);
   size_of_block += (sizeof(info_reply->memory_usage->memory_usage[0]) * info_reply->memory_usage->size);
   size_of_block += sizeof(info_reply->partition_ids->size);
   size_of_block += (sizeof(info_reply->partition_ids->partition_ids[0]) * info_reply->partition_ids->size);
   
   char* block = (char*) malloc(sizeof(char) * size_of_block);
   char* ptr = block;
   memcpy(ptr,&info_reply->start, sizeof(info_reply->start));
   ptr += sizeof(info_reply->start);
   memcpy(ptr,&info_reply->end,sizeof(info_reply->end));
   ptr += sizeof(info_reply->end);
   memcpy(ptr,&info_reply->cpu_usage->size,sizeof(info_reply->cpu_usage->size));
   ptr += sizeof(info_reply->cpu_usage->size);
   memcpy(ptr,info_reply->cpu_usage->cpu_usage,sizeof(info_reply->cpu_usage->cpu_usage[0]) * info_reply->cpu_usage->size);
   ptr += sizeof(info_reply->cpu_usage->cpu_usage[0]) * info_reply->cpu_usage->size;
   memcpy(ptr,&info_reply->memory_usage->size,sizeof(info_reply->memory_usage->size));
   ptr += sizeof(info_reply->memory_usage->size);
   memcpy(ptr,info_reply->memory_usage->memory_usage,sizeof(info_reply->memory_usage->memory_usage[0]) * info_reply->memory_usage->size);
   ptr += sizeof(info_reply->memory_usage->memory_usage[0]) * info_reply->memory_usage->size;
   memcpy(ptr,&info_reply->partition_ids->size,sizeof(info_reply->partition_ids->size));
   ptr += sizeof(info_reply->partition_ids->size);
   memcpy(ptr,info_reply->partition_ids->partition_ids,sizeof(*info_reply->partition_ids->partition_ids)* info_reply->partition_ids->size);
   if(write(sockfd,(void*)&size_of_block,sizeof(size_of_block)) < 0){
      free(block);
      return BPS_ERR_BAD_WRITE;
   }
   if(write(sockfd,(void*)block,size_of_block) < 0){
      free(block);
      return BPS_ERR_BAD_WRITE;
   }
   free(info_reply->partition_ids);
   free(info_reply->memory_usage);
   free(info_reply->cpu_usage);
   return BPS_ERR_OK;
}

error_t bps_inforeply_recv(int sockfd, bps_info_reply_t* reply)
{
   int size_of_block;
   if(read(sockfd,(void*)&size_of_block,sizeof(size_of_block)) < 0){
      return BPS_ERR_BAD_READ;
   }
   char* block = (char*) malloc(sizeof(char) * size_of_block);
   char* ptr = block;
   if(read(sockfd,(void*)block,size_of_block) <0){
      free(block);
      return BPS_ERR_BAD_WRITE;
   }
   memcpy(&reply->start, ptr,sizeof(reply->start));
   ptr += sizeof(reply->start);
   memcpy(&reply->end,ptr,sizeof(reply->end));
   ptr += sizeof(reply->end);
   int size;
   memcpy(&size,ptr,sizeof(reply->cpu_usage->size));
   ptr += sizeof(size);
   bps_cpusused_alloc(&reply->cpu_usage,size);
   memcpy(reply->cpu_usage->cpu_usage,ptr,sizeof(reply->cpu_usage->cpu_usage[0]) * reply->cpu_usage->size);
   ptr += sizeof(reply->cpu_usage->cpu_usage[0]) * reply->cpu_usage->size;

   memcpy(&size,ptr,sizeof(size));
   ptr += sizeof(size);
   bps_memoryused_alloc(&reply->memory_usage,size);
   memcpy(reply->memory_usage->memory_usage,ptr,sizeof(reply->memory_usage->memory_usage[0]) * reply->memory_usage->size);
   ptr += sizeof(reply->memory_usage->memory_usage[0]) * reply->memory_usage->size;

   memcpy(&size,ptr,sizeof(size));
   ptr += sizeof(size);
   bps_partitionids_alloc(&reply->partition_ids, size);
   memcpy(reply->partition_ids->partition_ids,ptr,sizeof(*reply->partition_ids->partition_ids)* reply->partition_ids->size);

   free(block);
   return BPS_ERR_OK;
}

error_t bps_reply_send(int sockfd, bps_reply_t* reply)
{
   if(write(sockfd,(void*) reply, sizeof(*reply))< 0){
      return BPS_ERR_BAD_WRITE;
   };
   return BPS_ERR_OK;
}

error_t bps_reply_recv(int sockfd, bps_reply_t* reply)
{
   if(read(sockfd,(void*) reply, sizeof(*reply)) <0){
      return BPS_ERR_BAD_READ;
   }
   return BPS_ERR_OK;
};

error_t bps_sysmsg_send(int sockfd, bps_system_message_t* msg)
{
   error_t err;
   if(write(sockfd,(void*)&msg->msg,sizeof(msg))< 0){
      return BPS_ERR_OK;
   }
   switch(msg->msg){
   case BPS_SYS_SSPART_CREATE:
      err = bps_partinit_send(sockfd,msg);
      return err;
   case BPS_SYS_SSPART_DELETE:
      err = bps_partinit_send(sockfd,msg);
      return err;
   case BPS_SYS_THREAD_KILL:
      err = bps_threadmod_send(sockfd,msg);
      return err;
   case BPS_SYS_THREAD_CREATE:
      err = bps_threadmod_send(sockfd,msg);
      return err;
   case BPS_SYS_CHANGE_DEFAULT:
      err = bps_changedefault_send(sockfd,msg);
      return err;
   case BPS_SYS_MODIFY_PARTITION:
      err = bps_partmod_send(sockfd,msg);
   case BPS_SYS_CHANGE_PERMISSIONS:
      err = bps_permmsg_send(sockfd,msg);
   default:
      return BPS_ERROR;
   }
};
error_t bps_sysmsg_recv(int sockfd, bps_system_message_t* msg){
   error_t err;
   if(read(sockfd,(void*)&msg->msg,sizeof(msg))< 0){
      return BPS_ERR_OK;
   }
   switch(msg->msg){
   case BPS_SYS_SSPART_CREATE:
      err = bps_partinit_recv(sockfd,msg);
      return err;
   case BPS_SYS_SSPART_DELETE:
      err = bps_partinit_recv(sockfd,msg);
      return err;
   case BPS_SYS_THREAD_KILL:
      err = bps_threadmod_recv(sockfd,msg);
      return err;
   case BPS_SYS_THREAD_CREATE:
      err = bps_threadmod_recv(sockfd,msg);
      return err;
   case BPS_SYS_CHANGE_DEFAULT:
      err = bps_changedefault_recv(sockfd,msg);
      return err;
   case BPS_SYS_MODIFY_PARTITION:
      err = bps_partmod_recv(sockfd,msg);
   case BPS_SYS_CHANGE_PERMISSIONS:
      err = bps_permmsg_recv(sockfd,msg);
   default:
      return BPS_ERROR;
   }
}   
error_t bps_partinit_send(int sockfd, bps_system_message_t* msg)
{
   int string_size = strlen(msg->bps_msg_data.init_data.filename) + 1;
   int size = strlen(msg->bps_msg_data.init_data.filename) + 1;
   size += sizeof(msg->bps_msg_data.init_data.type_id)* 2;
   size += sizeof(string_size);
   char* block = (char*) malloc(sizeof(char) * size);
   symp(size);
   char *ptr = block;
   memcpy(ptr,&msg->bps_msg_data.init_data.type_id, sizeof(msg->bps_msg_data.init_data.type_id));
   ptr += sizeof(msg->bps_msg_data.init_data.type_id);
   memcpy(ptr,&msg->bps_msg_data.init_data.part_id, sizeof(msg->bps_msg_data.init_data.part_id));
   ptr += sizeof(msg->bps_msg_data.init_data.type_id);
   memcpy(ptr,&string_size, sizeof(string_size));
   ptr += sizeof(string_size);
   symp(string_size);
   memcpy(ptr,msg->bps_msg_data.init_data.filename,strlen(msg->bps_msg_data.init_data.filename) + 1);
   symp(ptr);
   if(write(sockfd,&size, sizeof(size)) < 0){
      free(block);
      return BPS_ERR_BAD_WRITE;
   }
   if(write(sockfd,block,size) < 0){
      free(block);
      return BPS_ERR_BAD_WRITE;
   } 
   free(block);
   return BPS_ERR_OK;
}

error_t bps_partinit_recv(int sockfd, bps_system_message_t* msg)
{
   int size;
   int string_size;
   if(read(sockfd,&size, sizeof(size)) < 0){
      return BPS_ERR_BAD_READ;
   }
   char* block = (char*) malloc(sizeof(char) * size);
   char *ptr = block;
   if(read(sockfd,block,size) < 0){
      free(block);
      return BPS_ERR_BAD_READ;
   } 
   memcpy(&msg->bps_msg_data.init_data.type_id, ptr,sizeof(msg->bps_msg_data.init_data.type_id));
   ptr += sizeof(msg->bps_msg_data.init_data.type_id);
   memcpy(&msg->bps_msg_data.init_data.part_id, ptr,sizeof(msg->bps_msg_data.init_data.part_id));
   ptr += sizeof(msg->bps_msg_data.init_data.type_id);
   memcpy(&string_size,ptr,sizeof(string_size));
   symp(string_size);
   ptr += sizeof(string_size);
   msg->bps_msg_data.init_data.filename = (char*) malloc(sizeof(char) * string_size);
   memcpy(msg->bps_msg_data.init_data.filename,ptr,string_size);
   symp(   msg->bps_msg_data.init_data.filename);
   free(block);
   return BPS_ERR_OK;
}

error_t bps_threadmod_send(int sockfd, bps_system_message_t* msg){
   if(write(sockfd,(void*) &msg->bps_msg_data.num_threads,sizeof(msg->bps_msg_data.num_threads)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   return BPS_ERR_OK;
}
error_t bps_threadmod_recv(int sockfd, bps_system_message_t* msg){
   if(read(sockfd,(void*) &msg->bps_msg_data.num_threads,sizeof(msg->bps_msg_data.num_threads)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   return BPS_ERR_OK;
}
error_t bps_changedefault_send(int sockfd, bps_system_message_t* msg){
   if(write(sockfd,(void*) &msg->bps_msg_data.uuid,sizeof(msg->bps_msg_data.uuid)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   return BPS_ERR_OK;
}
error_t bps_changedefault_recv(int sockfd, bps_system_message_t* msg){
   if(read(sockfd,(void*) &msg->bps_msg_data.uuid,sizeof(msg->bps_msg_data.uuid)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   return BPS_ERR_OK;
}
error_t bps_partmod_send(int sockfd, bps_system_message_t* msg){
   int size = sizeof(msg->bps_msg_data.tune_data.part_id);
   size += msg->bps_msg_data.tune_data.size;
   size += sizeof(msg->bps_msg_data.tune_data.size);
   char* block = new char[size];
   char* ptr = block;
   memcpy(ptr,&msg->bps_msg_data.tune_data.part_id,sizeof(msg->bps_msg_data.tune_data.part_id));
   ptr +=sizeof(msg->bps_msg_data.tune_data.part_id);
   memcpy(ptr,&msg->bps_msg_data.tune_data.size,sizeof(msg->bps_msg_data.tune_data.size));
   ptr += sizeof(msg->bps_msg_data.tune_data.size);
   memcpy(ptr,msg->bps_msg_data.tune_data.data,msg->bps_msg_data.tune_data.size);
   
   if(write(sockfd,&size,sizeof(size)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   if(write(sockfd,block,size) <0){
      return BPS_ERR_BAD_WRITE;
   }
   return BPS_ERR_OK;
}

error_t bps_partmod_recv(int sockfd, bps_system_message_t* msg){
   int size;
   if(read(sockfd,&size,sizeof(size)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   char* block = new char[size];
   char* ptr = block;
   if(read(sockfd,block,size) <0){
      return BPS_ERR_BAD_WRITE;
   }
   memcpy(&msg->bps_msg_data.tune_data.part_id,ptr,sizeof(msg->bps_msg_data.tune_data.part_id));
   ptr +=sizeof(msg->bps_msg_data.tune_data.part_id);
   memcpy(&msg->bps_msg_data.tune_data.size,ptr,sizeof(msg->bps_msg_data.tune_data.size));
   ptr += sizeof(msg->bps_msg_data.tune_data.size);
   msg->bps_msg_data.tune_data.data = (char*) malloc(sizeof(char) * size);
   memcpy(msg->bps_msg_data.tune_data.data,ptr,msg->bps_msg_data.tune_data.size);
   return BPS_ERR_OK;
}
error_t bps_permmsg_send(int sockfd, bps_system_message_t* msg){ 
   int size;
   size =sizeof(msg->bps_msg_data.perm_msg.part_id);
   size += sizeof(msg->bps_msg_data.perm_msg.msg);
   size += msg->bps_msg_data.perm_msg.data.size;
   size += sizeof(msg->bps_msg_data.perm_msg.data.size);
   char* block = new char[size];
   char* ptr = block;
   memcpy(ptr,&msg->bps_msg_data.perm_msg.part_id,sizeof(msg->bps_msg_data.perm_msg.part_id));
   ptr+=sizeof(msg->bps_msg_data.perm_msg.part_id);
   memcpy(ptr,&msg->bps_msg_data.perm_msg.msg,sizeof(msg->bps_msg_data.perm_msg.msg));
   ptr+=sizeof(msg->bps_msg_data.perm_msg.msg);
   memcpy(ptr,&msg->bps_msg_data.perm_msg.data.size,sizeof(msg->bps_msg_data.perm_msg.data.size));
   ptr += sizeof(msg->bps_msg_data.perm_msg.data.size);
   memcpy(ptr,msg->bps_msg_data.perm_msg.data.data,msg->bps_msg_data.perm_msg.data.size);
   
   if(write(sockfd,&size,sizeof(size)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   if(write(sockfd,block,size) <0){
      return BPS_ERR_BAD_WRITE;
   }
   return BPS_ERR_OK;
}  
error_t bps_permmsg_recv(int sockfd, bps_system_message_t* msg){ 
   int size;
   if(read(sockfd,&size,sizeof(size)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   char* block = new char[size];
   char* ptr = block;

   if(read(sockfd,block,size) <0){
      return BPS_ERR_BAD_WRITE;
   }
   size =sizeof(msg->bps_msg_data.perm_msg.part_id);
   size += sizeof(msg->bps_msg_data.perm_msg.msg);
   size += msg->bps_msg_data.perm_msg.data.size;
   size += sizeof(msg->bps_msg_data.perm_msg.data.size);
   memcpy(&msg->bps_msg_data.perm_msg.part_id,ptr,sizeof(msg->bps_msg_data.perm_msg.part_id));
   ptr+=sizeof(msg->bps_msg_data.perm_msg.part_id);
   memcpy(&msg->bps_msg_data.perm_msg.msg,ptr,sizeof(msg->bps_msg_data.perm_msg.msg));
   ptr+=sizeof(msg->bps_msg_data.perm_msg.msg);
   memcpy(&msg->bps_msg_data.perm_msg.data.size,ptr,sizeof(msg->bps_msg_data.perm_msg.data.size));
   ptr += sizeof(msg->bps_msg_data.perm_msg.data.size);
   msg->bps_msg_data.perm_msg.data.data = (char*) malloc (sizeof(char) *msg->bps_msg_data.perm_msg.data.size);
   memcpy(msg->bps_msg_data.perm_msg.data.data,ptr,msg->bps_msg_data.perm_msg.data.size);
   return BPS_ERR_OK;
}  
error_t bps_messageclient_send(int sockfd, bps_message_t* msg){
   error_t err;
   if(write(sockfd,&msg->msg,sizeof(msg->msg)) < 0){
      return BPS_ERR_BAD_WRITE;
   }
   switch(msg->msg){
   case BPS_MSG_QUERY:
      err = bps_inforeq_send(sockfd, &msg->bps_msg_data.info);
      return err;
   case BPS_MSG_CANCEL:
   case BPS_MSG_SUBMIT:
      err = bps_request_send(sockfd, &msg->bps_msg_data.req);
      return err;
   case BPS_MSG_SYSTEM:
      err = bps_sysmsg_send(sockfd,&msg->bps_msg_data.sys_msg);
      return err;
   default:
      return BPS_ERR_BAD_WRITE;
   };
};

error_t bps_messageserver_recv(int sockfd, bps_message_t* msg){
   error_t err;
   if(read(sockfd,&msg->msg,sizeof(msg->msg)) <0){
      return BPS_ERR_BAD_READ;
   };
   switch(msg->msg){
   case BPS_MSG_QUERY:
      err = bps_inforeq_recv(sockfd, &msg->bps_msg_data.info);
      return err;
   case BPS_MSG_CANCEL:
   case BPS_MSG_SUBMIT:
      err = bps_request_recv(sockfd, &msg->bps_msg_data.req);
      return err;
   case BPS_MSG_SYSTEM:
      err = bps_sysmsg_recv(sockfd,&msg->bps_msg_data.sys_msg);
      return err;
   default:
      return BPS_ERR_BAD_WRITE;
   }
}
      
   
error_t bps_inforeq_send(int sockfd, bps_info_request_t* msg){
   if(write(sockfd,(void*) msg,sizeof(*msg)) < 0){
      return BPS_ERR_BAD_WRITE;
   };
   return BPS_ERR_OK;
};
      
   
error_t bps_inforeq_recv(int sockfd, bps_info_request_t* msg){
   if(read(sockfd,(void*) msg,sizeof(*msg)) < 0){
      return BPS_ERR_BAD_WRITE;
   };
   return BPS_ERR_OK;
};
error_t bps_request_send(int sockfd, bps_request_t* msg){
   if(write(sockfd,(void*) msg,sizeof(*msg)) < 0){
      return BPS_ERR_BAD_WRITE;
   };
   return BPS_ERR_OK;
};
error_t bps_request_recv(int sockfd, bps_request_t* msg){
   if(read(sockfd,(void*) msg,sizeof(*msg)) < 0){
      return BPS_ERR_BAD_WRITE;
   };
   return BPS_ERR_OK;
}   
