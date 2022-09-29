/****************************************************************
 *    NAME: 
 *    ACCT: kostadis
 *    FILE: BPSJob.C
 *    DATE: Mon Jul 10 17:50:15 PDT 1995
 ****************************************************************/

#include "BPSJob.H"
#include <BPSSSPartition.H> 
#include <Debug.H>

#include <iostream.h> 
ostream& operator << (ostream& os, const BPSJob& job){
   os << " job [" << job.magic_cookie_ << "] " << "START " << job.start_time_ << " END " << job.end_time_ << " cpus " << job.jobqueue_->front().cpus << " memory " << job.jobqueue_->front().memory << " length " << job.jobqueue_->front().length ;
   return os;
};
bool_t 
BPSJob::rlock()
{
   rw_lock_.rlock();
   return 1;
}

bool_t 
BPSJob::wlock()
 {
    rw_lock_.wlock();
    return 1;
 }

 bool_t 
 BPSJob::unlock()
 {
    rw_lock_.unlock();
    return 1;
 }


 /*************************************************************************
  * Function Name: getCookie
  * Parameters: bps_magic_cookie_t* magic_cookie
  * Returns: bool_t 
  * Effects: returns the associate magic cookie of the job which is the BCB
    id.
  *************************************************************************/
 const bps_magic_cookie_t&
 BPSJob::getCookie()
 {
    return magic_cookie_;
 }

 const bps_time_t& BPSJob::startTime(){
    return start_time_;


 }
 const bps_time_t& BPSJob::endTime(){
    return end_time_;

 }

 BPSJob::BPSJob(jobqueue* jobs,
                BPSSSPartition* sspart,
                bps_time_t start_time,
                bps_time_t end_time,
                const bps_magic_cookie_t& magic_cookie){
    start_time_ = start_time;
    end_time_ = end_time;
    jobqueue_ = jobs;
    sspart_ = sspart;
    magic_cookie_ = magic_cookie;
 };

 BPSSSPartition* BPSJob::getPartition() const{
    return sspart_;
 };
 bool operator==(const bps_job_t&,
                 const bps_job_t&){
    return 1;
 }

 bool operator<(const bps_job_t&,
                const bps_job_t&){
    return 1;
 };

/****************************************************************
 * Function Name: ~BPSJob                                       
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: This destructor handles the deallocation of any unused
            resources that are still claimed by the job.

  What is an unused resource? An unused resource is any resource
  of a partition that is claimed and has not been used and CAN be used.
  So a resource in the past, is not released, but a resource in the future
  is.

****************************************************************/



 BPSJob::~BPSJob(){
    // need to deallocate the partition resources
    BPSSSPartitionIterator* iterator;
    // lock the current time
    sspart_->values_wlock();

    iterator = sspart_->getIterator();

    // for each job in the jobqueue
    while(!jobqueue_->empty()){
       bps_job_t jobt;
       int size = jobqueue_->size();
       jobt = jobqueue_->front(); // this gets the front job
       jobqueue_->pop(); // this deletes it from the queue (stl for ya!)

       // the bps_job_t has a start time, but NO end time,
       // and the reason for that is that the start is fluid.
       // so you have to calculate the start time.
       long intervals = jobt.length/(sspart_->interval() * jobt.cpus);
       long start_time;
       start_time = jobt.end - intervals*sspart_->interval();

       assert(start_time == start_time_);
       // time cleared the values? The values are in the past
       if(sspart_->curTime() >= jobt.end){
          continue;
       };


      // If the end is not in the past, the start may be, so you want 
      // to start at the current time.
       if(sspart_->curTime() > start_time){
          start_time = sspart_->curTime();
       };
      
       iterator->start(start_time);
       assert (start_time == sspart_->unitTime(iterator->current()));
       for(;iterator->stop(jobt.end); iterator->next()){
          BPSSSPartitionUnit* unit = iterator->current();
          unit->freeMemory(jobt.memory);
          unit->freeCPU(jobt.cpus);
       };
    
       bps_time_t val = sspart_->unitTime(iterator->current());
       assert (end_time_ == val);
    };
    delete jobqueue_;
    sspart_->values_unlock();
    sspart_->unlock();
};

/****************************************************************
 * Function Name: getJob(bps_job_t job)
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: is the casting operator, converts a BPSJob to a bps_job_t;
****************************************************************/
error_t BPSJob::getJob(bps_job_t& job){
   job = jobqueue_->front();
   return BPS_ERR_OK;
};
   

BPSJob::BPSJob(BPSSSPartition* sspart,
               bps_time_t start_time,
               bps_time_t end_time,
               const bps_magic_cookie_t& magic_cookie,
               bps_job_t job){
   sspart->rlock();
   jobqueue_ = new jobqueue;
   jobqueue_->push(job);
   sspart_ = sspart;
   start_time_ = start_time;
   end_time_ = end_time;
   magic_cookie_ = magic_cookie;
};
