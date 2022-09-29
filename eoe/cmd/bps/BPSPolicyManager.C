/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSPolicyManager.C                                  */
/*    ASGN:                                                     */
/*    DATE: Sun Jul 16 16:40:08 1995                            */
/****************************************************************/



#include <BPSPolicyManager.H>
#include <BPSPolicy.H>
#include <bps.h>
#include <BPSDefaultPolicy.H>
BPSPolicy* BPSPolicyManager::base_ = 0;
static BPSDefaultPolicy policy(BPS_POLICY);

BPSPolicyManager::BPSPolicyManager(){
};


/****************************************************************
 * Function Name: clone                                         
 * Parameters:                                                  
 * Returns: a policy                                            
 * Effects: Searches the null terminated linked list            
            for a policy that matches the policy id, constructs
            the new policy and returns it. If no policy could
   be found that matched the id, 'BPS_ERR_BAD_POLICY_ID' is returned
****************************************************************/

error_t BPSPolicyManager::clone(const bps_request_t& request,
                                BPSSSPartition* partition,
                                const bps_policy_id_t& id,
                                BPSPolicy*& policy){
   BPSPolicy* temp = BPSPolicyManager::base_;
   while(temp != 0){
      if(*temp == id){
         temp->clone(&request,partition,policy);
         return BPS_ERR_OK;
      };
      temp = temp->next_;
   };
   return BPS_ERR_BAD_POLICY;
}







