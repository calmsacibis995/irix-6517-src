#include <iostream.h>
#include <fstream.h>
#include <iomanip.h>
#include <string.h>
#include <stdio.h>
#include "knrTestSystem.H"
#include <assert.h>
typedef struct _knrThread{
   int 
id;
   int gang_num;
   float time;
   _knrThread(int gang_num_ = -1, int time_ = -1):
   id(-1),gang_num(gang_num_),time(time_){;}
} knrThread_t;

typedef struct _knrGang {
   int id_;
   int num_threads_;
   float time_;
   _knrGang(int id = -1,int num_threads = -1, float time = -1.0):
   id_(id),num_threads_(num_threads),time_(time){;}
} knrGang_t;
const int MAXTHREADS = 1000; // this should be enough for now...
const int MAXGANGS = 1000;
int max_threads = 0;
int max_gangs = 0;
int max_g_gangs = 0;
int max_g_threads = 0;
int main(int argc, char** argv)
{
   
  if(argc < 2){
     cerr << "No input file specified" << endl;
     exit(1);
  }
  ifstream f(argv[1]);
  if(!f){
     cerr << "Could not open input file!" << endl;
     exit(1);
  };
  

  knrOutput* gang_out = G_msg_output;
  knrOutput* thread_out = G_msg_output;
  if(argc > 2){
     gang_out = new knrFileOutput(argv[2],"");
  }
  if(argc == 4){
     thread_out = new knrFileOutput(argv[3],"");
  };
  if(argc > 4){
     G_err_output->printf("Extra parameters ignored");
  }
  int gang_num;

  knrThread_t thread[MAXTHREADS][MAXGANGS];
  knrGang_t gangs[MAXGANGS];
  int num_gangs;


  float time_r;
  char buf[1024 * 1024]; // a really large buffer so just in case

                         // we never go over.
     char *bufptr = buf;
  f >> bufptr;
  f >> bufptr;
  f >> bufptr;
  f >> bufptr;
  f >> bufptr;
 assert(strcmp(bufptr,"gangs:") == 0);
  f >> bufptr;
  sscanf(bufptr,"%d",&max_g_gangs);
  
 
  while(!f.eof()){

     f >> bufptr;
     if(strcmp(bufptr,"gangs") == 0){
        f>> bufptr;
        sscanf(bufptr,"%d",&num_gangs);
     };
     // parse thread data
        int id;
     if(strcmp(bufptr,"thread") == 0){
        f>> bufptr;

        sscanf(bufptr,"%d",&id);

        
        f >> bufptr; // getting 'finished'
        f >> bufptr; // getting 'of'
        f >> bufptr; // getting 'gang'
        f >> bufptr; // got the gang of thread
        int gang_num;
        sscanf(bufptr,"%d",&gang_num);
        assert(gang_num < MAXGANGS);
        assert(id< MAXTHREADS);
        if(id > max_threads) {
           max_threads  = id;
        }
        if(gang_num > max_gangs){
           max_gangs = gang_num;
        }
        thread[id][gang_num].id = id;
        thread[id][gang_num].gang_num = gang_num;
        while(strcmp(bufptr,"diff") != 0){
           f >> bufptr;
        }
        f>> bufptr; // got the time
        sscanf(bufptr,"%f",&thread[id][gang_num].time);
     }
     
     if(strcmp(bufptr,"total") == 0){ // hit a gang total line
        while(strcmp(bufptr,"gang") != 0){
           f>> bufptr;
        }           
        f >> bufptr; // read in the gang number
        sscanf(bufptr,"%d",&gang_num);
        assert(gang_num < MAXGANGS); // a bit of sanity checking
        gangs[gang_num].id_ = gang_num;
        if(gang_num > max_g_gangs){
           max_g_gangs = gang_num;
        }
        
        f >> bufptr; // read in 'of'
        f >> bufptr; // read in 'size'
        assert(strcmp (bufptr,"size") == 0);
        f >> bufptr;
        int size;
        sscanf(bufptr, "%d", &size);
        assert(size < MAXTHREADS);
        gangs[gang_num].num_threads_ = size;
        if(size > max_g_threads){
           max_g_threads = size;
        }
        while(strcmp(bufptr,"diff") != 0){
           f>> bufptr;
        }
        f >> bufptr; // read in the time
        sscanf(bufptr,"%f",&gangs[gang_num].time_);
        assert(gangs[gang_num].time_ >= 0); // a little bit of sanity checking
        
     }
     if(strcmp(bufptr,"diff") == 0){
        f >> bufptr;
        cout << bufptr << endl;
        sscanf(bufptr,"%f",&time_r);
     }
     

     if(bufptr > (buf + 1024*1024)){
          assert(1==0);
       }// just to make sure we never beyond the end.
  };
  int i;
  assert (max_g_threads == max_threads +1); // need to add one 
                                            // because threads start at 
  float total =0;
  for(int k = 0; k < max_g_gangs; k++){
     if((gangs[k].id_ == -1) ||
        (gangs[k].num_threads_ == -1) ||
        (gangs[k].time_ == -1)){
        gang_out->printf("%d corrupted!",k);
        continue;
     }
     gang_out->printf("gangs %d %d %5.5f",gangs[k].id_,gangs[k].num_threads_,gangs[k].time_);
  }
  for(k = 0; k < max_g_gangs; k++){    
     for(i = 0; i < max_threads +1 ; i++) {
       thread_out->printf("threads %d %d %5.5f ",thread[i][k].id,
                          thread[i][k].gang_num, thread[i][k].time);
  }
  };
  return 1;
}
