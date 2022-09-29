/****************************************************************
*    NAME:                                                     
*    ACCT: kostadis                                            
*    FILE: BPSStaticPolicy.H                                   
*    ASGN:                                                     
*    DATE: Mon Jul 17 12:48:00 1995                            
****************************************************************/


#ifndef BPSSTATICPOLICY_HEADER
#define BPSSTATICPOLICY_HEADER

    
/****************************************************************
 * CLASS NAME : BPSStaticPolicy                                  
  	Whenever a user wants to use a static policy with respect
  to the way their programs are scheduled, this object is received
  from the polic manager. 
	The static policy tries make an allocation that is a 
  contiguous block of space exactly as requested such that the job
  does not terminate after the user requested.
****************************************************************/
#include <BPSPolicy.H>
#include <bps.h>
#include <BPS_lib.H>

class BPSSSPartition;
class BPSSSPartitionUnit;
class BPSJob;



class BPSStaticPolicy : public BPSPolicy{
   BPSStaticPolicy();
   BPSStaticPolicy& operator=(BPSStaticPolicy&);
   BPSStaticPolicy(BPSStaticPolicy&);
protected:
   bps_request_t* request_;
   BPSQueue<BPSSSPartitionUnit>* units_;
   // Returns a time that is equal to 
   // (window + cur_time - requested_time)
   virtual struct timeval searchStart(const struct timeval& cur_time) ;
   
   // return BPS_ERR_OK if the memory and cpus
   // available are exactly equal to the requested number
   virtual error_t isMatch(BPSSSPartitionUnit* unit) ;
public:
   BPSStaticPolicy(char* id);
   BPSStaticPolicy(bps_request_t* request,BPSSSPartition* partition);
   ~BPSStaticPolicy();
   virtual error_t clone(const bps_request_t * request, BPSSSPartition* partition,
                         BPSPolicy*& policy) ;

   // creates a job structure out of the ss_partition_units
   // linked list
   // create bps_job_t list
   // for each element in BPSSSPartitionUnit list
   //   set new memory and cpus
   //   add to list
   //   unlock unit.
   // create a new BPSJob with job list
   // return BPS_ERR_OK
   virtual error_t getJob(BPSJob*& job,const bps_magic_cookie_t& cookie);

   // Search does the following
   //   for time = search_start to requested_time
   //   unit.wlock
   //   if(match(unit))
   //     add to list
   //   else
   //     unlock all elements in list
   //     return BPS_ERR_NO_MATCH
   virtual error_t search() ;

};


#endif
