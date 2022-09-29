#include <stdlib.h>
#include <stdio.h>
#include <bps.h>
#include <BPSServer.H>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <BPSSSPartManager.H>
#include <stl.h>

extern char* optarg;
extern int optind, opterr, optopt;

#define TYPE_ID 0
#define SSPART_ID 1
#define FILE_NAME 2
#define NO_MATCH -1
char * myopts[] = {
                "tid",
                "sspid",
                "fn", 
                NULL};

extern "C" void uuid_from_string(char* uuidstr, uuid_t *uuid, uint_t * status);
extern "C" struct hostent *gethostbyname(const char *name);
extern "C" int gethostname(char* name, int namelen);
#define uuid_s_ok 0
sproc_t sid6,sid1;
static BPSSSPartition mypart(BPS_POLICY);
int main(int argc, char** argv){
   sproc_create(&sid1,NULL,NULL,NULL);
   sproc_create(&sid6,NULL,NULL,NULL);
   list <bps_sspartinit_data_t> data_list;
   int c;
   ncpu_t cpus;
   memory_t  memory;
   struct sockaddr_in serv_addr;
   struct hostent *hostptr;
   int server_threads;
   int port_number = BPS_DFLT_PORT;
   char * options;
   char * value;
   bps_sspartdata_t data;
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   uint_t status;
   while((c = getopt(argc,argv,"C:M:I:D:P:T:S:p:")) != EOF){
      switch(c){
      case 'C':
         sscanf(optarg,"%d",&data.ncpus);
         break;
      case 'M':
         sscanf(optarg,"%d",&data.memory);
         break;
      case 'I':
         sscanf(optarg,"%d",&data.interval);
         break;
      case 'D':
         sscanf(optarg,"%d",&data.duration);
         break;
      case 'P':
         sscanf(optarg,"%d",&port_number);
         serv_addr.sin_port = htons(port_number);
         break;
      case 'T':
         sscanf(optarg,"%d",&server_threads);
         break;
      case '?':
         exit(0);
      case 'p':
         options = optarg;
         bps_sspartinit_data_t init_data;
         while(*options != '\0'){
            switch(getsubopt(&options,myopts,&value)){
            case TYPE_ID:
               if(value == NULL){
                  fprintf(stderr,"No type id supplied.\n");
                  exit(1);
               }else{
                  uuid_from_string(value,&init_data.type_id,&status);
                  if(status != uuid_s_ok){
                     fprintf(stderr, "Bad uuid supplied.\n");
                     exit(1);
                  }
               }
               break;
            case SSPART_ID:
               if(value == NULL){
                  fprintf(stderr,"No partition id supplied.\n");
                  exit(1);
               }else {
                  uuid_from_string(value,&init_data.part_id,&status);
                  if(status != uuid_s_ok){
                     fprintf(stderr, "Bad uuid supplied.\n");
                     exit(1);
                  }
               }
               break;
            case FILE_NAME:
               if(value == NULL){
                  fprintf(stderr,"No input file supplied. \n");
                  exit(1);
               }else{
                  init_data.filename = new char[strlen(value) + 1];
                  strcpy(init_data.filename,value);
                  break;
               }
            case NO_MATCH:
               fprintf(stderr,"Invalid token supplied.\n");
               exit(1);
            }
         }
         data_list.push_front(init_data);
         
      }
   }
   bps_sspartinit_data_t d = data_list.front();
   error_t err;
   BPSServer server(data,server_threads,(caddr_t) &serv_addr);
   err = server.SSPartitionAdd(d);
   if(err != BPS_ERR_OK){
      fprintf(stderr,"ill will gone down.\n");
      exit(1);
   };
   server.start();
   while(1);
}
         
            

   
   






