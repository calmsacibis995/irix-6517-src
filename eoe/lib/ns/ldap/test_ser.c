#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include "lber.h"
#include "ldap.h"

/* #define NS */

void main( int argc, char **argv ){

  double t;
  LDAP * l;
  int i, v;
  struct timeval ti, tf;
  LDAPMessage	*lr = NULL;

  gettimeofday( &ti, NULL );

  for( i = 0; i < 100; i++ ){

#ifdef NS
    if((l = ldap_open( "nunkun.engr.sgi.com", 390 ))==NULL){

      printf( "ERROR ladp_open\n" );
      exit( 1 );

    }

    v =\
      ldap_simple_bind_s( l,\
      "cn=Directory Manager, o=Silicon Graphics Inc. , c=US",\
			"atardece7");
#else
    if((l = ldap_open( "nunkun.engr.sgi.com", 389 ))==NULL){

      printf( "ERROR ladp_open\n" );
      exit( 1 );

    }

    v =\
      ldap_simple_bind_s( l,\
      "cn=root, o=Silicon Graphics Inc. , c=US",\
			"secret");


#endif

    if( v < 0 ){

      printf( "ERROR ldap_simple_bind %d\n",v  );
      exit( 1 );

    }
    /*  v = ldap_result( l, LDAP_RES_ANY, 1, NULL, &lr ); */

    ldap_unbind( l );
    
    printf( "\\\b \b" );
    fflush( stdout );

  }

  gettimeofday( &tf, NULL );

  if( tf.tv_usec < ti.tv_usec ){
    tf.tv_usec += 1000000;
    tf.tv_sec--;
  }

  tf.tv_sec -= ti.tv_sec;
  tf.tv_usec -= ti.tv_usec;

  t = tf.tv_usec + tf.tv_sec * 1000000;
  
  printf("at=%f\n", t/100 );

}




