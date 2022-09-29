#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

char NetN[][30]={"130.62.11","130.62.55","192.48.167","192.48.171",\
"150.166.162","192.82.178","192.132.152","192.132.171","198.29.65",\
"199.74.37","155.11.187","155.11.161","155.11.160","150.166.3",\
"150.166.232","150.166.251","130.62.40","192.48.150","192.48.180",\
"192.48.196","192.82.190","192.82.205","192.82.211","192.132.178",\
"155.11.159","204.94.223","150.166.40","150.166.73",\
"150.166.109","130.62.33","130.62.41","192.26.62","192.48.169","192.48.173",\
"192.48.174","192.48.175","192.132.174","192.132.176","192.132.203",\
"198.29.87","155.11.242","155.11.241","155.11.240","155.11.201","155.11.167",\
"155.11.156","155.11.153","155.11.140","192.68.139"};

void main(){

  int			n, i, j;
  struct netent *	s;
  u_long		net;
  struct in_addr	in;

  n = sizeof( NetN )/30;

  for( i = 0; i < n; i++ ){
    
    fprintf( stdout, "main: NetN(%s)\n", NetN[ i ] );
    fflush( stdout );

    net = inet_network( NetN[ i ] );

    if(( s = getnetbyaddr( net, AF_INET ))==NULL){
      
      fprintf( stdout, "main: getnetbyaddr() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );

    }else{

      fprintf( stdout, "main: n_name->%s<\n", s->n_name );
      fprintf( stdout, "main: n_addrtype->%s<\n",\
	       (s->n_addrtype==AF_INET)?"AF_INET":"UNKNOWN"  );
      fprintf( stdout, "main: n_net->%x<\n", s->n_net );
      in.s_addr = s->n_net;
      fprintf( stdout, "main: n_net->%s<\n",\
	       inet_ntoa( in ));     
      for( j = 0; s->n_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: n_aliases[%d]->%s<\n", j,\
		 s->n_aliases[ j ] );

      }
      
    }
    
  }

}




