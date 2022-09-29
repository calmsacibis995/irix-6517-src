
#include <stdio.h>
#include <errno.h>
#include <string.h>

struct ether_addr {
        unsigned char ether_addr_octet[6];
};

char * ether_ntoa_r(struct ether_addr *, char *);
char * ether_ntoa(struct ether_addr *);
struct ether_addr * ether_aton_r(char *, struct ether_addr *);
struct ether_addr * ether_aton(char *);
int ether_hostton(char *, struct ether_addr *);
int ether_ntohost(char *, struct ether_addr *);

char HName[][ 40 ]={ "babylon", "vihar", "piecomputer", "machine",\
		     "manaslu", "neteng", "bonnie", "nunkun", "annapurna" };

void main(){

  int			i, n;
  struct ether_addr	a;

  n = sizeof( HName )/40;

  fprintf( stdout, "main: processing %d hosts\n", n );
  fflush( stdout );

  for( i = 0; i < n; i++ ){

    fprintf( stdout, "main: host->%s<\n", HName[ i ] );
    fflush( stdout );

    if( ether_hostton( HName[ i ], &a ) != 0 ){

      fprintf( stdout, "main: ether_hostton() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );


    }else{
    
      fprintf( stdout, "main: mac adds->%s<\n", ether_ntoa( &a ) );
      fflush( stdout );
    
    }

  }
  


}
