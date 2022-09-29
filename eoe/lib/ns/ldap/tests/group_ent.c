/*
 * Test getprotobynumber, getprotobyname, and getprotoent.
 *
 */

int GId[]={ 1, 3, 995, 997, 16, 18, 41, 43, 170, 370 , 372, 5, 59999, 6, 12,\
44, 100, 102, 500, 555, 1000, 0, 4, 8, 10, 15};

#include <grp.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>

void main(){

  int			j, i, n;
  struct group *	g;

  /* Get protocol by number */

  for( i = 0;; i++ ){

    fprintf( stdout, "main: group-entry[%d]\n", i );

    if((g = getgrent())==NULL){


      fprintf( stdout,\
	       "main: EOF or getgrent() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );
      break;

    }else{
      
      /* Display results */

      fprintf( stdout, "main: grp name ->%s<\n", g->gr_name );
      fprintf( stdout, "main: grp name ->%s<\n", g->gr_passwd );
      fprintf( stdout, "main: grp id   ->%u<\n", (u_long)g->gr_gid );


      for( j = 0; g->gr_mem[ j ] != NULL; j++ ){

	fprintf( stdout, "main: gr_mem[%d]->%s<\n", j, g->gr_mem[ j ] );

      }
      
      fprintf( stdout, "\n" );


    }
    
    fflush( stdout );

  }

}

