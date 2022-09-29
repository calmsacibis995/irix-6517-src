/****************************************************************
 *    NAME: 
 *    ACCT: kostadis
 *    FILE: BPSServerThread.C
 *    DATE: Mon Jul 10 17:50:15 PDT 1995
 ****************************************************************/

#include <BPSServerThread.H>
#include <BPSServer.H>
#include <BPSPrivate.H>
#include <BPSJob.H>
#include <BPSJobSchedule.H>
#include <Thread.H>
#include <BPSSocket.H>
#include <BPSPolicy.H>
#include <BPSPolicyManager.H>
#include <Debug.H>

extern "C" {
   #define uuid_s_ok 0
   boolean_t uuid_is_nil(uuid_t* uuid, uint_t* status);
};

/****************************************************************
 * Function Name:  BPSServerThread                              
 * Parameters: BPSServer*                                       
 * Returns:                                                     
 * Effects: creates a joinable thread that is in the suspended state 

****************************************************************/



BPSServerThread::BPSServerThread(BPSServer* server):JoinableThread(),
server_(server)
{
}

  
/****************************************************************
 * Function Name:startup                                               
 * Parameters:                                                  
 * Returns:                                                     
 * Effects: When makeRunnable is called by the BPSServer::startup
            this method gets called and is the 'main loop' of the
  BPS. The server thread is in a do-while loop that at the top 
  grabs the server socket lock and processes a request, and at the bottom
  checks wether it was slated to die. 
  1. Grab the socket lock (if already owned by somebody wait on it)
  2. Accept the incoming request
  3. Release the socket lock, and process requests from the socket
     returned by accept.
  4. Switch on the message type
     By the time the thread returns the socket had better be deleted
     and closed.
  5. Get any dead jobs from the kernel and try to delete them
  6. Check if the thread has been slated to die, if so then exit
     otherwise continue

 If an error occurs in a read or a write, it is unclear what exactly
 should happen. Realistically a failure on a read or a write should
 never happen because there is no network, but in IRIX anything is
 possible.
****************************************************************/



void 
BPSServerThread::startup()
{
   error_t err;
   // 1. On startup tries to get a lock on the socket.
   // 2. If it gets a lock it reads the data from the socket
   do{
      TRACE(server_->socket_->lock());
      TRACE(err = server_->socket_->accept(socket_));
   // XXX What happens in the case of error?
      if(err != BPS_ERR_OK){  
         bps_error_print(err);
         server_->socket_->unlock();
         continue; 
      }
      server_->socket_->unlock();   
      bps_message_t data;
   // XXX What happens in the case of error?
   // probably should return an error... but then what if the client
   // died or something else happened?
      assert(socket_->read(&data) == BPS_ERR_OK);
      // 3 if the message is a query
      switch(data.msg){
      case BPS_MSG_QUERY:
         query( data.bps_msg_data.info);
         break;
      case BPS_MSG_SYSTEM:
         system( data.bps_msg_data.sys_msg);
         break;
      case BPS_MSG_SUBMIT:
         submit( data.bps_msg_data.req);
      break;
      case BPS_MSG_CANCEL:
         cancel( data.bps_msg_data.req);
         break;
      default:
         error(BPS_ERR_BAD_MSG_TYPE);
      };

   }while(!checkKill());
   exit();
};

/*************************************************************************
 * Function Name: system
 * Parameters: bps_system_message_t& sys_msg
 * Returns: void 
 * Effects: Handles system requests
   
   This method switches on system message, calls the appropriate
   server method and returns to startup
 *************************************************************************/
void BPSServerThread::system (bps_system_message_t& sys_msg){
   bps_reply_t reply;
   error_t err;
   switch(sys_msg.msg){
   case BPS_SYS_SSPART_CREATE:
      server_->SSPartitionAdd(sys_msg.bps_msg_data.init_data);
      break;
   case BPS_SYS_SSPART_DELETE:
      if(( err = server_->SSPartitionDelete(sys_msg.bps_msg_data.uuid)) !=
         BPS_ERR_OK){
         error(err);
         return;
      };
      break;
   case BPS_SYS_THREAD_CREATE:
      if(( err = server_->threadCreate(sys_msg.bps_msg_data.num_threads)) != BPS_ERR_OK){
         error(err);
         return;
      };
      break;
   case BPS_SYS_THREAD_KILL:
      if(( err = server_->threadKill(sys_msg.bps_msg_data.num_threads)) != BPS_ERR_OK){
         error(err);
         return;
      };
      break;
   case BPS_SYS_MODIFY_PARTITION:
      if(( err = server_->
          modifyPartition(sys_msg.bps_msg_data.tune_data))!= BPS_ERR_OK){
         error(err);
         return;
      };
      break;
   case BPS_SYS_CHANGE_PERMISSIONS:
      if(( err = server_->
          changePermissions(sys_msg.bps_msg_data.perm_msg)) != BPS_ERR_OK){
         error(err);
         return;
      }
      break;
   case BPS_SYS_CHANGE_DEFAULT:
      if((err = server_->
          resetDefault(sys_msg.bps_msg_data.uuid)) != BPS_ERR_OK){
         error(err);
         return;
      }
   default:
      error(BPS_ERR_BAD_MSG);
   };

   delete socket_;
   socket_ = (BPSSocket*) 0xA3;
   return;
};
         

 /*************************************************************************
 * Function Name: submit
 * Parameters: bps_request_t
 * Returns: void 
 * Effects: 

     This method handles the mechanics of the submit command. First a
  few words to our sponsors: The big idea was to make it possible to
  have any number of policies that could be as flexible as possible in
  their selection of jobs. As an optimal allocation is an NP complete
  problem, there are an infinite possible of sub optimal
  allocations. So to make the mechanism extremely flexible the type of
  policy is chosen at run time. To make that possible, I used the
  'exemplar' idiom from Advanced C++ Idioms (Coplien) also known as
  the 'prototype' pattern in Design Patterns (Gamma, Vlissides
  etal). The idea is to make the type an object (much like Smalltalk
  and Self) and use the type object to generate other objects. To pull
  that off, at startup a list of policy objects is created using
  the trick found in Coplien and each new policy object is created by cloning
  from that list of policy types.
  
    The method thus works like this:
    1. First get the partitition id
       If it is null, then the default partition is used.
       And get the associated partition which is read locked. The partition has
       a readers priority read-writers lock to protect it from being deleted while
       still in use. The readers priority lock guarantees that the writer is the 
       last person to access the partition, which means that the partition can
       be safely deleted. Note, that each job has a read lock on the partition
       it is associated, and part of the deallocation is to release that read lock.
    2. Then get a policy object of the specified type using the policy manager
    3. Then reset the current time of the partition. You need to do this first
       because an unused partition might have a current time that is significanlty
       in the past. Since the current time is the time used to notify the kernel 
       when a job terminates it is important that the time be in sync with the kernel 
       time
    
    4. Call policy->search to find a job
    5. Call policy->getJob to get a job
       The search and getJob are split to make it possible to use the same 
       search engines to generate different jobs (an engine could find multiple
       jobs) but the getJob get a job that is optimal based on different
       requirements. Note also that the locking scheme used on the partition depends
       on the policy. For example a partition which had very many small jobs might
       find it more economical to use locks on each partition unit, wherease one that
       handled long jobs would find that uneconomical because of all the backtracking.
       It is incumbent on the policy to handle locking so that data corruption or dead
       lock does not occur.
    6. Add the job to the job schedule and release the partition read lock. (from above
       you might be wondering what about the JOB? Well the job has its own read lock).
   
 *************************************************************************/
void BPSServerThread::submit(bps_request_t& request){
  BPSPolicy* policy;
  uint_t status;
  BPSSSPartition* sspart;
  if(uuid_is_nil(&request.sspart,&status) && (status == uuid_s_ok)){
     request.sspart = server_->default_;
  };
  // returns the partition read locked
  if(!server_->SSPartitionFind(request.sspart,sspart)){
     error(BPS_ERR_BAD_POLICY);
     return;
  }
  error_t err;
  // returns an object of the desired type
  if(( err= server_->getPolicy(request,policy,sspart)) != BPS_ERR_OK){
     error(err);
     return;
  }

  // lock the partition values to reset the time
  sspart->values_wlock();
  // reset time
  struct timeval time;
  gettimeofday(&time,0);

  // only reset the time if an interval has passed. because it is a fairly
  // expensive operation
  if((tstoS(time) % sspart->duration()) / sspart->interval() > sspart->interval() ){
     sspart->resetTime(tstoS(time));
  };

  // if the window = default then the user wants the whole duration used
  if(request.window == BPS_WINDOW_DFLT){
     // need to subtract one interval because
     // otherwise will go right up to the current time because time starts at 1
     // not 0
     request.window = sspart->duration() - sspart->interval();
  };

  if((err = policy->search()) != BPS_ERR_OK){
     error(err);
     sspart->values_unlock();
     delete policy;
     return;
  }

  BPSJob* job;
  if((err = policy->getJob(job,request.job_id)) != BPS_ERR_OK){
     error(err);
     sspart->values_unlock();
     delete policy;
     return;
  }

  if((err = server_->addJob(job)) != BPS_ERR_OK){
     delete policy;
     error(err);
     return;
  }
  sspart->unlock(); // need to unlock it, at this point if no more references
                    // to the job existed the job could be deleted
  // do the reply stuff and kernel communication
  server_->handleDeadJobs();
  delete socket_;
  delete policy;
  socket_ = (BPSSocket*) 0xA3;
  return;
};

/*************************************************************************
 * Function Name: cancel
 * Parameters: bps_request_t
 * Returns: void 
 * Effects: Handles canceling a request, Cancelation is much simpler than
            submit. All that takes place, is that the job is found from the 
   schedule and then deleted. 
 *************************************************************************/
void BPSServerThread::cancel(bps_request_t& request){
   error_t err;
   if((err = server_->cancelJob(request)) != BPS_ERR_OK){
      error(err);
      return;
   }
  
   bps_message_t msg;
   msg.bps_msg_data.reply.err = BPS_ERR_OK;
   delete socket_;
   socket_ = (BPSSocket*) 0xA3;
   return;
}

/*************************************************************************
 * Function Name: query
 * Parameters: 
 * Returns: void 
 * Effects:  Forwards the request to the Server
 *************************************************************************/
void BPSServerThread::query (bps_info_request_t& req){
   error_t err;
   bps_info_reply_t * reply = (bps_info_reply_t*) 0xA3;
   switch(req.flag){
   case BPS_QUERY_SSPART:
      if((err = server_->SSPartitionQuery(req,
                                          reply)) != BPS_ERR_OK){
         error(err);
         return;
      };
      break;
   case BPS_QUERY_SYS:
      if((err = server_->query(req, reply)) != BPS_ERR_OK){
         error(err);
         return;
      };
      break;
   default:
      error(BPS_ERR_BAD_MSG);
      return;
   }
   // do reply stuff
   assert(socket_ != (BPSSocket*) 0xA3);
   delete socket_;
   socket_ = (BPSSocket*) 0xA3;
   return;
};




void BPSServerThread::setKill(){
   kill_mutex_.lock();
   iskill_ = 1;
   kill_mutex_.unlock();
};

bool_t BPSServerThread::checkKill(){
   bool_t retval;
   kill_mutex_.lock();
   retval = iskill_;
   kill_mutex_.unlock();
   return retval;
};

/****************************************************************
 * Function Name: error                                          
 * Parameters:                                                  
 * Returns:                                                     
 * Effects:  Returns an error message to the user and deletes   
             the socket associated with the communication
****************************************************************/



void BPSServerThread::error(error_t error){
   bps_message_t msg;
   msg.msg = BPS_MSG_ERROR;
   msg.bps_msg_data.reply.err = error;
   socket_->write(&msg);
   delete socket_;
   socket_ = (BPSSocket*) 0xA3;
   return;
}






