#include <pwd.h>
#include <stdio.h>

char PNam[][40]={"pschoppe","pomes","byee","djh","monte","farrahs","ana",\
		 "jedmonds","GONE-billm","euc14","narula","jes","mende",\
		 "eddiem","gomez"};

void main(){

  int			i, n;
  struct passwd *	p;

  n = sizeof( PNam )/40;

  for( i = 0; i < n; i ++ ){

    fprintf( stdout, "main: retrieving entry for->%s<\n", PNam[i] );

    if(( p = getpwnam(PNam[i]) )!= NULL){
      
      fprintf( stdout, "main: pw_name->%s<\n",\
	       p->pw_name ? p->pw_name: "NULL" );
      fprintf( stdout, "main: pw_passwd->%s<\n",\
	       p->pw_passwd ? p->pw_passwd: "NULL" );
      fprintf( stdout, "main: pw_uid->%u<\n", p->pw_uid );
      fprintf( stdout, "main: pw_uid->%u<\n", p->pw_gid );
      fprintf( stdout, "main: pw_age->%s<\n",\
	       p->pw_age ? p->pw_age: "NULL" );
      fprintf( stdout, "main: pw_comment->%s<\n",\
	       p->pw_comment ? p->pw_comment: "NULL" );
      fprintf( stdout, "main: pw_gecos->%s<\n",\
	       p->pw_gecos ? p->pw_gecos: "NULL" );
      fprintf( stdout, "main: pw_dir->%s<\n",\
	       p->pw_dir ? p->pw_dir: "NULL" );
      fprintf( stdout, "main: pw_shell->%s<\n",\
	       p->pw_shell ? p->pw_shell: "NULL" );
      fflush( stdout );
      
    }else{
      
      fprintf( stdout, "FAILED\n" );
      fflush( stdout );
      
    }

  }


}
