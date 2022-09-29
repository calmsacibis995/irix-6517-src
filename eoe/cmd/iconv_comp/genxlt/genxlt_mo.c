/*
*	genxlt_mc.c
*
*	genxlt for the multi-octet conversion tables.
*
*	This genxlt should be used only for the conversiont table
*	generator.
*
*	It processes the conversion table based on the following
*	characteristics:
*
*	DBCS codeset:
*		First byte:	0x81-0x9F, 0xE0-0xEF (SJIS)
*				0xA1-0xFE (BIG5)
*				0x81-0xFE (IBM DBCS PC)
*				always in the jump table (which
*				provides the entrance to the
*				result code point.)
*		Second byte:	range between 0x40 - 0xFE. (leaf table size)
*
*	DBCS character set:
*		First byte:	0x21 - 0x7E
*				always in the jump table
*		Second byte:	0x21 - 0x7E
*
*	EUC codeset:
*		First byte:	Either above 0x80 or an ASCII
*		Following:	above 0x80.
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <locale.h>
#include <nl_types.h>
#include "make_cvt.h"

#define TABLE_ENTRY	0x100000
#define TABLE_MASK	0x0fffff
#define MAX_BYTES 255
#define MAX_CS_BYTES	6
#define MAX_LINE 256
#define UNK_CHR 0xff
#define max(a)	if (max_len < a) max_len = a


typedef struct table_head {
	int	version;
	int	euc_tbl_mask;
	int	euc_tbl_offs;
} table_header;

table_header table_hd;

uchar_t	*map_table; /* the data map */
	
uint_t  atom;

int	num_leaf = 0;

int	leaf_begin_ptr =0;

int	leaf_table_cnt=0;
int	jump_table_cnt=1;

/* Conversion map file table */
struct ByteRec {
        uchar_t len;
	ushort_t table_num;
        union {
            uchar_t bytes[MAX_CS_BYTES];
            struct TableRec *tblrec;
        } u;
};

struct TableRec { /* Table has 256 entries */
    int transition_mark; /* 0 for transition (jump) table 1 for leaf table */
    struct ByteRec tbl[0x100];
};

int	verbose=0;

int	table_cnt = 1; 	/* total table count */
int	max_len =0;	/* MB_CUR_MAX in the "to" codeset */

ushort_t *jump_table_offset;
ushort_t *leaf_table_offset;

int	num_freed = 0;

struct ByteRec DefRec = {UNK_CHR, {0}};
struct ByteRec SubstRec = {1, {0x1a}};

struct TableRec *Table;

char	Usage_msg[]=
"Usage: gen_iconv [options] -s <type> -f <from_codeset> -t <to_codeset> -i \
<mapping table> -p <Converter C file path> -o\n\
-s 1 for DBCS codeset\n\
   2 for DBCS character set\n\
   3 for EUC codeset\n\
-v verbose\n\
-o stdout output\n\
-h usage message\n\
-? usage message\n\
\n\
When the -f or -t is omitted, the default codeset is set to UCS-2\n\
When -i is omitted, the stdin is used\n\
When -p is omiited, the current directory is used\n";

char	err_str[255];
FILE	*out_fd = NULL; /* default */
FILE	*verbose_fd;
int	type_flag = 0;

main(int argc, char *argv[])
{
    uint_t 	    i, line_no=0;
    uint_t 	    inlen, outlen;
    uchar_t 	    inbytes[MAX_BYTES], outbytes[MAX_BYTES];
    char 	    instr[MAX_BYTES], outstr[MAX_BYTES];
    char 	    bytestr[3];
    char 	    line[MAX_LINE];
    char 	    *in_ptr, *out_ptr;
    extern int      optind;
    extern char     *optarg;
    int             flag;
    FILE	    *in_fd=stdin; /* default*/
    char	    *in_file=NULL, *out_file=NULL;
    char	    *from_codeset = "UCS-2";  /*default */
    char	    *to_codeset = "UCS-2";
    char	    *path = ".\/";
    nl_catd	    catd; /* Message catalog */
    int		    force_stdout=0;

    while ((flag = getopt (argc, argv, "s:p:f:i:ot:vk?h")) != EOF) {
            switch (flag) {
            case 'i': in_file = strdup (optarg); break;
	    case 'o': out_fd = stdout; break;
	    case 'p': path = strdup(optarg); break;
	    case 'f': from_codeset = strdup(optarg); break;
	    case 's': type_flag = atoi(optarg); break;
	    case 't': to_codeset = strdup(optarg); break;
            case 'v': verbose = 1; break;
	    case 'k': force_stdout = 1; break;
            case '?':
            default : 
		fprintf(stderr, "%s\n", catgets(catd, 1,1,
				Usage_msg));
		exit(1);
            }
    }

   if (! type_flag) {
	fprintf(stderr, "%s\n", Usage_msg);
	exit(1);
   }

   if (in_file) {
	in_fd = fopen(in_file, "r");
	if (! in_fd) {
		fprintf(stderr, "Error opening the input file\n");
		exit(1);
	}
   }

   out_file = (char *) malloc (strlen(from_codeset) +
   		strlen(to_codeset) + strlen(path) + 2);
   out_file[0] = '\0';
   if (out_file && !out_fd) {
        sprintf(out_file, "%s/%s_%s\.map", path, from_codeset, to_codeset);
	out_fd = fopen(out_file, "w");
	if (! out_fd) {
		sprintf(err_str, "Error openning the output file %s",
			out_file);

		fprintf(stderr, "%s\n", err_str);
		exit(1);
	}
   }

  if(verbose) { /* no translation English only :-) */
        char    fname[50];

        sprintf(fname, "%s_%s.log", from_codeset, to_codeset);
        if (force_stdout) verbose_fd = stdout;
        else verbose_fd = fopen(fname, "w");
	fprintf(verbose_fd, "Verbose mode on\n");
	if (in_file) fprintf(verbose_fd, "Input file: %s\n", in_file);
		else fprintf(verbose_fd, "Using stdin\n");
	if (out_file[0]) fprintf(verbose_fd, "Outfile: %s\n", out_file);
	 else fprintf(verbose_fd, "Using stdout\n");
	fprintf(verbose_fd, "generating %s_%s mapping iconv converter\n",
		from_codeset, to_codeset);
   }


   /* Initialize first byte table. */
   Table = (struct TableRec *)malloc(sizeof(struct TableRec));
   Table->transition_mark = 0;
   for (i = 0; i < 0x100; i++) {
	Table->tbl[i] = DefRec;
   }


   while (fgets(line, 255, in_fd)) {

	line_no++;
	if (sscanf(line, "%s%s", instr, outstr) != 2) {
	    /* Skip incomplete or blank lines. */
	    continue;
	}

	if(!(inlen = parse(instr, inbytes, 0))) {
		sprintf(err_str, "Line %d: Invalid from_string: <%s>, Expecting \\xXX\\xXX, 0xXXXX, or <UXXXX> in hex\n",
			line_no, instr);
                fprintf(verbose_fd, "%s\n", catgets(catd, 1, 3,
                                err_str));
		continue;
	}
	if(!(outlen = parse(outstr, outbytes, 1))) {
		sprintf(err_str, "Line %d: Invalid to_string: <%s>, Expecting \\xXX\\xXX, 0xXXXX, or <UXXXX> in hex\n",
			line_no, instr);
                fprintf(verbose_fd, "%s\n", catgets(catd, 1, 4, err_str));
		continue;
	}
		
	make_entry(Table, inbytes, inlen, outbytes, outlen);

   }
   cal_table (Table, 0, "Table");

   if (Table->transition_mark) num_leaf -= 1; /* except the Root table */

   if(verbose) fprintf(verbose_fd, "%d leaf table\n", num_leaf);
   table_head_size = (short) sizeof(struct table_head);

   switch (type_flag) {
     case 1: /* DBCS code set */
   	data_map_size = (((table_cnt - num_leaf+1)*256) * sizeof(int)) +
                num_leaf * (0x100 - 0x40) * sizeof(short);
	break;
     case 2: /* DBCS character set */
   	data_map_size = (((table_cnt - num_leaf+1)*256) * sizeof(int)) +
                num_leaf * 128 * sizeof(short);
	break;
     case 3: /* EUC codeset */
   	/* The jump tables have 256 entries.  And the leaf tables have 128 entry */
   	/* The high-bit is ignore since this is a specialized euc to UCS table */
   	data_map_size = (((table_cnt - num_leaf+1)*256) * sizeof(int)) +
		num_leaf * 128 * sizeof(short);
	break;
     default:
	fprintf(stderr, "%s\n", Usage_msg);
	exit(1);
   }

   leaf_begin_ptr = ((table_cnt - num_leaf+1)*256) * sizeof(int);

   jump_table_offset = (ushort_t *) malloc((table_cnt - num_leaf + 1) * sizeof(ushort_t));
   leaf_table_offset = (ushort_t *) malloc(num_leaf * sizeof(ushort_t));

   if(verbose) 
     fprintf(verbose_fd, "# of table = %d, size of table = %d, leaf begin %d\n", table_cnt,
	data_map_size, leaf_begin_ptr);

   map_table = (uchar_t *) malloc (data_map_size);

   table_hd.version = 1;
   table_hd.euc_tbl_mask = TABLE_MASK;
   switch (type_flag) {
  	case 1: /* DBCS code set */
   		table_hd.euc_tbl_offs = leaf_begin_ptr - (0x40*sizeof(ushort_t)) -1;
       	break;
     	case 2: /* DBCS character set */
   		table_hd.euc_tbl_offs = leaf_begin_ptr - 1;
       	break;
     	case 3: /* EUC codeset */
   		table_hd.euc_tbl_offs = leaf_begin_ptr - (128*sizeof(ushort_t)) - 1;
        break;
    }
   numbering_table (Table);

   if (verbose) {
   for (i = 0; i< jump_table_cnt; i++)
	fprintf(verbose_fd,"jump_table_offset[%d] %d\n", i, jump_table_offset[i]);
   for (i = 0; i< leaf_table_cnt; i++)
	fprintf(verbose_fd,"leaf_table_offset[%d] %d\n", i, leaf_table_offset[i]);
   }

   gen_table(Table, 0, "Table", 0);

   free_tables (Table);

#if 0 /* debug */
   dump_map();
#endif

   fwrite(&table_hd, table_head_size, 1, out_fd); 
   fwrite(map_table, data_map_size, 1, out_fd);

   fclose(out_fd);
   switch (type_flag) {
     case 1: /* DBCS code set */
	printf("\"%s\" \"%s\" \"libc.so.1\" \"__iconv_flb_dbcs_flh\" \"libc.so.1\"\
 \"__iconv_control\" FILE \"%s_%s.map\";\n", from_codeset, to_codeset, 
		from_codeset, to_codeset);
	break;
     case 2: /* DBCS character set */
	printf("\"%s\" \"%s\" \"libc.so.1\" \"__iconv_flb_7dbcs_flh\" \"libc.so.1\"\
 \"__iconv_control\" FILE \"%s_%s.map\";\n", from_codeset, to_codeset,
		 from_codeset, to_codeset);
	break;
     case 3: /* EUC codeset */
	printf("\"%s\" \"%s\" \"libc.so.1\" \"__iconv_flb_euc_flh\" \"libc.so.1\"\
 \"__iconv_control\" FILE \"%s_%s.map\";\n", from_codeset, to_codeset,
		 from_codeset, to_codeset);
	break;
     default:
	fprintf(stderr, "%s\n", Usage_msg);
	exit(1);
   }

   exit(0);
}

int make_entry(struct TableRec *tblrec, uchar_t *inbytes, uint_t inlen,
   uchar_t *outbytes, uint_t outlen)
{
   struct ByteRec *rec;
   uint_t i;

   rec = &(tblrec->tbl[*inbytes]);

   /* If there are more than one bytes left. */
   if (inlen > 1) {

	/* If table entry is still subst char ... */
	if (rec->len) {
	    rec->len = 0;
	    table_cnt ++;
	    rec->u.tblrec = (struct TableRec *)malloc(sizeof(struct TableRec));
	    for (i = 0; i < 0x100; i++) {
		rec->u.tblrec->tbl[i] = DefRec;
	    }
	    rec->u.tblrec->transition_mark = 0;
	}

	/* Recurse. */
	make_entry(rec->u.tblrec, inbytes + 1, inlen - 1, outbytes, outlen);
   }

   else { /* Last input byte. */
	tblrec->transition_mark = 1;
	rec->len = outlen;
	max(outlen);
	memcpy(rec->u.bytes, outbytes, outlen);
#if 0
if (verbose) {
fprintf(verbose_fd, "make_entry: %d bytes: ", rec->len);
for (i=0;i<outlen;i++)
  fprintf(verbose_fd, "\\x%02x", outbytes[i]);
fprintf(verbose_fd, "\n");
}
#endif 
   }
}

int	table_index = 0;

int free_tables(struct TableRec *tblrec)
{
   uint_t i, j;
   struct ByteRec *rec;

   for (i = 0; i < 0x100; i++) {
	rec = &(tblrec->tbl[i]);
	if (!(rec->len)) {
	    free_tables(rec->u.tblrec);
	}
   }

   num_freed ++;
   free(tblrec);
}

dump_map()
{
	int	i;
	if (verbose) {
	  for (i=0;i<data_map_size; i++)
		fprintf(verbose_fd, "map_table[%d] %08x\n", i, map_table[i]);
      }
}

int numbering_table(struct TableRec *tblrec)
{
   uint_t i, j;
   struct ByteRec *rec;

   for (i = 0; i < 0x100; i++) {
	rec = &(tblrec->tbl[i]);
	if (!(rec->len)) {
	    if (rec->u.tblrec->transition_mark == 1) {
		if (type_flag == 1) /* DBCS codeset 0x40 - 0x100 */
		{
			leaf_table_offset[leaf_table_cnt] = leaf_begin_ptr + (leaf_table_cnt * (0x100-0x40) *sizeof(ushort_t));
		}else {
			leaf_table_offset[leaf_table_cnt] = leaf_begin_ptr + (leaf_table_cnt * 128 *sizeof(ushort_t));
		}
	    	rec->table_num = leaf_table_cnt++;
	    }else { 
		jump_table_offset[jump_table_cnt] = 256* jump_table_cnt * sizeof(uint_t) ;
		rec->table_num = jump_table_cnt++;
	    }
            numbering_table(rec->u.tblrec);
	}
   }
}

int cal_table(struct TableRec *tblrec, int index, char *name)
{
   uint_t i, j;
   struct ByteRec *rec;
   char next_name[(MAX_BYTES * 2) + 1];
   int  leaf_cnt = 0;

   for (i = 0; i < 0x100; i++) {
        rec = &(tblrec->tbl[i]);
        if (!(rec->len)) {
            sprintf(next_name, "%s%02x", name, i);
            cal_table(rec->u.tblrec,table_index, next_name);
        }
   }

   if (tblrec->transition_mark == 1)
	num_leaf +=1;
}

	
int gen_table(struct TableRec *tblrec, int table_num, char *name, int root_flag)
{
   uint_t i, j;
   struct ByteRec *rec;
   char next_name[(MAX_BYTES * 2) + 1];

   uint_t	*int_ptr;
   ushort_t	*sht_ptr;

   if (verbose) {
     if (tblrec->transition_mark == 1)
	 fprintf(verbose_fd, "In leaf table [%s] of %d offset:%u\n", name, table_num,
		leaf_table_offset[table_num]);
     else
	 fprintf(verbose_fd, "In jump table [%s] of %d offset:%u\n", name, table_num,
		jump_table_offset[table_num]);
   }


   /* Generate subtables. */
   for (i = 0; i < 0x100; i++) {
	rec = &(tblrec->tbl[i]);
	if (!(rec->len)) {
	    sprintf(next_name, "%s%02x", name, i);
#if 0
if (verbose) fprintf(verbose_fd, "Going to subtable <%s>\n", next_name);
#endif
            if (verbose) {
	      if (rec->u.tblrec->transition_mark == 1) /* entering leaf table */
     	         fprintf(verbose_fd, "Enter leaf [%s] table %d begin at %u\n",
			next_name, rec->table_num, leaf_table_offset[rec->table_num]);
	      else  /* entering jump table */
     	         fprintf(verbose_fd, "Enter jump [%s] table %d begin at %u\n",
			next_name, rec->table_num, jump_table_offset[rec->table_num]);
	    }
	    gen_table(rec->u.tblrec,rec->table_num, next_name, 1);
	}
   }

/* ROOT */
   /* map to the memory */
   if (root_flag == 0) { /* Root table */

   if (verbose)
	 fprintf(verbose_fd, "Writing Root table [%s] of %d\n", name, table_num);

   int_ptr = (uint_t *) map_table; /* jump table is in uint_t */

   for (i = 0; i < 0x100; i++) {  /* 256 entries */
	atom = 0;
	rec = &(tblrec->tbl[i]);
	if (rec->len) {
		if (rec->len == UNK_CHR)  {
			continue;
			atom = 0;
		} else {
			for (j = 0; j< rec->len; j++)
				atom |= (uint_t) (rec->u.bytes[j] << ((rec->len -j-1) *8));	
		}
	}
	else { 
  	switch (type_flag) {
     		case 1: /* DBCS code set */
                if (rec->u.tblrec->transition_mark == 1) { /* a leaf table pointer */
	  	    atom = (uint_t) ((leaf_table_offset[rec->table_num] - (0x40*sizeof(ushort_t)))  | TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found leaf table %d offset at: %u\n",
                    name,  i, rec->table_num, leaf_table_offset[rec->table_num]);
                }else {
		    atom = (uint_t) (jump_table_offset[rec->table_num]| TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found jump table %d offset at: %u\n",
                    name,  i, rec->table_num, jump_table_offset[rec->table_num]);
                }
        	break;
     		case 2: /* DBCS character set */
                if (rec->u.tblrec->transition_mark == 1) { /* a leaf table pointer */
	  	    atom = (uint_t) (leaf_table_offset[rec->table_num]  | TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found leaf table %d offset at: %u\n",
                    name,  i, rec->table_num, leaf_table_offset[rec->table_num]);
                }else {
		    atom = (uint_t) (jump_table_offset[rec->table_num]| TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found jump table %d offset at: %u\n",
                    name,  i, rec->table_num, jump_table_offset[rec->table_num]);
                }
        	break;
     		case 3: /* EUC codeset */
                if (rec->u.tblrec->transition_mark == 1) { /* a leaf table pointer */
	  	    atom = (uint_t) ((leaf_table_offset[rec->table_num] - (128*sizeof(ushort_t)))  | TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found leaf table %d offset at: %u\n",
                    name,  i, rec->table_num, leaf_table_offset[rec->table_num]);
                }else {
		    atom = (uint_t) (jump_table_offset[rec->table_num]| TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found jump table %d offset at: %u\n",
                    name,  i, rec->table_num, jump_table_offset[rec->table_num]);
                }
        	break;
     		default:
        	fprintf(stderr, "%s\n", Usage_msg);
        	exit(1);
   	    }
	}
	*(int_ptr+i) = (uint_t) atom;
	if(verbose) {
	  fprintf(verbose_fd, "offset at:%u with value ", i * sizeof(uint_t));
	  fprintf(verbose_fd, "%08x (atom %08x)\n", *(int_ptr + i), atom);
	}
   }
   } else if (tblrec->transition_mark == 1) {/* leaf */

        if (verbose)
	   fprintf(verbose_fd, "Writing leaf table [%s] of %d offset:%u\n", name, table_num,
		leaf_table_offset[table_num]);

	sht_ptr = (ushort_t  *)(map_table + leaf_table_offset[table_num]); 

   	switch (type_flag) {
     	case 1: /* DBCS code set */
        /* only look between 0x40 - 0xff for shift-range */
        for (i = 0x40; i < 0x100; i++)
        {
                atom = 0;
                rec = &(tblrec->tbl[i]);
                if (rec->len == UNK_CHR)  {
                        continue;
                   atom = 0;
                } else {
                   for (j = 0; j< rec->len; j++)
                      atom |= (uint_t) (rec->u.bytes[j] << ((rec->len -j-1) *8));
                }
                *(sht_ptr+i-0x40) = (ushort_t) atom;

                if(verbose) {
                    fprintf(verbose_fd, "offset at:%u with value ",
                        leaf_table_offset [table_num] + ((i-0x40)*sizeof(ushort_t)));
                    fprintf(verbose_fd, "%08x (atom %08x)\n",(uint_t) *(sht_ptr+ (i-0x40)), atom);
                }
        }
	break;
     	case 2: /* DBCS character set */
        /* only look at the lower 128 */ /* hardcoded DBCS charset pattern :-(*/
        for (i = 0; i < 128; i++)
        {
                atom = 0;
                rec = &(tblrec->tbl[i]);
                if (rec->len == UNK_CHR)  {
                        continue;
                   atom = 0;
                } else {
                   for (j = 0; j< rec->len; j++)
                      atom |= (uint_t) (rec->u.bytes[j] << ((rec->len -j-1) *8));
                }
                *(sht_ptr+i) = (ushort_t) atom;

                if(verbose) {
                    fprintf(verbose_fd, "offset at:%u with value ",
                        leaf_table_offset [table_num] + ((i)*sizeof(ushort_t)));
                    fprintf(verbose_fd, "%08x (atom %08x)\n",(uint_t) *(sht_ptr+ (i)), atom);
                }
        }
	break;
     	case 3: /* EUC codeset */
	/* only look at the upper 128 */ /* hardcoded euc pattern :-(*/
	for (i = 128; i < 0x100; i++)
	{
		atom = 0;
		rec = &(tblrec->tbl[i]);
		if (rec->len == UNK_CHR)  {
			continue;
		   atom = 0;
		} else {
		   for (j = 0; j< rec->len; j++)
		      atom |= (uint_t) (rec->u.bytes[j] << ((rec->len -j-1) *8));
		}
		*(sht_ptr+i-128) = (ushort_t) atom;

	        if(verbose) {
	  	    fprintf(verbose_fd, "offset at:%u with value ", 
			leaf_table_offset [table_num] + ((i-128)*sizeof(ushort_t)));
	  	    fprintf(verbose_fd, "%08x (atom %08x)\n",(uint_t) *(sht_ptr+ (i-128)), atom);
		}
	}
	break;
     	default:
	break;
   }
   } else { /* jump table */

	if (verbose)
	    fprintf(verbose_fd, "Writing jump table [%s] of %d offset %u\n", name, 
		table_num, jump_table_offset[table_num]);

   	int_ptr =  (uint_t *)( map_table + jump_table_offset[table_num]);

   	for (i = 0; i < 0x100; i++) {
	    atom = 0;
    	    rec = &(tblrec->tbl[i]);
    	    if (rec->len) {
    		if (rec->len == UNK_CHR)  {
    			continue;
    			atom = 0;
    		} else {
    			for (j = 0; j< rec->len; j++)
    				atom |= (uint_t) (rec->u.bytes[j] << ((rec->len -j-1) *8));	
    		}
    	    }else {
#if 0
                if (rec->u.tblrec->transition_mark == 1) { /* a leaf table pointer */
	  	    atom = (uint_t) (leaf_table_offset[rec->table_num]  | TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found leaf table %d offset at: %u\n",
                    name,  i, rec->table_num, leaf_table_offset[rec->table_num]);
                }else {
		    atom = (uint_t) (jump_table_offset[rec->table_num]| TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found jump table %d offset at: %u\n",
                    name,  i, rec->table_num, jump_table_offset[rec->table_num]);
                }
            }
#endif
  	    switch (type_flag) {
     		case 1: /* DBCS code set */
                if (rec->u.tblrec->transition_mark == 1) { /* a leaf table pointer */
	  	    atom = (uint_t) ((leaf_table_offset[rec->table_num] - (0x40*sizeof(ushort_t)))  | TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found leaf table %d offset at: %u\n",
                    name,  i, rec->table_num, leaf_table_offset[rec->table_num]);
                }else {
		    atom = (uint_t) (jump_table_offset[rec->table_num]| TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found jump table %d offset at: %u\n",
                    name,  i, rec->table_num, jump_table_offset[rec->table_num]);
                }
        	break;
     		case 2: /* DBCS character set */
                if (rec->u.tblrec->transition_mark == 1) { /* a leaf table pointer */
	  	    atom = (uint_t) (leaf_table_offset[rec->table_num]  | TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found leaf table %d offset at: %u\n",
                    name,  i, rec->table_num, leaf_table_offset[rec->table_num]);
                }else {
		    atom = (uint_t) (jump_table_offset[rec->table_num]| TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found jump table %d offset at: %u\n",
                    name,  i, rec->table_num, jump_table_offset[rec->table_num]);
                }
        	break;
     		case 3: /* EUC codeset */
                if (rec->u.tblrec->transition_mark == 1) { /* a leaf table pointer */
	  	    atom = (uint_t) ((leaf_table_offset[rec->table_num] - (128*sizeof(ushort_t)))  | TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found leaf table %d offset at: %u\n",
                    name,  i, rec->table_num, leaf_table_offset[rec->table_num]);
                }else {
		    atom = (uint_t) (jump_table_offset[rec->table_num]| TABLE_ENTRY);
                    if (verbose) fprintf(verbose_fd, "At [%s] of %02x, found jump table %d offset at: %u\n",
                    name,  i, rec->table_num, jump_table_offset[rec->table_num]);
                }
        	break;
     		default:
        	fprintf(stderr, "%s\n", Usage_msg);
        	exit(1);
   	    }
	    }
    	    *(int_ptr +i)= (uint_t) atom;
            if(verbose) {
                fprintf(verbose_fd, "offset at:%u with value ", 
    		   jump_table_offset[table_num] + (i * sizeof(uint_t)));
                fprintf(verbose_fd, "%08x (atom %08x)\n", *(int_ptr + i), atom);
            }
       }
   }
}

#define ERROR 0
int parse (uchar_t *in_string, uchar_t *outbuf, int lead_zero)
{

	uchar_t	*in_ptr, *p, *beg_ptr, *end_ptr, tempbuf[255] = "\0";
	uint_t	ucs_char=0;
	int	i, out_buf_ptr =0, out_len=0;
	char	h, l, in_strlen=0;

	in_ptr = in_string;
	in_strlen = strlen (in_string);
	end_ptr = in_string + in_strlen;

	while (isspace(*in_ptr)) in_ptr++;  /* skip the space */

	if (*in_ptr == '<') { /* Toekn <xxxx> */

	   if (*(in_ptr+1) == 'U') { /* UCS-2 <UXXXX> */
		in_ptr +=2; /* skip the < and U */
		ucs_char = strtoul((char *) in_ptr, (char **) &p,
				16);
		if ((*p != '>') && ((p - in_ptr) != 5) && 
			(ucs_char > 0xffffL)) 
			return(ERROR); /* Invalid UCS-2 value */
	       
		outbuf[0] = (uchar_t) ((ucs_char & 0xff00) >>8);
		outbuf[1] = (uchar_t) (ucs_char & 0x00ff);
		out_len  = 2;
		outbuf[2] = '\0';
		return(out_len);
	    }
	    for (i = 0; *(in_ptr + i) != '>'; i++) {
		if (isspace(*(in_ptr + i)) || (*(in_ptr + i) == '\0'))
			break;
	    }
	    if ( *(in_ptr + i) != '>') return (ERROR); /* no closing > mark */
	    memcpy(outbuf, in_ptr, i);
	    out_len = i/2;
	    outbuf[i] = '\0';
	    return (out_len);
	}

	if (*in_ptr == '0') { /* hex const 0xXXXX... */
		in_ptr += 1;
		if (*in_ptr != 'x' && *in_ptr != 'X') 
			return(ERROR);
		in_ptr += 1;
		beg_ptr = (uchar_t *) &tempbuf;
		while(in_ptr < end_ptr) {

		   	if ( !isxdigit(h = *(in_ptr++)))
				return (ERROR);
			if( !isxdigit(l = *(in_ptr++)))
				return (ERROR);
			out_len ++;
			*beg_ptr++ = h;
			*beg_ptr++ = l;
			*beg_ptr = '\0';
			beg_ptr =  (uchar_t *)&tempbuf;
			sscanf(beg_ptr, "%x", &ucs_char);
			outbuf[out_buf_ptr++] = (uchar_t) ucs_char;
		}
		outbuf[out_buf_ptr] = '\0';
		return(out_len);
	  }
	if (*in_ptr == '\\') { /* hex const */
		in_ptr += 1;
		beg_ptr =  (uchar_t *)&tempbuf;
		while(in_ptr < end_ptr) {
		   if (*in_ptr == 'x' || *in_ptr == 'X') {
			in_ptr += 1; /* to next char */
			/* more than one byte with
			multiple segment like \xYYY\xYYY */

		   	if ( !isxdigit(h = *(in_ptr++)))
				return (ERROR);
			if( !isxdigit(l = *(in_ptr++)))
				return (ERROR);
			in_ptr += 1; /* skip the \ */

			out_len ++;
			*beg_ptr++ = h;
			*beg_ptr++ = l;
			*beg_ptr = '\0';
			beg_ptr =  (uchar_t *)&tempbuf;
			sscanf(beg_ptr, "%x", &ucs_char);
			outbuf[out_buf_ptr++] = (uchar_t) ucs_char;
		   }
		}
		outbuf[out_buf_ptr] = '\0';
		return(out_len);
	  }
	  return(ERROR);
}
