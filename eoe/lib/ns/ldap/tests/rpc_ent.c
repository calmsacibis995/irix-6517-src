#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

void main(){

  int			i, j;
  struct rpcent *	s;

  for( i = 0;; i++ ){
    
    fprintf( stdout, "main: retrieveing entry[ %d ]\n", i);

    if(( s = getrpcent())==NULL){
      
      fprintf( stdout, "main: getrpcent() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );
      break;

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
