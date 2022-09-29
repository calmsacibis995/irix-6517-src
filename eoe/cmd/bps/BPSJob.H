/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSJob.H                                            */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 15:17:38 1995                            */
/****************************************************************/


#ifndef BPSJOB_HEADER
#define BPSJOB_HEADER

#include <bps.h>
#include <BPSPrivate.H>
#include <Thread.H>
#include <BPS_lib.H>
#include <bool.h>
#include <stl.h>
class BPSSSPartition;
               
               
typedef   queue<list<bps_job_t> > jobqueue;

/****************************************************************
 * CLASS NAME : BPSJob                                                 
   BPSJob represents an allocation of resources in the form of a job.
   A couple of points:
   1. A job is maintained as a job queue, rather than a single job,
      the reason for that is that it is conceivable in a future revision
      that a job will not represent a single contiguous allocation
      but rather a series of non-contiguous and non-uniform allocations.
   2. A job can be converted from a BPSJob to bps_job_t by the 'getJob'
      method.  This needs to be extended if we decide to go with the
      BPSJob 'having more than one job_t' philosophy.
   3. A job has a link to the partition the allocation was made from, and 
      a read lock on it. When it is deleted, it deallocates any unused resources, 
      and unlocks the partition.
****************************************************************/



class BPSJob {
protected:
   RWLock_writer rw_lock_;
   bps_time_t start_time_,end_time_;
   jobqueue * jobqueue_;
   BPSSSPartition* sspart_;
   bps_magic_cookie_t magic_cookie_;
public:
   BPSJob(jobqueue* job, 
          BPSSSPartition* sspart, 
          bps_time_t start_time,
          bps_time_t end_time,
          const bps_magic_cookie_t& magic_cookie);
   BPSJob(BPSSSPartition* sspart,
          bps_time_t start_time,
          bps_time_t end_time,
          const bps_magic_cookie_t& magic_cookie,
          bps_job_t job);
   ~BPSJob();
   bool_t rlock();
   bool_t wlock();
   bool_t unlock();
   bool operator==(const BPSJob&) {assert (1==0); return 1;};
   bool operator<(const BPSJob&){assert (1==0); return 1;}

   const bps_time_t& startTime();
   const bps_time_t& endTime();
   error_t getJob(bps_job_t& job);
   const bps_magic_cookie_t& getCookie(); // returns the cookie of the job
   BPSSSPartition* getPartition() const; // returns the associated partition
friend  ostream& operator << (ostream& os, const BPSJob& job);
};

bool operator==(const bps_job_t&, const bps_job_t&);
bool operator<(const bps_job_t&, const bps_job_t&);
bool operator>(const bps_job_t&, const bps_job_t&);

#endif


