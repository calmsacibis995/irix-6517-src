/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSPolicy.H                                         */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 15:43:18 1995                            */
/****************************************************************/


#ifndef BPSPOLICY_HEADER
#define BPSPOLICY_HEADER
#include <bps.h>
class BPSSSPartition;
class BPSJob;
class BPSSSPartitionUnit;

#include <Thread.H>



/*****************************************************************
  CLASS NAME : BPSPolicy 

  The policy in the BPS is responsible for finding and creating a job
  based on the request structure and the partition it can search
  in. Since the optimal policy is 'NP complete' the type of policy
  implemented can be globally sub optimal but locally optimal for
  particular types of requests, partition types or desired goals.  The
  trick was to create a mechanism that was flexible and extensible at
  the same time. The central idea was that adding a new policy had to
  not require modifying ANY existing source (so that a customer could
  use it).  The most common approach is to use dynamic linking. The
  approach used here is an idiom referred to by Coplien in _Advanced
  C++ Idioms_ as 'exemplars'. The idea, derived from Self and
  Smalltalk, is that a type is also an object. If a type is an object,
  and you can modify an object you can also modify a type. So the
  idiom Coplien describes is to at run time create a list of types and
  then later on, from that list create the object you want, by passing
  some sort of information to a 'clone' method that scans the list
  looking for a match. To set the list up, Coplien uses static
  instantiations of variables. Each type derives from some common base
  class. The base class has a static list_ and a non static next_
  member variable.  Each 'type' that wants to register itself to the
  system creates a static instance of itself, using the base
  constructor that sets the list_ variable to this and the next_
  variable to list_. At run time, each static instance adds itself to
  the list. The only important consideration is where the head of the
  list is because the order of static initialization is undefined
  (stroustrop 'Design and Evolution of C++' discusses why). Whereas
  for Coplien that was not an issue because all the statics were
  initialized in the same source file, this is not the case with this
  system. To keep track of the 'head of the list' we assign to
  BPSPolicyManager::base_, the this pointer of the policy object being
  initialized. This way the BPSPolicyManager which actually performs
  the scan always has a pointer to the head of the list. The head of
  the list, btw, is NULL terminated. The net effect is that adding a
  new policy, merely requires, compiling the new policy object and
  relinking the system. 


      
****************************************************************/


class BPSPolicy {
   BPSPolicy();
   BPSPolicy& operator=(BPSPolicy&);
   BPSPolicy(BPSPolicy&);
   static BPSPolicy* list_;
   BPSPolicy* next_;
friend class BPSPolicyManager;
protected:
   bps_policy_id_t id_; // the type id
   bps_request_t* request_; 
   BPSSSPartition* partition_; 
   virtual bps_time_t searchStart(const bps_time_t&) = 0;
   virtual error_t isMatch(BPSSSPartitionUnit* unit) = 0;
public:
   BPSPolicy(char* policy_id); // used to create policy types
   // used to create policy objects
   BPSPolicy(bps_request_t* request,BPSSSPartition* partition);
   ~BPSPolicy();

   // returns an object of the same type
   virtual error_t clone(const bps_request_t* request, BPSSSPartition* partition,
                        BPSPolicy*& policy) = 0;


   // returns the job found by search
   virtual error_t getJob(BPSJob*& job, const bps_magic_cookie_t& cookie) = 0;

   // searches for a job in the given partition
   virtual error_t search() = 0;
   int operator==(const bps_policy_id_t& id){
      return bps_policy_equal(&id_,(bps_policy_id_t*)&id);
   }
};

BPSPolicy* BPSPolicy::list_ = 0;

#endif
