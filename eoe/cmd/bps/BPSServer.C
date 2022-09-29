/****************************************************************
 *    NAME: 
 *    ACCT: kostadis
 *    FILE: BPSServer.C
 *    DATE: Mon Jul 10 17:50:15 PDT 1995
 ****************************************************************/

#include "BPSServer.H"
#include <BPS_lib.H>
#include <BPSJobSchedule.H>
#include <BPSSocket.H>
#include <bps.h>
#include <BPSSSPartManager.H>
#include <BPSPolicy.H>
#include <BPSPolicyManager.H>
extern "C" {
   void uuid_create_nil (uuid_t *uuid, uint_t *status);
   boolean_t uuid_is_nil   (uuid_t *uuid, uint_t *status);
   #define uuid_s_ok 0
};

/*************************************************************************
 * Function Name: SSPartitionFind
 * Parameters: const bps_SSPartition_id_t&  sspart
               BPSSSPartition* sspart 
 * Returns: bool_t 
 * Effects: returns the specified sspart read locked. Note that the manager
            handles its own locking, so the server does not have to lock it.
 *************************************************************************/
error_t BPSServer::SSPartitionFind(const bps_sspartition_id_t& sspart_id,
                                      BPSSSPartition*& sspart){
   error_t err;
   if((err = manager_->find(sspart_id,
                  sspart)) != BPS_ERR_OK){
      return err;
   }
   if(err != BPS_ERR_OK){
      sspart->unlock();
      sspart = 0;
      return err;
   }
   
   return BPS_ERR_OK;
};

/****************************************************************
 * Function Name: SSPartitionAdd                                
 * Parameters:                                                   
 * Returns: BPS_ERR_BAD_SSPART_ID if the partition id already exists
 * Effects:  Adds a new partition of the specified type to the 
             list of active partition. The new partition has the partition
  id specified by the user.
****************************************************************/




error_t BPSServer::SSPartitionAdd(const bps_sspartinit_data_t& data){

   if(bps_sspartid_equal((bps_sspartition_id_t*) &data.part_id,
                         (bps_sspartition_id_t*) &default_)){
      return BPS_ERR_BAD_SSPART_ID;
   }
   BPSSSPartition* sspart;
   if(manager_->find(data.part_id, sspart) == BPS_ERR_OK){
      sspart->unlock();
      return BPS_ERR_BAD_SSPART_ID;
   };
   error_t err;
   // NOTE: no locking necessary since the manager handles that
   err = manager_->add(data.type_id,
                 data.part_id,
                 data.filename);


   return BPS_ERR_OK;
};
     

      
/*************************************************************************
 * Function Name: SSPartitionDelete
 * Parameters: bps_sspartition_id_t sspart
 * Returns: error_t
 * Effects: deletes a SSPartition. The effect of removing it from the
            list of active partitions managed causes the resources to be deallocated.
 *************************************************************************/
error_t 
BPSServer::SSPartitionDelete(const bps_sspartition_id_t&  sspart_id)
{
   BPSSSPartition* sspart;
   manager_->remove(sspart_id,
                    sspart);
   delete sspart;
   return BPS_ERR_OK;
}


/*************************************************************************
 * Function Name: threadKill
 * Parameters: int num_threads
 * Returns: error_t 
 * Effects: 
       First checks that there exists sufficient resources -1. YOu need
       one left over because that is this thread
       If there are enough threads kill them use the setKill
       If two thread kill's are called to delete all threads but 1, then
  deadlock occurs, but since only one person should be using this function,
  that should not be problem.
 *************************************************************************/
error_t 
BPSServer::threadKill(int num_threads)
{
   if((num_threads <= 0) || (num_threads >= server_threads_.size())){
      return BPS_ERR_TOO_MANY_THREADS;
   }
   serverlist::iterator i = server_threads_.begin();
   BPSServerThread* thread;
   for(int b = 0; b < num_threads; b++){
      i++;
      (*i)->setKill(); // sets the dead flag
      (*i)->join(); // wait until the flag dies
      server_threads_.erase(i);
      delete thread;
   };
   return BPS_ERR_OK;
}


/*************************************************************************
 * Function Name: threadCreate
 * Parameters: int num_threads
 * Returns: bool_t 
 * Effects: just create the damn threads, add them to the thread list
            and start them up
 *************************************************************************/
error_t 
BPSServer::threadCreate(int num_threads)
{
   serverlist::iterator b = server_threads_.begin();
   for(int i = 0; i < num_threads; i++){
     BPSServerThread* thread = new BPSServerThread(this);
     server_threads_.insert(b,thread);
     thread->makeRunnable();
  };
   return BPS_ERR_OK;
}

/***************************************************************
 * Function Name: modifyPartitions                              
 * Parameters: bps_tune_data_t& data                            
 * Returns:                                                     
 * Effects:  Since each partition can be very different and the 
   mechanism for its modification may be very different, rather than
   use a functional specification, what is done is the following:
   A char* block of data and size is passed to the partition whose job
   it is to unmarshall the data and perform the modification itself.
   To handle the default case, we will provide client functions that do the
   marshalling client side, but will also provide the raw interface that
   takes a char* and size.
****************************************************************/



error_t BPSServer::modifyPartition(const bps_tune_data_t& data){
   BPSSSPartition* sspart;
   error_t err;
   // find the partition
   err = manager_->find(data.part_id,sspart);
   if(err != BPS_ERR_OK){
      sspart->unlock();
      return err;
   }
   // ask the partition to modify itself
   err = sspart->modify(data.data, data.size,manager_);
   sspart->unlock();
   return err;
};

   
/*************************************************************************
 * Function Name: query
 * Parameters: bps_info_reply_t*
 * Returns: bool_t 
 * Effects: Calls manager_->queryAll and returns data about all the partitions
 *************************************************************************/
error_t 
BPSServer::query(bps_info_request_t& req, bps_info_reply_t* reply)
{
   error_t err;
   err = manager_->queryAll(req,reply);
   return err;
}


/****************************************************************
 * Function Name: BPSServer
 * Parameters:                                                  
 * Returns:                                                     
 * Effects:                                                     
****************************************************************/




BPSServer::BPSServer(const bps_sspartdata_t &data, // data for the default partition
                     int num_threads, // how many server threads
                     caddr_t port_addr) // which port to bind to 
{
   uint_t status;
   // the default partition is set to null to handle startup, remember
   // until the partition is added, it is not part of the system
   uuid_create_nil((bps_sspartition_id_t*) &default_,&status);
   if(status != uuid_s_ok){
      assert(1==0);
   };
   // create the manager, the schedule and the socket
   manager_ = new BPSSSPartManager(data);
   schedule_ = new BPSJobSchedule();
   socket_ = new BPSSocket(port_addr);
   // create the threads but do NOT start them up, since there are no
   // partitions yet
   for(int i = 0; i < num_threads; i++){
      BPSServerThread* server_thread = new BPSServerThread(this);
      server_threads_.insert(server_threads_.begin(),server_thread);
   }
};
BPSServer::~BPSServer(){
   delete socket_;
   delete schedule_;
   delete manager_;
};

/****************************************************************
 * Function Name: start                                         
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: Starting the server is a two step process since adding
            partitions is error prone, and exceptions are not 
  yet implemented, you first create the server, then add the partitions
  then set the default partition and then call start.
****************************************************************/



void BPSServer::start(){
  // start the threads, presumably the threads
  for(serverlist::iterator i = server_threads_.begin();
      i != server_threads_.end(); i++){
     (*i)->makeRunnable();
  }
  
};

/****************************************************************
 * Function Name: SSPartitionQuery                                          
 * Parameters:                                                  
 * Returns:                                                     
 * Effects:                                                     
****************************************************************/




error_t BPSServer::SSPartitionQuery(bps_info_request_t& req,
                                    bps_info_reply *reply) 
{
   BPSSSPartition* sspart;
   error_t err;
   err = manager_->find(req.id,sspart);
   if(err != BPS_ERR_OK){
      return err;
   };
   // the partition handles the interior locking
   err = sspart->query(req,reply);
   if(err != BPS_ERR_OK){
      return err;
   };
   sspart->unlock(); // have to unlock the partiion because find returns
                     // it locked
   return BPS_ERR_OK;
}



bool operator==(const bps_sspartition_id_t& x,  const bps_sspartition_id_t& y) {
   return bps_sspartid_equal((bps_sspartition_id_t*) &x, (bps_sspartition_id_t*)&y);
};


bool operator>(const bps_sspartition_id_t& x, const bps_sspartition_id_t& y){
   return !(bps_sspartid_greaterthan((bps_sspartition_id_t*)&x, (bps_sspartition_id_t*) &y));
};

/****************************************************************
 * Function Name:resetDefault                                   
 * Parameters:                                                  
 * Returns:                                                     
 * Effects:  Sets the default partition. If the default partition
             is null, then you have to memcpy the id onto the default
  partition and simply assign because of the way the null uuid is handled
  
****************************************************************/

error_t BPSServer::resetDefault(const bps_sspartition_id_t& id){
   uint_t status;
   // if the default partition is nil 
   if(uuid_is_nil(&default_,&status) && (status == uuid_s_ok)){
      assert(sizeof(default_) == sizeof(id));
      memcpy(&default_,&id,sizeof(default_));
      return BPS_ERR_OK;
   };
   values_.lock();
   default_ = id;
   values_.unlock();
   return BPS_ERR_OK;
};

/****************************************************************
 * Function Name: changePermissions                             
 * Parameters:                                                  
 * Returns:                                                     
 * Effects:  First find the right partition and then calls 
             changePermissions on it. The permissions are as complex
  to modify as the partition itself, so the same trick is used : a char*
  is passed that the partition is required to unmarshall and process.
****************************************************************/



error_t BPSServer::changePermissions(const bps_permissions_msg_t& msg){
   BPSSSPartition* sspart;
   error_t err;
   err = SSPartitionFind(msg.part_id,sspart);
   if(err != BPS_ERR_OK){
      return err;
   };
   err = sspart->changePermissions(msg);
   sspart->unlock();
   return err;
}


error_t BPSServer::getPolicy(const bps_request_t& request,
                             BPSPolicy*& policy, BPSSSPartition* sspart){
   return policy_->clone(request,
                         sspart,
                         request.policy,
                         policy);
};

error_t BPSServer::addJob(BPSJob* job){
  schedule_->wlock();
  error_t err;
  if((err = schedule_->addJob(job)) != BPS_ERR_OK){
     schedule_->unlock();
     delete job; // have to delete the job because it can not be added
     return err;
  }
  schedule_->unlock();
  return BPS_ERR_OK;
};

error_t BPSServer::handleDeadJobs(){
   // DOES STUFF HERE;
   return BPS_ERR_OK;
};

error_t BPSServer::cancelJob(const bps_request_t& request){
   schedule_->wlock();
   error_t err;
   BPSJob* job;
   if((err = schedule_->deleteJob(job,request.job_id))
      != BPS_ERR_OK){
      schedule_->unlock();
      return err;
   }
   schedule_->unlock();
   delete job;
   return BPS_ERR_OK;
}
