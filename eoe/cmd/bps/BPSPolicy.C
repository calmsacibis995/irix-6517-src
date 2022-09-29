/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSPolicy.C                                         */
/*    ASGN:                                                     */
/*    DATE: Sun Jul 16 16:32:50 1995                            */
/****************************************************************/


#include <BPSPolicyManager.H>
#include "BPSPolicy.H"
#include <bps.h>
#include <BPS_lib.H>
#include <stdio.h>
#include <BPSSSPartition.H>

#include <sys/types.h>
#include <Debug.H>
#include <string.h>
extern "C" {
   void uuid_from_string(char* uuid_str, uuid_t* uuid, uint_t *status);
};

/*************************************************************************
 * Function Name: BPSPolicy                                               
 * Parameters: policy_id : the type id of the policy
 * Returns:
 * Effects: creates a policy type and logs it with the policy manager
*************************************************************************/

BPSPolicy::BPSPolicy(char* policy_id){
   uint_t status;
   uuid_from_string(policy_id,&id_,&status);
   BPSPolicyManager::base_ = this;
   next_ = BPSPolicy::list_;
   BPSPolicy::list_ = this;
};

BPSPolicy::~BPSPolicy(){;}

/*************************************************************************
 * Function Name: BPSPolicy                                               
 * Parameters: const bps_request_t
 * Returns:
 * Effects:  creates a policy object
*************************************************************************/
BPSPolicy::BPSPolicy(bps_request_t* request,BPSSSPartition* partition)
:request_(request),partition_(partition){    

};

