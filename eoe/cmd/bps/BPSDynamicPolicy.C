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
/*************************************************************************
 * Function Name: matchJob
 * Parameters:
 * Returns:
 * Effects:
     A job is a match if 
     1. There is enough free memory
        
         
*************************************************************************/

error_t BPSDynamicPolicy::isMatch(BPSSSPartitionUnit* unit){
   if(unit->getFreeMemory() >= request_->memory)
      return BPS_ERR_OUT_OF_MEMORY;
   if((unit->getFreeCPU() / request_->multiple) == 0){      
      return BPS_ERR_NOT_ENOUGH_CPUS;
   }
   return BPS_ERR_OK;
};
/*************************************************************************
 * Function Name: getJob
 * Parameters:
 * Returns:
 * Effects:
     allocates the space from the sspartunits 
*************************************************************************/

error_t BPSDynamicPolicy::getJob(BPSJob*& job,const bps_magic_cookie_t& cookie){
   
   BPSSSPartitionUnit* unit;
   BPSQueue<bps_job_t>* jobqueue = new BPSQueue<bps_job_t>;
   bps_job_t* jobt = new bps_job_t;
   units_.remove(unit);
   jobt->start = partition_->unitTime(unit);
   if(units_.empty()){
      Stots(jobt->length,tstoS(jobt->start)* num_cpus_);
   }
   jobt->cpus = num_cpus_;
   jobt->memory = request_->memory;
   partition_->getID(jobt->id);
   while(units_.empty()){
      units_.remove(unit);
      unit->freeCPU(num_cpus_);
      unit->freeMemory(request_->memory);
   };
   Stots(jobt->length,tstoS(jobt->start) + tstoS(partition_->unitTime(unit)));
   jobqueue->insert(jobt);
   job = new BPSJob(jobqueue,jobt->start,jobt->length,cookie);
   return BPS_ERR_OK;

};
/*************************************************************************
 * Function Name: search
 * Parameters:
 * Returns:
 * Effects:  
    for start time to end of window and job not found
    if(isMatch() == BPS_ERR_RESTART){
    starttime_job = cur_time;
    cpus = unit.cpus;
    } else if (isMatch() == BPS_ERR_THINER){
    cpus = unit.cpus;
    } else if (isMatch() == BPS_ERR_NO_JOB){
    return BPS_ERR_NO_JOB;
    }
      
    for unit_Time to end of window
    
*************************************************************************/
                                                     
error_t BPSDynamicPolicy::search(){
   BPSSSPartitionIterator* it = partition_ ->getIterator();
   struct timeval cur_time = partition_->curTime();
   struct timeval search_start;
   struct timeval start_time;
   search_start =searchStart(cur_time);

   // check that the window is okay
   if(tstoS(search_start) + tstoS(request_->toc) > 
      (tstoS(request_->window) + tstoS(cur_time)))
      return BPS_ERR_WINDOW_TOO_SMALL;

   it->start(search_start);
   // sanity check tht the job can actually be run in the time constraints requested;
   num_cpus_ = request_->ncpus;

   // check that more cpus than available are not being requested
   if(partition_->getNcpu() < num_cpus_)
      return BPS_ERR_NOT_ENOUGH_CPUS;

   // check that the memory actually exists
   if(request_->memory > partition_->getMemory())
      return BPS_ERR_OUT_OF_MEMORY;
   long window = tstoS(request_->window);
   long toc = tstoS(request_->toc);
   long search_start_sec = tstoS(search_start);
   long time_alloc = 0;
   long intervals = 0;
   if(toc / num_cpus_ > (search_start_sec + window))
      return BPS_ERR_WINDOW_TOO_SMALL;
   
   start_time = partition_->curTime();
   error_t err;
   for(;it->stop(request_->window) || (time_alloc == toc);it->next()){
      if(search_start_sec == -1){
         search_start_sec = tstoS(partition_->unitTime(it->current()));
      }
      if(((search_start_sec + toc) / num_cpus_) >  (search_start_sec + window)) 
         return BPS_ERR_WINDOW_TOO_SMALL;
      it->current()->wlock();
      if((err = isMatch(it->current())) != BPS_ERR_OK) {
         it->current()->unlock();
         num_cpus_ = request_->ncpus;
         search_start_sec = -1;
         {
            BPSSSPartitionUnit* unit;
            while(!units_.empty()){
               units_.remove(unit);
               unit->unlock();
            };
         }
      } else {
         units_.insert(it->current());
         num_cpus_ = (it->current()->getFreeCPU() / request_->multiple) * request_->multiple;
         intervals++;
         time_alloc = intervals * partition_->interval() * num_cpus_;
      }
   }
   if(time_alloc > toc){
      BPSSSPartitionUnit* unit;
      while(!units_.empty()){
         units_.remove(unit);
         unit->unlock();
      };
      return BPS_ERR_WINDOW_TOO_SMALL;
   }
   return BPS_ERR_OK;
};
      
   

   






