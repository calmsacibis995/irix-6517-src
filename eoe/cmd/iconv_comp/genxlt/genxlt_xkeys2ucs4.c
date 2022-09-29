/*
*       genxlt_xkeys2ucs4.c
*
*       genxlt for the X KeySyms to UCS-4 conversion tables.
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
#define   IDX_NUM_ELMNT     0x2ff
#define   HFUNC_VAL1        9
#define   HFUNC_VAL2        9
#define   HFUNC_VAL3        3
#define   HFUNC_VAL4        0
/*
#define   HASH_MASK         0x3ff
*/
#define   HASH_NUM_ELMNT    0x400    
#ifndef   HASH_NUM_FACTOR
#define   HASH_NUM_FACTOR   1.20
#endif
#define   HASH_KEYS_FUNC( h_key, h_val, h_num )                     \
       ( (                                                          \
            (((h_key) >> ((h_val)[0])) ^ ((h_key) << ((h_val)[1]))) \
           * ((h_val)[2]) + (h_num)                                 \
         ) % (h_num)                                                \
       )

#define   verboseprint      fprintf


typedef struct hash_element {
    unsigned int            key_val;
    int                     cnv_val;
} hash_element;

typedef struct hash_params {
    unsigned int            func_vals[4];
    unsigned int            num_elements;
} hash_params;

typedef struct table_head {
    int                     version;
    int                     offset_to_hash_table;
    hash_params             hash_params;
    unsigned int            maxtval;
} table_head;


table_head      table_hd;
uint_t          table_hd_size = 0;

uint_t         *idx_table     = NULL;
uint_t          idx_map_size  = 0;

hash_element   *hash_table    = NULL;
uint_t          hash_map_size = 0;

int	        verbose = 0;
FILE	       *vs_fd;


char Usage_msg[] = "Usage: genxlt_keys2ucs4 [options] \
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
  uint_t         hash_num_elmnt = 0, hash_num_count = 0;
  uint_t         hash_val, hash_func_vals[4];
  hash_element  *p_hash_entry;
  char          *check;

  extern int     optind;
  extern char   *optarg;
  int            flag;
  char	        *from_codeset = "XKEYS"; 
  char	        *to_codeset   = "UCS-4";
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

  hash_num_elmnt  =  HASH_NUM_ELMNT * HASH_NUM_FACTOR;
  hash_map_size   =  hash_num_elmnt * sizeof( hash_element );
  hash_table      = (hash_element   *)malloc( hash_map_size );
  bzero( hash_table, hash_map_size );
 
  hash_func_vals[0] = HFUNC_VAL1;
  hash_func_vals[1] = HFUNC_VAL2;
  hash_func_vals[2] = HFUNC_VAL3;
  hash_func_vals[3] = HFUNC_VAL4;

  if ( verbose ) {/* VERBOSE */ 
       verboseprint( vs_fd, "HASH_NUM_ELMNT : %d\n", HASH_NUM_ELMNT  );
       verboseprint( vs_fd, "HASH_NUM_FACTOR: %f\n", HASH_NUM_FACTOR );
       verboseprint( vs_fd, "hash_num_elmnt : %d\n", hash_num_elmnt  );
  }

  
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

      } else { /* Hash table */

           hash_val = HASH_KEYS_FUNC( tbl_key_b, hash_func_vals, hash_num_elmnt );              

#ifdef _DEBUG
           if ( verbose ) {/* VERBOSE */ 
                verboseprint( vs_fd, "HASH  value : hash_val = %x (%d)\n", hash_val, hash_val );
	   }
#endif


           p_hash_entry   = hash_table + hash_val;
           hash_num_count = 0;

           while ( p_hash_entry->key_val != 0 ) { /* while (#2) */

                  if ( hash_num_count > hash_num_elmnt ) {

                       fprintf( stderr, "Error: No free element in a hash table.\n" );
                       fprintf( stderr, "TERMINATED\n" );
                       exit(1);

                  } else {  /* Colission ! */ 

                       p_hash_entry ++;
                       hash_val ++;

                       if ( hash_val  >= hash_num_elmnt ) {
                            hash_val   = 0;
                          p_hash_entry = hash_table;
                       }

                       hash_num_count ++;
                  }
           } /* while (#2) */

#ifdef _DEBUG
           if ( verbose ) {/* VERBOSE */
                verboseprint( vs_fd, "Colission ? : hash_val = %x (%d)\n", hash_val, hash_val );
           }
           if ( hash_val >= hash_num_elmnt ) {
                fprintf( stderr, "Error: Exceed max hash_val: %x\n", hash_val );
                exit(1);
           }
#endif
           hash_table[ hash_val ].key_val = tbl_key_b;
           hash_table[ hash_val ].cnv_val = tbl_cnv_b;

      } /* Index or Hash table. */

  } /* while (#1) */


  table_hd_size    = (short) sizeof( table_head );
  table_hd.version = 1;
  table_hd.offset_to_hash_table 
                   = table_hd_size  + idx_map_size;
  table_hd.hash_params.func_vals[0] = hash_func_vals[0];
  table_hd.hash_params.func_vals[1] = hash_func_vals[1];
  table_hd.hash_params.func_vals[2] = hash_func_vals[2];
  table_hd.hash_params.func_vals[3] = hash_func_vals[3];
  table_hd.hash_params.num_elements = hash_num_elmnt;
  table_hd.maxtval = idx_maxval;

  if ( verbose ) {/* VERBOSE */ 
       verboseprint( vs_fd, "table_hd.version ....................... %d\n", table_hd.version ); 
       verboseprint( vs_fd, "table_hd.offset_to_hash_table .......... %d\n", table_hd.offset_to_hash_table ); 
       verboseprint( vs_fd, "table_hd.hash_params.func_vals[0] ...... %d\n", table_hd.hash_params.func_vals[0] ); 
       verboseprint( vs_fd, "table_hd.hash_params.func_vals[1] ...... %d\n", table_hd.hash_params.func_vals[1] ); 
       verboseprint( vs_fd, "table_hd.hash_params.func_vals[2] ...... %d\n", table_hd.hash_params.func_vals[2] ); 
       verboseprint( vs_fd, "table_hd.hash_params.func_vals[3] ...... %d\n", table_hd.hash_params.func_vals[3] ); 
       verboseprint( vs_fd, "table_hd.hash_params.num_elements ...... %d\n", table_hd.hash_params.num_elements ); 
       verboseprint( vs_fd, "table_hd.maxtval ....................... %d\n", table_hd.maxtval ); 
  }

  fwrite( &table_hd,  table_hd_size, 1, ot_fd ); 
  fwrite( idx_table,   idx_map_size, 1, ot_fd );
  fwrite( hash_table, hash_map_size, 1, ot_fd );

  printf( "\"%s\" \"%s\" \"libc.so.1\" "
          "\"__iconv_flw_hash_flw\" \"libc.so.1\" " 
          "\"__iconv_control\" FILE \"%s_%s.map\";\n", 
          from_codeset, to_codeset, from_codeset, to_codeset );

  fclose( ot_fd );
  fclose( in_fd );
}


