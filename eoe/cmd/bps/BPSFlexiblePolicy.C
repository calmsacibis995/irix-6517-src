/****************************************************************
*    NAME:                                                     
*    ACCT: kostadis                                            
*    FILE: BPSDynamicPolicy.C                                  
*    ASGN:                                                     
*    DATE: Mon Jul 17 11:08:45 1995                            
****************************************************************/



#include "BPSDynamicPolicy.H"
#include <bps.h>
#include <BPS_lib.H>
#include <BPSPolicy.H>
#include <BPSSSPartition.H>
#include <BPSJob.H>
#include <BPSSSPartitionUnit.H>
BPSDynamicPolicy::BPSDynamicPolicy(const bps_policy_id_t& id)
:BPSPolicy(id){;}

BPSDynamicPolicy::BPSDynamicPolicy(bps_request_t* request,BPSSSPartition* partition)
:BPSPolicy(request,partition){;};

BPSDynamicPolicy::~BPSDynamicPolicy(){;}

error_t BPSDynamicPolicy::clone(const bps_request_t * request,BPSSSPartition* partition,
                                BPSPolicy*& policy){
   policy = new BPSDynamicPolicy((bps_request_t*) request,partition);
   return BPS_ERR_OK;
};

struct timeval BPSDynamicPolicy::searchStart(const struct timeval& cur_time){
   struct timeval retval; 
   retval.tv_sec = request_->toc.tv_sec +cur_time.tv_sec;
   retval.tv_usec = request_->toc.tv_usec + cur_time.tv_usec;
   return retval;
};

error_t BPSDynamicPolicy::isMatch(BPSSSPartitionUnit* unit){
   if ((unit->getFreeMemory() > request_->memory) && 
       (unit->getFreeCPU() >= request_->multiple)){
      return BPS_ERR_OK;
   }
   return BPS_ERROR;
};

error_t BPSDynamicPolicy::getJob(BPSJob*& job,const bps_magic_cookie_t& cookie){
   int time = 0; 

   ncpu_t cur_cpu = 0;
   BPSQueue<bps_job_t>* jobqueue = new BPSQueue<bps_job_t>;
   BPSSSPartitionUnit* cur_unit = 0;
   BPSSSPartitionUnit* prev_unit = 0;
   bps_job_t* jobt = 0;
   struct timeval start_time;
   while(!units_.empty()){
      prev_unit = cur_unit;
      units_.remove(cur_unit);
      if(prev_unit == 0)
         start_time = partition_->unitTime(cur_unit);
      if((prev_unit == 0 ) || (!prev_unit->isContiguous(cur_unit))){
         if(jobt != 0){  
            int ts = tstoS(partition_->unitTime(prev_unit)) - tstoS(jobt->start);
            Stots(jobt->length,ts);
            jobqueue->insert(jobt);
         };
         jobt = new bps_job_t;
         jobt->start = partition_->unitTime(cur_unit);
         jobt->memory = cur_unit->getFreeMemory();
      };
      cur_cpu = (cur_unit->getFreeCPU() / request_->multiple) * request_->multiple;
      assert(((cur_cpu % multiple) == 0) && (cur_cpu <= cur_unit->getFreeCPU()));
      time+= cur_cpu * partition_->interval();
      // IF THE TIME IS > REQUESTED TIME, DO NOT ALLOCATE FEWER CPUS
      // ALLOCATE #CPUS FOR THE JOB
      if((jobt->cpus != cur_cpu)) {
         if(prev_unit != 0){
            int ts = tstoS(partition_->unitTime(prev_unit))- tstoS(jobt->start);
            Stots(jobt->length,ts);
            jobqueue->insert(jobt);
            jobt = new bps_job_t;
            jobt->start = partition_->unitTime(cur_unit);
            jobt->cpus = cur_cpu;
         } else {
            jobt->cpus = cur_cpu;
         };
      }
      cur_unit->freeCPU(cur_cpu);
      cur_unit->freeMemory(request_->memory);

      if(time >= tstoS(request_->toc)){
         BPSSSPartitionUnit* temp_unit;
         while(!units_.empty()){
            units_.remove(temp_unit);
            temp_unit->unlock();
         }
         job = new BPSJob(jobqueue,start_time,
                      partition_->unitTime(cur_unit),
                      cookie);
         cur_unit->unlock();
         return BPS_ERR_OK;
      }
      cur_unit->unlock();
   }
   return BPS_ERR_NO_JOB;
};
                                                     
error_t BPSDynamicPolicy::search(){
   BPSSSPartitionIterator* it = new BPSSSPartitionIterator(partition_);
   struct timeval time = searchStart(partition_->curTime());
   // check the window
   if(tstoS(time) + tstoS(request_->toc) >
      tstoS(partition_->curTime()) + tstoS(request_->window)){
      return BPS_ERR_WINDOW_TOO_SMALL;
   }
   int time_avail = 0;
   for(it->start(time);time_avail == tstoS(request_->toc);it->next()){
      it->current()->wlock();
      if(isMatch(it->current())){
         time_avail += (it->current()->getFreeCPU() / request_->multiple) * request_->multiple;
         units_.insert(it->current());
      };
      if(time_avail > tstoS(request_->window) - tstoS(partition_->curTime())){
         BPSSSPartitionUnit* temp_unit;
         while(!units_.empty())
            temp_unit->unlock();
         return BPS_ERR_WINDOW_TOO_SMALL;
      }
   }
   return BPS_ERR_OK;
};
      
   

   





