
#include <sys/types.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <iostream.h>
#include "knrGangMems.H"
#include "knrTimer.H"
#include "knrGang.H"
#include "knrTestSystem.H"
knrGang::knrGang(int gang_member,
		 int gang_id,
		 knrGangMaster::Mode mode,
		 knrOutput* output)
:output_(output),gang_id_(gang_id)
{
  master_ = new knrGangMaster(gang_member,mode);
};
knrGang::~knrGang(){delete master_;};

int knrGang::start(int iterations, knrOutput* output){
  pid_t process;
  process = fork();
  if(process == 0){
    prctl(PR_TERMCHILD);
    char buf[BUFSIZ];
    sprintf(buf,"total time for gang %d of size %d",gang_id_,master_->NumMems());
    knrTestTime start(output_,buf);
    
    for(int i = 0; i < master_->NumMems(); i++)
      master_->InitMember(new ItGangMember(i,gang_id_,iterations,output), i);
    master_->start();

    master_->wait();
    start.Stop();
    start.output();
    exit(1);
    // child should never get here
  }
  return process;
}


