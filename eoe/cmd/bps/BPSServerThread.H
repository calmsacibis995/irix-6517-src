/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSServerThread.H                                   */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 16:33:14 1995                            */
/****************************************************************/


#ifndef BPSSERVERTHREAD_HEADER
#define BPSSERVERTHREAD_HEADER
#include <bps.h>
#include <BPSPrivate.H>
#include <Thread.H>

class BPSServer;
class BPSSocket;

/****************************************************************
 * CLASS NAME : BPSServerThread                                 

****************************************************************/


class BPSServerThread : public JoinableThread{
   BPSServer* server_; // pointer to the server
   BPSSocket* socket_; // the socket associated with the client being
                       // served
   Mutex kill_mutex_;  // protects the is kill
   bool_t iskill_;
   virtual void startup(); // 
public:
   // creates a server thread
   BPSServerThread(BPSServer* server);
   // handles query requests
   virtual void query(bps_info_request_t& info);
   // handles cancel requests
   virtual void cancel(bps_request_t& req);
   // handles system requests
   virtual void system(bps_system_message_t& sys_msg);
   // handles submit requests
   virtual void submit(bps_request_t& req);
   // handles returning an error to the user
   virtual void error(error_t error);
   // used to set the isKill_ flag to true
   virtual void setKill();
   // used to check the status of the kill flag
   virtual bool_t checkKill();
};


#endif
