/*
 * Test getprotobynumber, getprotobyname, and getprotoent.
 *
 */

int PNumb[]={ 0, 1, 2, 3, 6, 8, 12, 17, 20, 22, 27, 29 };

char PNam[][20]={ "ip", "icmp", "igmp", "ggp", "tcp", "egp", "pup",\
"udp", "hmp", "xns-idp", "rdp", "iso-tp4"};
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>

void main(){

  char *		a;
  int			j, i, n;
  struct protoent *	p;


  /* Get protocol by number */

  for( i = 0;; i++ ){

    fprintf( stdout, "main: protocol_entr=%d\n", i );

    if((p = getprotoent())==NULL){


      fprintf( stdout,\
	       "main: EOF or getprotoent() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );
      break;

    }else{
      
      /* Display results */

      fprintf( stdout, "main: p_name ->%s<\n", p->p_name ? p->p_name: "NULL" );
      fprintf( stdout, "main: p_proto->%d<\n", p->p_proto );

      for( j = 0; p->p_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: p_aliasses[%d]->%s<\n", j, p->p_aliases[ j ] );

      }
      
      fprintf( stdout, "\n" );

    }
    
    fflush( stdout );

  }

}

