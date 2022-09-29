
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

char HAdds[][ 40 ]={ "8:0:69:4:15:73", "8:0:69:9:13:fb", "8:0:69:2:ec:64",\
		     "8:0:69:8:dd:9c", "8:0:69:4:15:4f", "8:0:69:4:48:71",\
		     "8:0:69:7:37:fe", "8:0:69:9:5:f2"};

void main(){

  int			i, n;
  struct ether_addr *	a;
  char			h[ 50 ];

  n = sizeof( HAdds )/40;

  fprintf( stdout, "main: processing %d addresses\n", n );
  fflush( stdout );

  for( i = 0; i < n; i++ ){

    fprintf( stdout, "main: Adds->%s<\n", HAdds[ i ] );
    fflush( stdout );

    a = ether_aton( HAdds[ i ] );
    
    h[0] = '\0';

    if( ether_ntohost( h, a ) != 0 ){

      fprintf( stdout, "main: ether_ntohost() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );


    }else{
    
      fprintf( stdout, "main: host-name->%s<\n", h );
      fprintf( stdout, "main: mac adds->%s<\n", ether_ntoa( a ) );
      fflush( stdout );
    
    }

  }
  


}
