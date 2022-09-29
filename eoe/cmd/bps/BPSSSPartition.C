/****************************************************************
 *    NAME: 
 *    ACCT: kostadis
 *    FILE: BPSSSPartition.C
 *    DATE: Mon Jul 10 17:50:15 PDT 1995
 ****************************************************************/

#include "BPSSSPartition.H"
#include <Debug.H>
#include <sys/time.h>
#include <bps.h>
#include <BPS_lib.H>
#include <BPSSSPartitionUnit.H>
#include <malloc.h>
#include <iostream.h>
#include <fstream.h>
#include <BPSSSPartManager.H>
#include <bps_sspart.h>
#include <BPSAuthenticator.H>
BPSSSPartition* BPSSSPartition::list_ = 0;
extern "C" {
   void uuid_from_string(char* uuid_str, uuid_t* uuid, uint_t *status);
};

#include <string.h>
#include <bps.h>
#include <assert.h>

/****************************************************************
 * Function Name:  BPSSSPartition
 * Parameters: data : structure that contains the relevant 
                      info
          array_size : how large the underlying array has to be
 * Returns:                                                     
 * Effects:  Creates a partition                                
****************************************************************/


BPSSSPartition::BPSSSPartition(const bps_sspartdata_t& data, long array_size)
:ncpu_(data.ncpus),memory_(data.memory)
{ 
   
   pthread_mutex_init(&mutex_,NULL);
   pthread_mutex_lock(&mutex_);
   pthread_cond_init(&cond_,NULL);
   
   authenticator_ = new BPSAuthenticator;
   refcnt_ = 0;
   isecs_ = data.interval;
   dsecs_ = data.duration;
   assert((dsecs_ % isecs_) == 0); // make sure there are no 'left over' pieces
   csecs_ = 0;
   size_ = array_size;
   sspart_unit_ = new sspuvec(array_size);
   part_id_ = data.id; 
};

/*************************************************************************
 * Function Name: ~BPSSSPartition
 * Parameters: 
 * Returns: 
 * Effects: 
 *************************************************************************/

BPSSSPartition::~BPSSSPartition()
{
   while(refcnt_ != 0)
      pthread_cond_wait(&cond_,&mutex_);
   pthread_cond_destroy(&cond_);
   pthread_mutex_destroy(&mutex_);
   return;
}


/*************************************************************************
 * Function Name: queery
 * Parameters: bps_info_req_t * req,bps_info_reply_t* reply
 * Returns: error_t
 * Effects: returns the reply structure filled with the relevant data
 *************************************************************************/
error_t 
BPSSSPartition::query(bps_info_request_t & req,bps_info_reply_t* reply)
{

   if(req.end - req.start > dsecs_){
      return BPS_ERR_INVALID_TIME_INTERVAL;
   }

   if(req.end - req.start <= 0){
      return BPS_ERR_INVALID_TIME_INTERVAL;
   };
   // you need to allocate arrays for the data to be placed in
   // so get the size
   int size = (req.end - req.start) % dsecs_;
   size = size / isecs_;

   bps_cpusused_alloc(&reply->cpu_usage, size);
   bps_memoryused_alloc(&reply->memory_usage, size);
   bps_partitionids_alloc(&reply->partition_ids,1);

   getID(reply->partition_ids->partition_ids[0]);

   BPSSSPartitionIterator* it = getIterator();
   int i = 0;
   it->start(req.start);
   for(; it->stop(req.end); it->next()){

      reply->cpu_usage->cpu_usage[i] = it->current()->getFreeCPU();
      reply->memory_usage->memory_usage[i] = it->current()->getFreeMemory();
      i++;
      i = i % size; 
   };

   return BPS_ERR_OK;
}


/*************************************************************************
 * Function Name: resetMemory
 * Parameters: memory_t memory is an absolute value because it 
               is an unsigned long to be equal to max theoretical 
   capacity of the system
 * Returns: error_t
 * Effects: 
   1. Adding memory just adds to capacity and does not affect existing
      jobs
   2. Delete memory : if the memory delete is excess capacity then the 
                      system successfully deletes the memory
      otherwise it reports an error. 
 *************************************************************************/
error_t 
BPSSSPartition::resetMemory(const memory_t& memory, BPSSSPartManager* manager)
{

   long memory_diff = memory - memory_; // first get the difference
                                        // and pray overflow does not take place
   if(memory_diff== 0) {
      return BPS_ERR_OK;
   }
   values_wlock();
   if(memory_diff < 0){
      if(memory_diff+ memory_ <= 0){ 
         values_unlock();
         return BPS_ERROR;
      };
      // Check to see if the memory is free throughout the partition
      for(int i = 0; i < size_; i++){
         if((*sspart_unit_)[i]->getFreeMemory() < memory_ + memory_diff){
         values_unlock();
         return BPS_ERR_MEMORY_USED;
         }
      };
     // if it is free set the new memory
      for(i = 0; i < size_; i++){
         (*sspart_unit_)[i]->setFreeMemory(memory);
      }
      error_t err;
     // ... and release it to the manager to be used by somebody else
      err = manager->releaseBlock(0,
                                  memory_diff,
                                  0,
                                  manager->duration());
      values_unlock();
      return err;
   }else {
      // We want to add the memory
      // So all you need to do is check wether the manager
      // has the resource you are looking for
      if(memory_ + memory_diff> manager->getMemory()){
         val_lock_.unlock();
         return BPS_ERROR;
      }
      error_t err;
      err = manager->lockBlock(0,
                         memory_diff,
                         0,
                         manager->duration());
      if(err != BPS_ERR_OK) {
         values_unlock();
         return err;
      }
      for(int i = 0; i < size_; i++){
         (*sspart_unit_)[i]->setFreeMemory(memory);
      }
   }; 
   values_unlock();
   return BPS_ERR_OK;
}


/*************************************************************************
 * Function Name: resize_cpu
 * Parameters: ncpu_t ncpus
 * Returns: bool_t 
 * Effects:  Works the same way memory does, so just read above!
 *************************************************************************/
error_t 
BPSSSPartition::resetCPU(const ncpu_t& ncpu, BPSSSPartManager* manager)
{
   ncpu_t ncpus = ncpu - ncpu_;
   if(ncpus == 0){
      return BPS_ERR_OK;
   }
   val_lock_.wlock();
   if(ncpus < 0){
      if(ncpus + ncpu_ <= 0){
         val_lock_.unlock();
         return BPS_ERROR;
      };
      for(int i = 0; i < size_; i++){
         if((*sspart_unit_)[i]->getFreeCPU() < ncpu_ + ncpus){
            val_lock_.unlock();
            return BPS_ERR_CPU_USED;
         }
      };
      for(i = 0; i < size_; i++){
         (*sspart_unit_)[i]->setFreeCPU(ncpus);
      }
      error_t err;
      err = manager->releaseBlock(0,
                                  ncpus,
                                  0,
                                  manager->duration());
      values_unlock();
      return err;
   }else {
      if(ncpu_ + ncpus > manager->getNcpus()){
         val_lock_.unlock();
         return BPS_ERROR;
      }
      error_t err;
      err = manager->lockBlock(0,
                         ncpus,
                         0,
                         manager->duration());
      if(err != BPS_ERR_OK){
         values_unlock();
         return err;
      }
      for(int i = 0; i < size_; i++){
         (*sspart_unit_)[i]->setFreeCPU(ncpus);
      }
   };
   val_lock_.unlock();
   return BPS_ERR_OK;

}


/*************************************************************************
 * Function Name: rlock
 * Parameters: 
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartition::rlock()
{
   refmut_.lock();
   refcnt_++;
   symp(refcnt_);
   refmut_.unlock();
   rw_lock_.rlock();
   return 1;
}


/*************************************************************************
 * Function Name: wlock
 * Parameters: 
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartition::wlock()
{
   refmut_.lock();
   refcnt_++;
   symp(refcnt_);
   refmut_.unlock();
   rw_lock_.wlock();
   return 1;
}


/*************************************************************************
 * Function Name: unlock
 * Parameters: 
 * Returns: bool_t 
 * Effects: 
 *************************************************************************/
bool_t 
BPSSSPartition::unlock()
{
   refmut_.lock();
   symp(refcnt_);
   assert(refcnt_> 0);
   refcnt_--;
   if(refcnt_ == 0)
      pthread_cond_broadcast(&cond_);
   refmut_.unlock();
   rw_lock_.unlock();
   return 1;
}


/*************************************************************************
 * Function Name: getNcpu
 * Parameters: 
 * Returns: ncpu_t 
 * Effects: 
 *************************************************************************/
ncpu_t 
BPSSSPartition::getNcpu() const
{
   return ncpu_;
}


/*************************************************************************
 * Function Name: getMemory
 * Parameters: 
 * Returns: memory_t 
 * Effects: 
 *************************************************************************/
memory_t 
BPSSSPartition::getMemory() const
{
   return memory_;
}


/*************************************************************************
 * Function Name: getIterator
 * Parameters: 
 * Returns: BPSSSPartition* 
 * Effects: 
 *************************************************************************/
BPSSSPartitionIterator* 
BPSSSPartition::getIterator()
{
   return new BPSSSPartitionIterator(this);
}


/*************************************************************************
 * Function Name: getID
 * Parameters: 
 * Returns: bps_sspartition_id_t 
 * Effects: 
 *************************************************************************/
void
BPSSSPartition::getID(bps_sspartition_id_t& id) 
{
   memcpy(&id, &part_id_,sizeof(bps_sspartition_id_t));
}


/*************************************************************************
 * Function Name: resetTime
 * Parameters: bps_time_t time
 * Returns: void 
 * Effects: 
    resets the value of current_time moving it forward.
    The other effect is to clear up all the time between the two
    moves so ncpus_free and memory are all free.
    We move time forward this way because, cur time must never be so out 
    of sync from current kernel time that a job gets scheduled in the past!
    Moving forward in time, furthermore, clears up all the resources, this is 
    important, especially for long jobs, that are the entire length of the
    partition.
    One side effect of resetTime, is that when deallocating resources associated
    with a job, you have to make sure not to deallocate resources that have
    already been deallocated.
 *************************************************************************/
#include <Debug.H>
void 
BPSSSPartition::resetTime(bps_time_t time)
{
   
   bps_time_t new_time = time / isecs_ * isecs_;
   if(new_time < csecs_){
      return;
   };
   BPSSSPartitionIterator* it = getIterator();
   it->start(csecs_);
   if(csecs_ == 0){
      csecs_ = new_time;
      return;
   }
   symp(csecs_);
   symp(new_time);
   for(; it->stop(new_time); it->next()){
      symp(*(it->current()));
      it->current()->setFreeCPU(ncpu_);
      it->current()->setFreeMemory(memory_);
      symp(*(it->current()));
   };
   csecs_ = new_time;
}

bps_time_t BPSSSPartition::interval(){
   return isecs_;
}
bps_time_t BPSSSPartition::duration(){
   return dsecs_;
}
bps_time_t BPSSSPartition::curTime(){
   return csecs_;
};

/****************************************************************
 * Function Name: unitTime                                      
 * Parameters:                                                  
 * Returns: The time associated with a unit                     
 * Effects: Calculates the time represented by the current unit 
            relative to the start time. It is imperative that the
  time returned by this function be equal to the time calculated by
  BPSSSPartitionIterator::start.

****************************************************************/



bps_time_t BPSSSPartition::unitTime(BPSSSPartitionUnit* unit){
   int index = unit->index();
   bps_time_t retval;
   assert(index >= 0 && index < size_);
   symp(index);
   int cur_pos = csecs_ % dsecs_;
   cur_pos = cur_pos / isecs_;
   retval = (index - cur_pos) * isecs_ + csecs_;
   return retval;
}


/****************************************************************
 * Function Name: resetDuration                                 
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: Simply resets the max length of any job.            
            Remember the underlying 'duration' is the duration
   of the manager, so any duration from the interval size to the 
   duration size is ok! Furthermore, it can be changed with 
   impunity.
****************************************************************/



error_t BPSSSPartition::resetDuration(const bps_time_t& time, BPSSSPartManager* manager){
   if(time > manager->duration()){
      return BPS_ERROR;
   }
   values_wlock();
   dsecs_ = time;
   values_unlock();
   return BPS_ERR_OK;

};
bool_t BPSSSPartition::values_rlock() {
   val_lock_.rlock();
   return 1;
};
bool_t BPSSSPartition::values_wlock(){
   val_lock_.wlock();
   return 1;
};
bool_t BPSSSPartition::values_unlock(){
   val_lock_.unlock();
   return 1;
}
 
/****************************************************************
 * Function Name:authenticate                                   
 * Parameters:   
 * Returns:                                                     
 * Effects: Does nothing currently, ought to be fixed          
****************************************************************/


  
error_t BPSSSPartition::authenticate(const bps_authentication_data_t& data){
   strp("in BPSSSPartition::authenticate");
   authenticator_->authenticate(data);
   return BPS_ERR_OK;
};

/****************************************************************
 * Function Name: clone                                         
 * Parameters:                                                  
 * Returns:       
    BPS_ERR_INTERVAL_BAD : interval less than 0
                           interval greater than manager interval
    BPS_ERR_DURATION_BAD : duration less than 0
                           duration greater than manager duration
                           duration % interval != 0
    BPS_ERR_NCPU_BAD     : cpus less than 0
                         : wants more cpus than manager has max available
                                              
 * Effects: This is a monster!
   1. First read in the data and perform necessary sanity checks
   
   Allocate Resources

****************************************************************/



error_t BPSSSPartition::clone(const bps_sspartition_id_t& part_id,
                              char* filename,
                              BPSSSPartition*& partition,
                              BPSSSPartManager* manager){
   // first check that the space requested actually exists;
   ifstream f(filename);
   if(!f) {
      return BPS_ERR_INIT_FILE_BAD;
   };
   bps_sspartdata_t ssdata;
   ssdata.id = part_id;
   f >> ssdata.interval;
   if(ssdata.interval <= 0 || manager->interval() < ssdata.interval){
      return BPS_ERR_INTERVAL_BAD;
   };
   f >> ssdata.duration;
   if(ssdata.duration <= 0 || manager->duration() < ssdata.duration){
      return BPS_ERR_DURATION_BAD;
   }
   f >> ssdata.ncpus;
   if(ssdata.ncpus <= 0 || manager->getNcpus() < ssdata.ncpus){
      return BPS_ERR_NCPU_BAD;
   }
   f >> ssdata.memory;
   if(ssdata.memory <= 0 || manager->getMemory() < ssdata.memory){
      return BPS_ERR_MEMORY_BAD;
   }
   if(ssdata.duration  % ssdata.interval != 0){
      return BPS_ERR_DURATION_BAD;
   };


   error_t err;
   err  = manager->lockBlock(ssdata.ncpus,
                             ssdata.memory,
                             0,
                             ssdata.duration);
   if(err != BPS_ERR_OK){
      return err;
   }else {
      partition = new BPSSSPartition(ssdata,manager->duration()/ssdata.interval);
      for(int k = 0; k < partition->size_;k++){
         (*partition->sspart_unit_)[k] = new BPSSSPartitionUnit(ssdata.ncpus,
                                                                k,
                                                                ssdata.memory);
      };
   };
   return BPS_ERR_OK;
};



/****************************************************************
 * Function Name: BPSSSPartition                                
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: Creates a type to be added to the type list          
****************************************************************/



BPSSSPartition::BPSSSPartition(char* type_id){
   uint_t status;
   uuid_from_string(type_id,&type_id_,&status);
   next_ = BPSSSPartition::list_;
  // Need this because thread library seg faults on me
   pthread_mutex_init(&mutex_,NULL);
   pthread_mutex_lock(&mutex_);
   pthread_cond_init(&cond_,NULL);
   refcnt_ = 0;
   BPSSSPartManager::base_ = this;
   BPSSSPartition::list_ = this;
};



/****************************************************************
 * Function Name: releaseResources                              
 * Parameters:                                                  
 * Returns:                                                     
 * Effects:  Needs to be implemented correctly                  
****************************************************************/



error_t BPSSSPartition::releaseResources(BPSSSPartManager* manager){

   manager->releaseBlock(ncpu_,
                         memory_,
                         0,
                         manager->duration());
   return BPS_ERROR;
};

/****************************************************************
 * Function Name: modify                                              
 * Parameters: caddr_t data : the unmarshalled data 
               unsigned long : size of data block
 * Returns:                                                     
 * Effects:
    Modifying a partition is a really *tricky* task because of
  the variety of possible partitions. Therefore the decision was 
  made to defer the 'data unmarshalling' to the target of the data
  rather than earlier on. As a result the data is a char* block.
  In the particular case of this 'partition type' it is 
  really a bps_tune_val_t.

   A Better way of doing this is to use CORBA! But that is 
   not part of the standard delivery system  :-(
****************************************************************/



error_t BPSSSPartition::modify(caddr_t data,
                               unsigned long size,
                               BPSSSPartManager* manager){
   bps_tune_val_t tune_values;
   if(size != sizeof(tune_values)){
      return BPS_ERR_BAD_DATA;
   };
   error_t err;
   memcpy(&tune_values,data,size);
   switch(tune_values.msg){
   case BPS_MOD_MEMORY:
      err = resetMemory(tune_values.memory,manager);
      break;
   case BPS_MOD_CPU:
      err = resetCPU(tune_values.ncpus,manager);
      break;
   case BPS_MOD_DURATION:
      err = resetDuration(tune_values.duration,manager);
      break;
   default:
      return BPS_ERR_BAD_DATA;
   }
   return err;
};

/****************************************************************
 * Function Name: changePermissions                             
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: Forwards the request to the authenticator
            Note that it is assumed that only 'priviledged' users
  will access this function, so that authentication takes place client
  side! 
****************************************************************/



error_t BPSSSPartition::changePermissions(const bps_permissions_msg_t& data)
{
   error_t err;
   err = authenticator_->changePermissions(data);
   return err;
};


/****************************************************************
 * Function Name: claimBlock                                    
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: claims a block between start_unit and end_unit      
            The net effect is that that region has ncpus, and memory
   decrememented
****************************************************************/



      
error_t BPSSSPartition::claimBlock(BPSSSPartitionUnit* start_unit,
                                   BPSSSPartitionUnit* end_unit,
                                   ncpu_t ncpus,
                                   memory_t memory,
                                   bps_job_t& jobt){
   bps_time_t start = unitTime(start_unit);
   bps_time_t end = unitTime(end_unit) + interval();
   jobt.cpus = ncpus;
   jobt.memory = memory;
   getID(jobt.id);
   BPSSSPartitionIterator* it = getIterator();
   int i = 0;
#ifndef NDEBUG
   BPSSSPartitionUnit*  unit;
#endif
   it->start(start);
   assert(it->current() == start_unit);
   for(;it->stop(end); it->next()){
#ifndef NDEBUG
      unit = it->current();
#endif
      i++;
      it->current()->claimCPU(ncpus);
      it->current()->claimMemory(memory);
   };
   assert(unit == end_unit);
   jobt.end = end;
   jobt.length = ncpus*i*interval();
   return BPS_ERR_OK;
};

                              
      

   

 
