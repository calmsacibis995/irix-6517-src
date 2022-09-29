#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

char HostA[][30]={"192.48.174.2","150.166.61.60","150.166.75.28",\
"150.166.75.20","150.166.75.14","192.26.80.10","192.26.80.202",\
"150.166.75.45"};

void main(){

  int			n, i, j;
  struct hostent  *	s;
  int			addrlen;
  struct in_addr	in;
  struct in_addr *	inp;

  n = sizeof( HostA )/30;

  for( i = 0; i < n; i++ ){
    
    fprintf( stdout, "main: HostA(%s)\n", HostA[ i ] );
    fflush( stdout );

    inet_aton( HostA[ i ], &in );
    addrlen = sizeof(struct in_addr);

    if(( s =gethostbyaddr( (void *)&in, addrlen, AF_INET))==NULL){
    
      fprintf( stdout, "main: gethostbyaddr() failed, errno=(%d,>%s<)\n",\
	       h_errno, hstrerror( errno ) );

    }else{

      fprintf( stdout, "main: h_name->%s<\n", s->h_name );
      fprintf( stdout, "main: h_length->%u<\n", s->h_length );
      fprintf( stdout, "main: h_addrtype->%s<\n",\
	       (s->h_addrtype==AF_INET)?"AF_INET":"UNKNOWN"  );

      for( j = 0; s->h_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: h_aliases[%d]->%s<\n", j,\
		 s->h_aliases[ j ] );

      }

      for( j = 0; s->h_addr_list[ j ] != NULL; j++ ){
	
	inp = (struct in_addr *)s->h_addr_list[ j ]; 
	fprintf( stdout, "main: h_addr_list[%d]->%x<\n", j, inp->s_addr );
	fprintf( stdout, "main: h_addr_list[%d]->%s<\n", j,\
		 inet_ntoa( *inp ));     

      }

    }
    
  }

}
