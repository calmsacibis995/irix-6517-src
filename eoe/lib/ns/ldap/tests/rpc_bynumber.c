#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

long RPCN[]={100000,100002,100003,100005,100009,100017,100022,100024,100028,\
100029,391023,391026,391028,391032,391035,391037,391041,391044,391046,\
391056,391060,391061,391063,100007,100011,100012,100014,100018,100026,\
391002,391003,391004,391006,391007,391009,391014,391017,391019,391039,\
391048,391050,391053,391055,391059,391101,100001,100004,100016,100020,\
100021,100023,391001,391020,391021,391022,391024,391025,391027,391030,\
391031,391033,391034,391036,391040,391042,391043,391045,391057,391062,\
100008,100010,100013,100015,100019,391000,391005,391008,391012,391013,\
391015,391016,391018,391029,391038,391047,391051,391052,391054,391058};

void main(){

  int			n, i, j;
  struct rpcent *	s;

  n = sizeof( RPCN )/sizeof( long );

  for( i = 0; i < n; i++ ){
    
    fprintf( stdout, "main: RPC(%ld)\n", RPCN[ i ] );
    fflush( stdout );

    if(( s = getrpcbynumber( RPCN[ i ]))==NULL){
      
      fprintf( stdout, "main: getrpcbynumber() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );

    }else{

      fprintf( stdout, "main: r_name->%s<\n", s->r_name );
      fprintf( stdout, "main: r_number->%ld<\n", s->r_number );
      for( j = 0; s->r_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: r_aliases[%d]->%s<\n", j,\
		 s->r_aliases[ j ] );

      }

    }
    
  }

}
