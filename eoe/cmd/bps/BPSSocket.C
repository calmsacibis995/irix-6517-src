/****************************************************************
 *    NAME: 
 *    ACCT: kostadis
 *    FILE: BPSSocket.C
 *    DATE: Mon Jul 10 17:50:15 PDT 1995
 ****************************************************************/

#include "BPSSocket.H"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <bps.h>
#include <BPSPrivate.H>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <Debug.H>
BPSSocket::BPSSocket(int sockfd, caddr_t addr):sockfd_(sockfd){
   addr_ = (caddr_t) new struct sockaddr_in;
   memset(addr_,0,sizeof(sockaddr_in));
   assert(sizeof (*addr_) == sizeof(*addr));
   memcpy(addr_,addr,sizeof(struct sockaddr_in));
};

/*************************************************************************
 * Function Name: BPSSocket
 * Parameters: sockaddr* name
 * Returns: 
 * Effects: 
 *************************************************************************/

BPSSocket::BPSSocket(caddr_t addr)
{

   assert(addr != 0);
   addr_ = (caddr_t) malloc(sizeof(struct sockaddr_in));
   memset(addr_,0,sizeof(struct sockaddr_in));
   memcpy (addr_,addr,sizeof(struct sockaddr_in));
   if((sockfd_ =  socket(AF_INET,SOCK_STREAM,0)) < 0){
      perror("could not create socket");
      exit(1);
   }
   if ((bind(sockfd_,addr_,sizeof(struct sockaddr_in))) < 0){
      perror("could not bind to socket");
      exit(1);
   }
   
   if (listen(sockfd_,50) == -1){
      perror("could not listen");
      exit(1);
   }
}

BPSSocket::~BPSSocket(){
   close(sockfd_);
   delete addr_;
};

/*************************************************************************
 * Function Name: lock
 * Parameters: 
 * Returns: void 
 * Effects: 
 *************************************************************************/
void 
BPSSocket::lock()
{
   mutex_.lock();
}


/*************************************************************************
 * Function Name: unlock
 * Parameters: 
 * Returns: void 
 * Effects: 
 *************************************************************************/
void 
BPSSocket::unlock()
{
   mutex_.unlock();
}

/****************************************************************
 * Function Name:                                               
 * Parameters:                                                  
 * Returns:                                                     
 * Effects:                                                     
****************************************************************/

error_t BPSSocket::accept(BPSSocket*& socket){
   int new_sockfd;
   struct sockaddr_in *cli_addr = new struct sockaddr_in;
   memset(cli_addr,0,sizeof(sockaddr_in));
   int len = sizeof(*cli_addr);
   if ( (new_sockfd = ::accept(sockfd_,(caddr_t*) cli_addr,&len)) < 0){
      perror("could not call accept!");
      exit(1);
   }   
   if(new_sockfd == -1) {
      perror("could not accept");
      return BPS_ERR_BAD_SOCKET;
   }
   strp("Accepted function and returning new socket");
   socket = new BPSSocket(new_sockfd,(caddr_t) cli_addr);
   return BPS_ERR_OK;
};

error_t BPSSocket::read(bps_message_t* msg){
   return bps_messageserver_recv(sockfd_,msg);
};

error_t BPSSocket::write(bps_message_t* msg){
   return bps_messageserver_send(sockfd_,msg);
};



   
   







