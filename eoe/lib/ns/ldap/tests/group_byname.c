/*
 * Test getprotobynumber, getprotobyname, and getprotoent.
 *
 */

char GNam[][20]={ "daemon", "adm", "other", "demos", "mfg", "csg_mktg",\
		  "www-ssd" };

#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <grp.h>


void main(){

  int			j, i, n;
  struct group *	g;

  n = sizeof( GNam )/20;

  fprintf( stdout, "main: testing %d groups...\n", n );
  fflush( stdout );

  /* Get protocol by number */

  for( i = 0; i < n; i++ ){

    fprintf( stdout, "main: group name[%d]=%s\n", i, GNam[i] );

    if((g = getgrnam( GNam[i] ))==NULL){


      fprintf( stdout, "main: getgrnam() failed, errno=(%d,>%s<)\n",\
	       errno, strerror( errno ) );

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

  /* Get Protocol by name */

}

