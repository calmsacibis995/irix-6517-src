#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

void main(){

  int			i, j;
  struct servent *	s;

  for( i = 0;; i++ ){
    
    fprintf( stdout, "main: retrieveing entry[ %d ]\n", i );

    if(( s = getservent())==NULL){
      
      fprintf( stdout, "main: getservent() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );
      break;

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
