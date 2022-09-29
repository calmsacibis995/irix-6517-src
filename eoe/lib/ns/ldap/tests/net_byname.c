#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

char NetN[][40]={"b43-sr-systems-2","b43-vlsi-mh-sys","b9u-tr2","b9u-tr3",\
"b7l-hippi","sgi109","terhorst-house","b2-hwlab2-temp","b17-corp-1",\
"b9-wpdlab5","allied","urbana","korea","b17u-dlabfddi","shore-nafo30",\
"dialbk-project4","b43-build","b7l-os","b7l-tpc1","everest-test",\
"b11-esd-ort","newport-bch1-net","b8u-ofc-net","b1-guiness-lab",\
"microwave-wbldT","harrisburg","ids-2","b8u-ipd","b9-empty73",\
"b7l-ssd-benchlab","b43-lab-sw-test","b43-lab-vlsi","b8-clv2",\
"b9l-isdn","b9l-sqa-fddi","b7l-asd-fddi","b9u-atm","b1-dss-msig-1",\
"b2u-south","clubfed-demo","appletalk_n7","wanhud-wanhan","wanbeth-wantim",\
"wanbeth-wanatl","frame-relay2","stlouis-sales","denver1","stlaurent",\
"knoxville","melbourne-net"};

void main(){

  int			n, i, j;
  struct netent *	s;
  struct in_addr	in;

  n = sizeof( NetN )/40;

  for( i = 0; i < n; i++ ){
  
    fprintf( stdout, "main: NetN(%s)\n", NetN[ i ] );
    fflush( stdout );

    if(( s = getnetbyname( NetN[ i ] ))==NULL){
      
      fprintf( stdout, "main: getnetbyname() failed, errno=(%d,>%s<)\n",\
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
