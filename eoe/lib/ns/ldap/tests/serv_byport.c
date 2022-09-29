#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int STCP[]={9,21,23,25,57,80,87,95,101,110,119,526,556,760,544,6000,5025,3881,\
3882,2365,2401,1666,7,17,43,77,103,109,135,512,513,530,531,1524,601,750,5232,\
5434,8778,1001,4557,5234,999,3878,2345,1,15,79,115,515,761,2105,2325,11,20,\
102,105,111,117,143,514,532,543,5019,2305,3601,13,19,37,42,53,104,113,540,\
32769,2033};

int SUDP[]={7,123,517,533,750,5135,5144,1645,39,53,135,162,177,513,5141,5143,\
13,19,37,525,5131,5136,5140,1646,68,69,111,512,514,520,5133,5137,5142,5555,\
5034,3879,9,67,121,161,518,371,5130};

void main(){

  int			n, i, j;
  struct servent *	s;

  n = sizeof( STCP )/sizeof( int );

  for( i = 0; i < n; i++ ){
    
    fprintf( stdout, "main: TCP(%d)\n", STCP[ i ] );
    fflush( stdout );

    if(( s = getservbyport( STCP[ i ], "tcp"))==NULL){
      
      fprintf( stdout, "main: getservbyport() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );

    }else{

      fprintf( stdout, "main: s_name->%s<\n", s->s_name );
      fprintf( stdout, "main: s_port->%d<\n", s->s_port );
      fprintf( stdout, "main: s_proto->%s<\n", s->s_proto );
      for( j = 0; s->s_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: s_aliases[%d]->%s<\n", j,\
		 s->s_aliases[ j ] );

      }
      
    }

  }

  n = sizeof( SUDP )/sizeof(int);

  for( i = 0; i < n; i++ ){
    
    fprintf( stdout, "main: UDP(%d)\n", SUDP[ i ] );
    fflush( stdout );

    if(( s = getservbyport( SUDP[ i ], "udp"))==NULL){
      
      fprintf( stdout, "main: getservbyport() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );

    }else{

      fprintf( stdout, "main: s_name->%s<\n", s->s_name );
      fprintf( stdout, "main: s_port->%d<\n", s->s_port );
      fprintf( stdout, "main: s_proto->%s<\n", s->s_proto );
      for( j = 0; s->s_aliases[ j ] != NULL; j++ ){

	fprintf( stdout, "main: s_aliases[%d]->%s<\n", j,\
		 s->s_aliases[ j ] );

      }
      
    }

  }

}
