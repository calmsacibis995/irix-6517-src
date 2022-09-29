#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


char RPCN[][30]={"portmapper","rusersd","nfs","mountd","yppasswdd","rexd",\
"x25.inr","status","ypupdated","keyserv","sgi_reserved","ypbind","rquotad",\
"sprayd","rje_mapper","alis","bootparam","sgi_fam","sgi_notepad","sgi_mountd",\
"sgi_pcsd","sgi_nfs","sgi_pod","quorumd","rstatd","ypserv","database_svc",\
"llockmgr","nlockmgr","statmon","sgi_toolkitbus","walld","etherstatd",\
"3270_mapper","selection_svc","sched","sgi_snoopd","sgi_smtd",\
"sgi_rfind"};

void main(){

  int			n, i, j;
  struct rpcent *	s;

  n = sizeof( RPCN )/30;

  for( i = 0; i < n; i++ ){

    fprintf( stdout, "main: RPC(%s)\n", RPCN[ i ] );
    fflush( stdout );

    if(( s = getrpcbyname( RPCN[ i ] ))==NULL){
      
      fprintf( stdout, "main: getrpcbyname() failed, errno=(%d,>%s<)\n",\
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
