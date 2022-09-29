#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>


#include "gparser.h"


#ifdef TESTING
#define MAX_OPTIONS        32
OPTIONS_T options[MAX_OPTIONS];
GPARSER_T parse[] = {
  { "a",         0, &options[0].found, &options[0].buf[0], OPTION_BUF_SIZE }, 
  { "b", GET_TOKEN, &options[1].found, &options[1].buf[0], OPTION_BUF_SIZE }, 
  { "d",         0, &options[2].found, &options[2].buf[0], OPTION_BUF_SIZE }, 
  { "F",         0, &options[3].found, &options[3].buf[0], OPTION_BUF_SIZE }, 
  { NULL, NULL, NULL, NULL }, 
};
#endif

int
gParser( int argc, char *argv[], int *Nargc_p, GPARSER_T *gParser_p, char *ArgBufp, long ArgBufLen )
{
  int i,j;
  int gindex=0;
  int  AC = argc;
  int len;
  char **AVp = argv;
  int Noptions=0;
  long CurLen=0;

  ArgBufLen = ArgBufLen ? ArgBufLen-1 : 0;

  if( !ArgBufp )   return( 0 );
  if( !ArgBufLen ) return( 0 );
  if( Nargc_p ) *Nargc_p = 0;

  ArgBufp[0] = '\0';

  if( (argc < 2) || !gParser_p ) {
    return( 0 );
  }

  for( i=1; i<argc; ++i ) {

    if( (AVp[i][0] == '-') && (AVp[i][1] != 0) ) {

      for( gindex=0; (gParser_p+gindex)->OptionStr; ++gindex ) {

        len = strlen( (gParser_p+gindex)->OptionStr );
        len = strlen( &AVp[i][1] );
        if( strcmp( (gParser_p+gindex)->OptionStr, &AVp[i][1] ) == 0 ) {

          ++Noptions;

          if( (gParser_p+gindex)->OptionBuf ) 
            memset( (gParser_p+gindex)->OptionBuf, 0, 
                (gParser_p+gindex)->OptionBufSize );
          if( (gParser_p+gindex)->Found ) 
            *(gParser_p+gindex)->Found = TRUE;

          if( AVp[i][1+len] == 0 ) {
            if( i >= argc-1 ) goto EndParser;

            /* Grab next argument */
            if( (gParser_p+gindex)->OptionBuf ) {

              strncpy( &(gParser_p+gindex)->OptionBuf[0], &AVp[i+1][0],
                (gParser_p+gindex)->OptionBufSize );
            }

            if( (gParser_p+gindex)->Flags == GET_TOKEN ) ++i;

          } else {

            if( (gParser_p+gindex)->OptionBuf ) {

              strncpy( &(gParser_p+gindex)->OptionBuf[0], &AVp[i][1+len],
                (gParser_p+gindex)->OptionBufSize );
            }
          }

        }
      }
    } else {

      /* Copy into ArgBufp */
      if( CurLen < ArgBufLen ) {
        strncat( ArgBufp, AVp[i], ArgBufLen-CurLen );
        CurLen += strlen( AVp[i] );

        if( CurLen < ArgBufLen ) {
          strncat( ArgBufp, " ", ArgBufLen-CurLen );
          CurLen += strlen( " " );
        }
      }
    }
  }

EndParser:
  ArgBufp[(CurLen ? CurLen-1 : 0)] = '\0';

  if( Nargc_p ) *Nargc_p = Noptions;
  return( 0 );
}

#ifdef TESTING
int
main( int argc, char *argv[] )
{
  int Nargs=0;
  int i;
  char argbuf[256];

  for( i=0; i<MAX_OPTIONS; ++i ) {
    options[i].found = FALSE;
  }

  printf( "Starting\n" );
  printf( "  Valid options are:\n" );

  for( i=0; parse[i].OptionStr ; ++i ) {
    printf( "  -%s\n", parse[i].OptionStr );
  }


  gParser( argc, argv, &Nargs, parse, argbuf, sizeof(argbuf) );

  printf( "Number of options found = %d\n", Nargs );
  
  for( i=0; i<MAX_OPTIONS; ++i ) {
    if( options[i].found ) {
      printf( "Option[%d] = '%s'\n", i, parse[i].OptionStr );
      if( parse[i].Flags ) printf( "  Value = '%s'\n", options[i].buf );
    }
  }

  printf( "Arguments: '%s'\n", argbuf );

  return( 0 );
}
#endif
