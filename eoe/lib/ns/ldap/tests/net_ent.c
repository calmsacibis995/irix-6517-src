#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void main(){

  int			i, j;
  struct netent  *	s;
  struct in_addr	in;

  for( i = 0;; i++ ){
    
    fprintf( stdout, "main: retrieveing entry[ %d ]\n", i);

    if(( s = getnetent())==NULL){
      
      fprintf( stdout, "main: getnetent() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );
      break;

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
