/****************************************************************
*    NAME:                                                     
*    ACCT: kostadis                                            
*    FILE: BPSDynamicPolicy.H                                  
*    ASGN:                                                     
*    DATE: Mon Jul 17 11:09:21 1995                            
****************************************************************/


#ifndef BPSDYNAMICPOLICY_HEADER
#define BPSDYNAMICPOLICY_HEADER

/****************************************************************
 * CLASS NAME : BPSDynamicPolicy                                  
  	Whenever a user wants to use a dynamic policy with respect
  to the way their programs are scheduled, this object is received
  from the polic manager. 
	The dynamic policy tries to get a best fit of the job
  widening and thining it as necessary.
****************************************************************/
#include <BPSPolicy.H>
#include <bps.h>
#include <BPS_lib.H>

class BPSSSPartition;
class BPSSSPartitionUnit;
class BPSJob;
class BPSDynamicPolicy : public BPSPolicy{
   BPSDynamicPolicy();
   BPSDynamicPolicy& operator=(BPSDynamicPolicy&);
   BPSDynamicPolicy(BPSDynamicPolicy&);
protected:
   ncpu_t num_cpus_; // number of cpus being used
   
   BPSQueue<BPSSSPartitionUnit> units_;

   virtual struct timeval searchStart(const struct timeval& cur_time) ;

   virtual error_t isMatch(BPSSSPartitionUnit* unit) ;
public:
   BPSDynamicPolicy(const bps_policy_id_t& id);
   BPSDynamicPolicy(bps_request_t* request,BPSSSPartition* partition);
   ~BPSDynamicPolicy();
   
   virtual error_t clone(const bps_request_t * request, BPSSSPartition* partition,
			    BPSPolicy*& policy) ;

   virtual error_t getJob(BPSJob*& job,const bps_magic_cookie_t& cookie) ;
   

   virtual error_t search() ;

};


#endif
