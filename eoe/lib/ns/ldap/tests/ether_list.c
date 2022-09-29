
#include <stdio.h>
#include <ns_api.h>
#include <errno.h>
#include <string.h>

void main(){

  char buffer[ 1024 ];
  int buflen = 1024;
  FILE * f;

  if(( f = ns_list( NULL, "ethers", NULL ))==NULL){

    fprintf( stdout, "main: ns_list() failed, errno=(%d,>%s<)\n",\
	     errno, strerror( errno ) );
    fflush( stdout );

  }else{

    while( fgets(buffer, buflen, f) != NULL ){

      fprintf( stdout, "main: %s\n", buffer );
      fflush( stdout );

    }

  }

}




