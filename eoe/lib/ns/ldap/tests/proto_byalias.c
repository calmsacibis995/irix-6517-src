/*
 * Test getprotobynumber, getprotobyname, and getprotoent.
 *
 */

int PNumb[]={ 0, 1, 2, 3, 6, 8, 12, 17, 20, 22, 27, 29 };

char PNam[][20]={ "IP", "ICMP", "IGMP", "GGP", "TCP", "EGP", "PUP",\
"UDP", "HMP", "XNS-IDP", "RDP", "ISO-TP4"};
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>

void main(){

  char *		a;
  int			j, i, n;
  struct protoent *	p;


  n = sizeof( PNumb )/sizeof( int );

  fprintf( stdout, "main: testing %d protocols...\n", n );
  fflush( stdout );

  /* Get protocol by number */

  for( i = 0; i < n; i++ ){

    fprintf( stdout, "main: protocol_number[%d]=%d\n", i, PNumb[i] );

    if((p = getprotobyname( PNam[i] ))==NULL){


      fprintf( stdout, "main: getprotobyname() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );

    }else{
      
      /* Display results */

      fprintf( stdout, "main: key    ->%s<\n", PNam[i] );
      fprintf( stdout, "main: p_name ->%s<\n", p->p_name ? p->p_name: "NULL" );
      fprintf( stdout, "main: p_proto->%d<\n", p->p_proto );

      for( j = 0; p->p_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: p_aliasses[%d]->%s<\n", j, p->p_aliases[ j ] );

      }
      
      fprintf( stdout, "\n" );

    }
    
    fflush( stdout );

  }

  /* Get Protocol by name */


}

