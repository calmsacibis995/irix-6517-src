/****************************************************************/
/*    NAME: Kostadis Roussos                                    */
/*    ACCT: kostadis                                            */
/*    FILE: BPSServer.H                                         */
/*    DATE: Sat Jul  8 15:11:13 1995                            */
/****************************************************************/


#ifndef BPSSERVER_HEADER
#define BPSSERVER_HEADER
#include <BPS_lib.H>
#include <BPSSSPartition.H>
class BPSSocket;
class BPSPolicyManager;
class BPSJobSchedule;
#include <stl.h>

#include <bps.h>                 

/****************************************************************
 * CLASS NAME : BPSServer                                                  
  The BPSServer is a 'mediator' object 
  (see Gamma, Vlissides etal Design Patterns). It acts as a layer of 
  indirection between the Server Thread and the underlying structures of the
  Server. As such it exports an interface to the objects that make up
  the BPS.
****************************************************************/

class BPSJob;
class BPSPolicy;
class BPSServer {
   friend class BPSServerThread;

   Mutex server_thr_lock_; // locks the server thread

   typedef list<BPSPtrWrapper<BPSServerThread> > serverlist;
   // list of server threads, used to delete threads
   serverlist server_threads_;
   BPSSocket* socket_; // the port that the server accepts requests from
   BPSPolicyManager* policy_; // the policies that can be accessed
   BPSJobSchedule* schedule_; // the job schedule
   BPSSSPartManager* manager_; // manages the soft system partition
   Mutex values_; // used to protect the default id of the server
   bps_sspartition_id_t default_; // default partition
public:
   virtual error_t SSPartitionAdd (const bps_sspartinit_data_t& data);

   virtual error_t SSPartitionFind(const bps_sspartition_id_t& find,
                                      BPSSSPartition*& sspart);
   virtual error_t SSPartitionDelete(const bps_sspartition_id_t& paluc);

   virtual error_t SSPartitionQuery(bps_info_request_t&,
                                        bps_info_reply_t*);
   virtual error_t getPolicy(const bps_request_t& request,
                             BPSPolicy*& policy, BPSSSPartition* sspart);
   virtual error_t addJob(BPSJob* job);
   virtual error_t cancelJob(const bps_request_t& request);
   virtual error_t handleDeadJobs();
   virtual error_t threadKill(int num_threads);
  
   virtual error_t threadCreate(int num_threads);

   virtual error_t modifyPartition(const bps_tune_data_t& data);
   virtual error_t changePermissions(const bps_permissions_msg_t& msg);
   virtual error_t resetDefault(const bps_sspartition_id_t& data);

public:
   // This actually creates the server, but creating the server
   // is a two step process.
   // You need to first create the server, then add the initial partitions
   // and then call start, which actually starts the server up
   BPSServer(const bps_sspartdata_t& data,
             int num_threads,
             caddr_t port_addr);
   ~BPSServer();
   // used to query the server as an entire system
   virtual error_t query(bps_info_request_t& req,
                         bps_info_reply_t*);
   virtual void start();
};
         

bool operator==(const bps_sspartition_id_t& x,  const bps_sspartition_id_t& y);
bool operator<(const bps_sspartition_id_t& x, const bps_sspartition_id_t& y);
#endif





