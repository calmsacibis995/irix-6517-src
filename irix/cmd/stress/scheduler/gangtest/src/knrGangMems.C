#include "knrGangMems.H"
#include "knrTestSystem.H"
#include <stdio.h>
#include <ulocks.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/param.h>
#include <sys/schedctl.h>
#include <unistd.h>
#include <assert.h>
#include "knrTimer.H"
#include <math.h>
extern "C" void wait(int* status);
knrGangMaster::knrGangMaster(int num_mems, knrGangMaster::Mode mode){
   if(mode == knrGangMaster::SINGLE){
      master_ = new knrGangSingleMaster(mode,num_mems);
   }
   else 
      master_ = new knrGangMultiMaster(num_mems,mode);

}
void knrGangMaster::InitMember(knrGangMember* member,int index){
   master_->InitMember(member,index);
};
void knrGangMaster::start()
{
   master_->start();
};
void knrGangMaster::wait(){
   master_->wait();
};
int knrGangMaster::SetMode(knrGangMaster::Mode mode){
   return master_->SetMode(mode);
};
   
int knrGangMaster::NumMems() const {
   return master_->NumMems();
}

knrGangRealMaster::knrGangRealMaster(knrGangMaster::Mode mode, int num_mems)
:mode_(mode),num_mems_(num_mems){;}

knrGangMultiMaster::knrGangMultiMaster(int num_mems, knrGangMaster::Mode mode) 
:knrGangRealMaster(mode,num_mems){
  gang_members_ = new knrGangMember*[num_mems_];
};

int knrGangRealMaster::SetMode(knrGangMaster::Mode mode){
  if(schedctl(SCHEDMODE, (int) mode) == -1){
     fprintf(stderr,
	     "bad happenings with schedmode\n");
     exit(1);
   };
  return 0;
};
void knrGangMultiMaster::start(){

  for(int i = 0; i < num_mems_; i++){
    sproc(knrGangMember::start,PR_SALL,gang_members_[i]);
  };
  SetMode(knrGangMaster::SINGLE);
  SetMode( mode_);  
};

void knrGangMultiMaster::wait(){
  int rem_mems= 0;
  for(int k = 0;  k <num_mems_; k++){
   if(gang_members_[k] == 0)
      continue;
   rem_mems++;
 }
  G_dbg_output->printf(" rem mems %d \n", rem_mems);
  for(int i = 0; i < rem_mems; i++){
    int status = 0;
    ::wait(&status);
 };
};

void knrGangMultiMaster::InitMember(knrGangMember* member, int i){

  assert (i >= 0 && i < num_mems_);
  gang_members_[i] = member;
};

int knrGangMultiMaster::terminate(int thread){
  assert(thread >= 0 && thread < num_mems_);
  if(gang_members_[thread] == 0) {
    return -1;
  }
  else{
    
    gang_members_[thread]->terminate();
    delete gang_members_[thread];
    gang_members_[thread] =0;
    return 1;
  };
};

knrGangSingleMaster::knrGangSingleMaster(knrGangMaster::Mode mode,
                                         int num_mems)
:knrGangRealMaster(mode,num_mems){
 gang_member_ = 0;
};

void knrGangSingleMaster::start(){
  assert(gang_member_ != 0);
  knrGangMember::start(gang_member_);
};

int knrGangSingleMaster::terminate(int thread){
	return 1;
}
void knrGangSingleMaster::wait(){
  return;
}

void knrGangSingleMaster::InitMember(knrGangMember* member, int index){
  assert(index == 0);
  gang_member_ = member;
};


 
knrGangMember::knrGangMember(int id,int gang_id):gang_id_(gang_id),id_(id){;}

void knrGangMember::start(void* thisp){
  ((knrGangMember*) thisp)->work_func();
  G_dbg_output->printf("got to here\n");
}

ItGangMember::ItGangMember(int id, int gang_id, int num, 
			   knrOutput* output):knrGangMember(id,gang_id),num_(num),
output_(output)

{;}

void ItGangMember::work_func(){
  int k = 0;
  double c = 0.0;
  char buf[BUFSIZ];
  sprintf(buf,"thread %d finished of gang %d , taking :",id_,gang_id_);
  knrTestTime a(output_,buf);
  for(int l = 0; l < 5; l++){
    for(int i = 0; i < num_; i++){
      for(k = 0; k < num_*num_; k++)
	c = tan(cos(sin(log ((double) k) * atan((double) i)) * cos(k*k)));
      
    };
  }
 a.Stop();

 a.output();
}
  

void ItGangMember::terminate(){
  exit(0);
}

