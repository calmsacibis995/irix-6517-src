#include <pwd.h>
#include <stdio.h>


void main(){

  struct passwd *p;
    
  while(( p = getpwent() )!= NULL){

    fprintf( stdout, "main: pw_name->%s<\n", p->pw_name ? p->pw_name: "NULL" );
    fprintf( stdout, "main: pw_passwd->%s<\n",\
	     p->pw_passwd ? p->pw_passwd: "NULL" );
    fprintf( stdout, "main: pw_uid->%u<\n", p->pw_uid );
    fprintf( stdout, "main: pw_uid->%u<\n", p->pw_gid );
    fprintf( stdout, "main: pw_age->%s<\n", p->pw_age ? p->pw_age: "NULL" );
    fprintf( stdout, "main: pw_comment->%s<\n",\
	     p->pw_comment ? p->pw_comment: "NULL" );
    fprintf( stdout, "main: pw_gecos->%s<\n",\
	     p->pw_gecos ? p->pw_gecos: "NULL" );
    fprintf( stdout, "main: pw_dir->%s<\n",\
	     p->pw_dir ? p->pw_dir: "NULL" );
    fprintf( stdout, "main: pw_shell->%s<\n",\
	     p->pw_shell ? p->pw_shell: "NULL" );
    fflush( stdout );

  }

}
