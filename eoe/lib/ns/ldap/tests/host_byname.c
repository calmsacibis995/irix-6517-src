#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

char HostN[][40]={"babylon","vihar","piecomputer","machine","manaslu",\
		  "neteng","bonnie-80","nunkun","annapurna"};

void main(){

  int			n, i, j;
  struct hostent  *	s;
  struct in_addr *	in;

  n = sizeof( HostN )/40;

  for( i = 0; i < n; i++ ){
  
    fprintf( stdout, "main: HostN(%s)\n", HostN[ i ] );
    fflush( stdout );

    if(( s = gethostbyname( HostN[ i ] ))==NULL){
      
      
      fprintf( stdout, "main: gethostent() failed, errno=(%d,>%s<)\n",\
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
	
	in = (struct in_addr *)s->h_addr_list[ j ]; 
	fprintf( stdout, "main: h_addr_list[%d]->%x<\n", j, in->s_addr );
	fprintf( stdout, "main: h_addr_list[%d]->%s<\n", j,\
		 inet_ntoa( *in ));     

      }
    }

  }


}
