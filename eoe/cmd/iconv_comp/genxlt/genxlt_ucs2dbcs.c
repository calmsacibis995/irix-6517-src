/*
*	genxlt_ucs2euc.c
*
*
*	It generates the UCS-2 to EUC conversion maps
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <locale.h>
#include <nl_types.h>
#include "make_cvt.h"

typedef struct table_head {
        int     version;
        uint_t  maxtval;
} table_header;

table_header table_hd;

ushort_t	*map_table; /* the data map */

#define MAX_BYTES 255
#define MAX_LINE 256

int	verbose=0;

char	Usage_msg[]=
"Usage: gen_iconv [options] -f <from_codeset> -t <to_codeset> -i \
<mapping table> -p <Converter C file path> -o\n\
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

uint_t  maxtval;

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

    while ((flag = getopt (argc, argv, "p:f:i:ot:vk?h")) != EOF) {
            switch (flag) {
            case 'i': in_file = strdup (optarg); break;
	    case 'o': out_fd = stdout; break;
	    case 'p': path = strdup(optarg); break;
	    case 'f': from_codeset = strdup(optarg); break;
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

   data_map_size = 0x10000 * sizeof(ushort_t);
   map_table = (ushort_t *) malloc (data_map_size); /* Map the whole UCS table */

   bzero(map_table, data_map_size);

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

	map_to_table(inbytes, inlen, outbytes, outlen);

   }

   table_head_size = (short) sizeof(struct table_head);
   table_hd.version = 1;

   table_hd.maxtval = maxtval;
   fwrite(&table_hd, table_head_size, 1, out_fd); 
   fwrite(map_table, data_map_size, 1, out_fd);

   fclose(out_fd);
   printf("\"%s\" \"%s\" \"libc.so.1\" \"__iconv_flh_bom_idxh_mbs\" \"libc.so.1\"\
 \"__iconv_control_bom\" \"libc.so.1\" \"__iconv_unicode_signature\" FILE \"%s_%s.map\";\n", from_codeset, to_codeset,
         from_codeset, to_codeset);
   printf("\"%s\" \"%s\" \"libc.so.1\" \"__iconv_utf8_idxh_mbs\" \"libc.so.1\"\
 \"__iconv_control\" FILE \"%s_%s.map\";\n", "UTF-8", to_codeset,
         from_codeset, to_codeset);
   exit(0);
}

int map_to_table(uchar_t *inbytes, uint_t inlen,
   uchar_t *outbytes, uint_t outlen)
{
   uint_t in_int=0, out_int=0;
   int  i;
   char **p;

   for (i = 0; i < inlen; i++)
        in_int = in_int | inbytes[i] << 8*(inlen - i - 1);
   for (i = outlen-1; i >=0 ; i--)
        out_int = out_int | outbytes[i] << 8* (i);

   map_table [in_int] = (ushort_t) out_int;

   maxtval = MAX(maxtval, in_int);

   if (verbose)
	if (in_int) fprintf(verbose_fd, "in %x, out %x\n", (uint_t) in_int,
			(ushort_t) map_table [in_int]);
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
