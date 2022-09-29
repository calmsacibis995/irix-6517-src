/****************************************************************
 *    NAME: 
 *    ACCT: kostadis
 *    FILE: BPSJobSchedule.C
 *    DATE: Mon Jul 10 17:50:15 PDT 1995
 ****************************************************************/

#include "BPSJobSchedule.H"
#include <bps.h>
#include <BPS_lib.H>
#include <Thread.H>
BPSJobSchedule::BPSJobSchedule()
{

};
/*************************************************************************
 * Function Name: addJob
 * Parameters: BPSJob* job
 * Returns: bool_t 
 * Effects: Adds a job to the job schedule
 *************************************************************************/
error_t 
BPSJobSchedule::
addJob(BPSJob* job)
{
   pair_type pair(job->getCookie(),job);
   schedule_.insert(pair);
   return BPS_ERR_OK;
}



/*************************************************************************
 * Function Name: deleteJob
 * Parameters: bps_magic_cookie_t magic_cookie
 * Returns: error_t 
 * Effects: removes a job, but does not delete it
 *************************************************************************/
error_t 
BPSJobSchedule::
deleteJob(BPSJob*& job, bps_magic_cookie_t magic_cookie)
{
   jobmap::iterator i = schedule_.find(magic_cookie);
   if(i == schedule_.end()){
      job = 0;
      return BPS_ERR_BAD_MAGIC_COOKIE;
   } else 
      job = ((*i).second)();
   schedule_.erase(i);
   return BPS_ERR_OK;
}


bool_t 
BPSJobSchedule::rlock()
{
   lock_.rlock();
   return 1;
}
bool_t 
BPSJobSchedule::wlock()
{
   lock_.wlock();
   return 1;
}



bool_t 
BPSJobSchedule::unlock()
{
   lock_.unlock();
   return 1;
}


/****************************************************************
 * Function Name: queryTime                                     
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: returns the job that starts at the specified time interval
****************************************************************/



error_t
BPSJobSchedule::
queryTime(const bps_time_t& time,
         BPSJob*& job){
   jobmap::iterator i = schedule_.begin();
   jobmap::iterator end = schedule_.end();
   while(i != end){
      BPSJob* jobl = ((*i).second)();
      if(job->startTime() == time){
         job = jobl;
         return BPS_ERR_OK;
      };
      i++;
   };
   return BPS_ERROR;

};

#ifndef SCHEDULENDEBUG
ostream& operator<< (ostream& os, BPSJobSchedule& schedule){
   BPSJobSchedule::jobmap::iterator i = schedule.schedule_.begin();
   BPSJobSchedule::jobmap::iterator end = schedule.schedule_.end();
   for(i; i != end; i++){
      os << *(((*i).second)()) << endl;
   };
   os << endl;
   return os;
};
#endif







