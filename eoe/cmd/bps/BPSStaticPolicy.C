/****************************************************************
*    NAME:                                                     
*    ACCT: kostadis                                            
*    FILE: BPSStaticPolicy.C                                   
*    ASGN:                                                     
*    DATE: Mon Jul 17 12:47:47 1995                            
****************************************************************/

#include "BPSStaticPolicy.H"
#include <bps.h>
#include <BPS_lib.H>
#include <BPSPolicy.H>
#include <BPSSSPartition.H>
#include <BPSJob.H>
#include <BPSSSPartitionUnit.H>
static BPSStaticPolicy policy("0996a7ad-5a34-101e-8ece-080069026881");

BPSStaticPolicy::BPSStaticPolicy(char* id)
:BPSPolicy(id){
   printf("here we are in BPSStaticPolicy\n");
}

BPSStaticPolicy::BPSStaticPolicy(bps_request_t* request,
                                 BPSSSPartition* partition)
:BPSPolicy(request,partition){;}
BPSStaticPolicy::~BPSStaticPolicy(){
   delete units_;
}


error_t BPSStaticPolicy::clone(const bps_request_t* request,
                               BPSSSPartition* unit,
                               BPSPolicy*& policy){
   policy = new BPSStaticPolicy((bps_request_t*) request,unit);
   return BPS_ERR_OK;
};

struct timeval BPSStaticPolicy::searchStart(const struct timeval& cur_time){
   struct timeval retval;
   return retval;
};


error_t BPSStaticPolicy::isMatch(BPSSSPartitionUnit* unit){
   
   if((unit->getFreeCPU() == request_->ncpus) && (unit->getFreeMemory() == request_->memory)){
      return BPS_ERR_OK;
   }
   return BPS_ERR_NO_MATCH;
};

error_t BPSStaticPolicy::search(){
   BPSSSPartitionIterator* it = new BPSSSPartitionIterator(partition_);
   struct timeval time = searchStart(partition_->curTime());

   if((tstoS(time)  + tstoS(request_->toc)) > (tstoS(partition_->curTime()) + tstoS(request_->window)))
      return BPS_ERR_WINDOW_TOO_SMALL;
   struct timeval stop;
   Stots(stop,(tstoS(request_->toc) + tstoS(time)));
   for(it->start(time);it->stop(stop);it->next()){
      it->current()->wlock();
      error_t err;
      if((err = isMatch(it->current()))!= BPS_ERR_OK){
         it->current()->unlock();
         BPSSSPartitionUnit* unit;
         while(!units_->empty()){
            units_->remove(unit);
            unit->unlock();
         };
         return err;
      }
      units_->insert(it->current());
   };
   return BPS_ERR_OK;
}

error_t BPSStaticPolicy::getJob(BPSJob*& job,const bps_magic_cookie_t& cookie){
   BPSSSPartitionUnit* temp_unit = (BPSSSPartitionUnit*) 0xA3;
   bps_job_t* jobt = new bps_job_t;
   jobt->cpus = request_->ncpus;
   jobt->memory = request_->memory;
   jobt->start = searchStart(partition_->curTime());
   Stots(jobt->length,tstoS(request_->toc));
   partition_->getID(jobt->id);

   BPSQueue<bps_job_t>* jobqueue = new BPSQueue<bps_job_t>;
   job = new BPSJob(jobqueue,jobt->start,jobt->length,cookie);
   while(!units_->empty()){
      units_->remove(temp_unit);
      temp_unit->freeCPU(request_->ncpus);
      temp_unit->freeMemory(request_->memory);
      temp_unit->unlock();
   }
   return BPS_ERR_OK;
};



      
      
 


