/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSPolicyManager.H                               */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 15:51:03 1995                            */
/****************************************************************/


#ifndef BPSPolicyManager_HEADER
#define BPSPolicyManager_HEADER

class BPSPolicyManager {
   BPSPolicyManager();
   BPSPolicyManager(BPSPolicyManager& pol);
   BPSPolicyManager& operator=(BPSPolicyManager&);

   static List<BPSPolicy> policy_;
public:
  
   virtual bool_t clone(bps_policy_id_t policy_id,
                        BPSRequest* request);
};



#endif
