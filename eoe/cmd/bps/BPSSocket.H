/****************************************************************/
/*    NAME:                                                     */
/*    ACCT: kostadis                                            */
/*    FILE: BPSSocket.H                                         */
/*    ASGN:                                                     */
/*    DATE: Sat Jul  8 15:13:58 1995                            */
/****************************************************************/


#ifndef BPSSOCKET_HEADER
#define BPSSOCKET_HEADER
#include <sys/types.h>
#include <Thread.H>
#include <bps.h>
#include <BPSPrivate.H>
class BPSSocket {
protected:
   int sockfd_; 
   Mutex mutex_; // protects the socket
   caddr_t addr_;
   BPSSocket(int sockfd, caddr_t name);
   BPSSocket& operator=(BPSSocket& sock);
   BPSSocket(BPSSocket& sock);
public:
   enum {BACK_LOG = 50};
   // creates a socket
   BPSSocket(caddr_t name);
   ~BPSSocket();
   // accepts a connection returning a new socket
   error_t accept(BPSSocket*& socket);
   // writes  the message to the socket using the code in BPSPrivate.H
   error_t write(bps_message_t* msg);
   // reads the data from the socket using the code in BPSPrivate.C
   error_t read(bps_message_t* msg);
   // used to lock and unlock the socket
   void lock();
   void unlock();
};

        

#endif
