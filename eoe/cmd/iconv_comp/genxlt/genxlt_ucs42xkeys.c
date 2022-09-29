/*
*       genxlt_ucs42xkeys.c
*
*       genxlt for the UCS-4 to X KeySyms conversion tables.
*
*       This genxlt should be used only for the conversiont table
*       generator.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>

#define   BUF_SIZE          255
#define   BASE_HEX          16
#define   IDX_NUM_ELMNT     0x10000

#define   verboseprint      fprintf


typedef struct table_head {
    int                     version;
    unsigned int            maxtval;
} table_head;


table_head      table_hd;
uint_t          table_hd_size = 0;
uint_t         *idx_table     = NULL;
uint_t          idx_map_size  = 0;
int	        verbose       = 0;
FILE	       *vs_fd;


char Usage_msg[] = "Usage: genxlt_ucs42xkeys [options] \
-s <type> \
-f <from_codeset> -t <to_codeset> -i <mapping table> \
-p <Converter C file path> -o\n\
-v verbose\n\
-o stdout output\n\
-h usage message\n\
When the -i is omitted, the stdin is used.\n\
When the -p is omiited, the current directory is used.\n";


/***
 ***  MAIN FUNCTION
 ***/

main(int argc, char *argv[])
{
  char          *in_file = NULL,  *ot_file = NULL;
  FILE          *in_fd   = stdin, *ot_fd   = NULL;
  uint_t         line_no = 0;
  char           line[BUF_SIZE];

  char           tbl_key[BUF_SIZE], tbl_cnv[BUF_SIZE];
  uint_t         tbl_key_b, tbl_cnv_b;
  int            idx_maxval = 0;
  char          *check;

  extern int     optind;
  extern char   *optarg;
  int            flag;
  char	        *from_codeset = "UCS-4"; 
  char	        *to_codeset   = "XKEYS";
  char	        *path         = ".\/";
  int	         force_stdout =  0;


  while ( (flag = getopt (argc, argv, "s:p:f:i:ot:vk?h")) != EOF ) {

          switch (flag) {
          case 'i': in_file      = strdup(optarg); break;
          case 'o': ot_fd        = stdout;         break;
          case 'p': path         = strdup(optarg); break;
          case 'f': from_codeset = strdup(optarg); break;
          case 't': to_codeset   = strdup(optarg); break;
          case 'v': verbose      = 1;              break;
          case 'k': force_stdout = 1;              break;
          case '?':
          default : 
                    fprintf(stderr, "%s\n", Usage_msg);
		    exit(1);
          }
  }


  if ( in_file ) {
       if ( (in_fd = fopen( in_file, "r" )) == NULL ) {
            fprintf( stderr, "Error: opening the input file.\n" );
	    exit(1);
       }
  }

  ot_file = (char *)malloc( strlen(from_codeset) +
   		            strlen(to_codeset)   + strlen(path) + 2 );
  ot_file[0] = '\0';

  if ( ot_file && !ot_fd ) {
       sprintf(ot_file, "%s/%s_%s\.map", path, from_codeset, to_codeset);

       if ( (ot_fd = fopen( ot_file, "w" )) == NULL ) {
             fprintf( stderr, "Error opening the output file: %s.\n", ot_file );
	     exit(1);
       }
  }


  if ( verbose ) {/* VERBOSE */ 
       char     fname[64];
       sprintf( fname, "%s_%s.log", from_codeset, to_codeset );
       if ( force_stdout ) vs_fd = stdout;
       else 
       if ( (vs_fd = fopen( fname, "w" )) == NULL ) {
             verboseprint( stderr, "Error open a verbose file: %s.\n", fname );
             exit(1);
       }
       fprintf( vs_fd, "Verbose mode on.\n" );
       if (in_file)     verboseprint(vs_fd, "Input file: %s\n", in_file);
       else             verboseprint(vs_fd, "Using stdin\n");
       if (ot_file[0])  verboseprint(vs_fd, "Outfile: %s\n", ot_file);
       else             verboseprint(vs_fd, "Using stdout\n");
       verboseprint( vs_fd, "generating %s_%s mapping iconv converter\n",
                     from_codeset, to_codeset );
  }


  idx_map_size    =  IDX_NUM_ELMNT * sizeof( uint_t );
  idx_table       = (uint_t *) malloc( idx_map_size );
  bzero( idx_table,  idx_map_size );

  
  while ( fgets( line, BUF_SIZE, in_fd ) ) { /* while( #1 ) */

      line_no++;
      if ( sscanf( line, "%s%s", tbl_key, tbl_cnv ) != 2 )
  	   continue;

      tbl_key_b = strtoul( tbl_key, &check, BASE_HEX );
      if ( *check != (char)NULL ) {
           fprintf( stderr, "Error: bad key value at line %d.\n", line_no );
           fprintf( stderr, " - Check a string \"%s\".\n", check );
           exit(1);
      }
 
      tbl_cnv_b = strtoul( tbl_cnv, &check, BASE_HEX );
      if ( *check != (char)NULL ) {
           fprintf( stderr, "Error: bad cnv value at line %d.\n", line_no );
           fprintf( stderr, " - Check a string \"%s\".\n", check );
           exit(1);
      }

#ifdef _DEBUG
      if ( verbose ) {/* VERBOSE */ 
           verboseprint( vs_fd, "--------- line_no: %d ---------\n", line_no );
           verboseprint( vs_fd, "tbl_key   = %s\n",  tbl_key          );
           verboseprint( vs_fd, "tbl_cnv   = %s\n",  tbl_cnv          );
           verboseprint( vs_fd, "tbl_key_b = %lx\n", tbl_key_b        );
           verboseprint( vs_fd, "tbl_cnv_b = %lx\n", tbl_cnv_b        );
      }
#endif

      if ( tbl_key_b < IDX_NUM_ELMNT ) { /* Index table */

           idx_table[ tbl_key_b ] = tbl_cnv_b;
           idx_maxval = MAX( idx_maxval, tbl_key_b );  /* Value of index. */

#ifdef _DEBUG
           if ( verbose ) {/* VERBOSE */ 
                verboseprint( vs_fd, "INDEX: tbl_key_b = %x (%d)\n", tbl_key_b,  tbl_key_b  );
                verboseprint( vs_fd, "       max index = %x (%d)\n", idx_maxval, idx_maxval );
	   }
#endif

      } else { 
                fprintf( stderr, "Error: over index value at line %d.\n", line_no );
                exit(1);

      } /* Index table. */

  } /* while (#1) */


  table_hd_size    = (short) sizeof( table_head );
  table_hd.version = 1;
  table_hd.maxtval = idx_maxval;

  if ( verbose ) {/* VERBOSE */ 
       verboseprint( vs_fd, "table_hd.version ....................... %d\n", table_hd.version ); 
       verboseprint( vs_fd, "table_hd.maxtval ....................... %d\n", table_hd.maxtval ); 
  }

  fwrite( &table_hd,  table_hd_size, 1, ot_fd ); 
  fwrite( idx_table,   idx_map_size, 1, ot_fd );

  printf( "\"%s\" \"%s\" \"libc.so.1\" "
          "\"__iconv_flw_bom_idxw_flw\" \"libc.so.1\" " 
          "\"__iconv_control_bom\" \"libc.so.1\" " 
          "\"__iconv_unicode_signature\" FILE \"%s_%s.map\";\n", 
          from_codeset, to_codeset, from_codeset, to_codeset );

  fclose( ot_fd );
  fclose( in_fd );
}


