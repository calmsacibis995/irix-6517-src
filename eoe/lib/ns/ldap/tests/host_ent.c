#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

extern int h_errno;

void main(){

  int			i, j;
  struct hostent *	s;
  struct in_addr *	in;

  for( i = 0;; i++ ){
    
    fprintf( stdout, "main: retrieveing entry[ %d ]\n", i);

    if(( s = gethostent())==NULL){
      
      fprintf( stdout, "main: gethostent() failed, errno=(%d,>%s<)\n",\
	       h_errno, hstrerror( errno ) );
      break;

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

