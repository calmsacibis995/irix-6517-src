/**************************************************************************
 *                                                                        *
 * Copyright (C) 1995 Silicon Graphics, Inc.                         *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <locale.h>
#include <unistd.h>
#include <locale.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <iconv.h>


#define BUGFSIZ 21

extern int optind;
extern int opterr;
extern char *optarg;

char	cmd_label[] = "UX:iconv";

unsigned char inbuf[BUFSIZ], outbuf[BUFSIZ];

int	ignore_error=1;
int	err_flag = 0;

void error_msg()
{
   switch (errno) {
      case EILSEQ:
	 _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	   gettxt(_SGI_MMX_iconv_eilseq, 
	       "Input conversion stopped due to an input byte that does not belong to the input code."));
	break;
      case E2BIG:
         _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
           gettxt(_SGI_MMX_iconv_e2big,
	 	"Input conversion stopped due to lack of space in the output buffer."));
	break;
      case EINVAL:
         _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
           gettxt(_SGI_MMX_iconv_einval,
	 	"Input conversion stopped due to an incomplete character or shift sequence at the end of the input buffer."));
	break;
      case EBADF:
         _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
           gettxt(_SGI_MMX_iconv_ebadf,
	 	"The cd argument is not a valid open conversion descriptor."));
	break;
   }
   err_flag = 1;
}

int dump_iconv_list()
{
    char	** ___iconv_gettable( void );
    char	** ps = ___iconv_gettable();
    iconv_t	    pi;
    char	 * fmt = "%s\t%s\n";

    if ( ! ps ) {
	perror( "iconv" );
	return 0;
    }

    if ( ignore_error ) {

	while ( * ps ) {
	    printf( fmt, ps[ 0 ], ps[ 1 ] );
	    ps += 2;
	}

    } else {

	while ( * ps ) {

	    pi = iconv_open( ps[ 1 ], ps[ 0 ] );
    
	    if ( pi != ( iconv_t ) -1 ) {
		printf( fmt, ps[ 0 ], ps[ 1 ] );
		iconv_close( pi );
	    }

	    ps += 2;
	}
    }
    return 0;
}

main(argc, argv)
int argc;
char **argv;
{
   register int c;
   char *fcode;
   char *tcode;
   int	 list;
   int fd;
   iconv_t cd;
   register int n;
   size_t inbytesleft, outbytesleft;
   unsigned char *inbuf1 = inbuf, *outbuf1 = outbuf;
   char	error_str[50];
   int	do_cat = 0;

   (void)setlocale(LC_ALL, "");
   (void)setcat("uxsgicore");
   (void)setlabel(cmd_label);


   fcode = (char*)NULL;
   tcode = (char*)NULL;
   list = 0;
   c = 0;

   while ((c = getopt(argc, argv, "aceilf:t:")) != EOF) {

      switch (c) {

         case 'f':
            fcode = optarg;
            break;   

         case 't':
            tcode = optarg;
            break;

	 case 'e':
	    ignore_error = 0;
	    break;

	 case 'i':
	    ignore_error = 1;
	    break;

	 case 'l':
	    list = 1;
	    break;
	 case 'a':
	    do_cat = 0;
	    break;
	 case 'c':
	    do_cat = 1;
	    break;

         default:
            usage_iconv(0);
      }

   }

   if (!fcode || !tcode)
   {
      if ( list ) {
	dump_iconv_list();
	return 0;
      }
      usage_iconv(1);
   }

  if (optind >= argc) { /* no more operand stdin */
	optind ++;
	fd = 0;
	goto conv_point;
  }

  for ( ; optind < argc; optind++){ /* exit until finish all the input files */
   fd = open(argv[optind],0);
      if (fd < 0) {
         _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
 	   gettxt(_SGI_MMX_iconv_nofile, "Cannot open %s: %s"), argv[optind], strerror(errno));
        err_flag = 1;
	continue;
      }

conv_point:
   if (do_cat && !strcmp(fcode, tcode)) /* no conversion */
   { /* cat it out directly - garbage in, garbage out */
	if(cat(fd)) err_flag = 1;
	continue;
   }

   if ((cd = iconv_open(tcode, fcode)) == (iconv_t)-1) {
      _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	gettxt(_SGI_MMX_iconv_noiconv, "Cannot open converter %s"), strerror(errno));
      err_flag = 1;
	continue;
   }

   inbytesleft = 0;
   outbytesleft = BUFSIZ;

   while ((n = read(fd, &inbuf[inbytesleft], BUFSIZ - inbytesleft)) > 0) {

      inbuf1 = inbuf, outbuf1 = outbuf;
      inbytesleft += n;

retry:
      if (iconv(cd, ( char ** ) &inbuf1, &inbytesleft, ( char ** ) &outbuf1, &outbytesleft) == -1) {
         switch (errno) {
            case  EILSEQ:
	       write(1, outbuf, BUFSIZ - outbytesleft);
	       if (!ignore_error) error_msg();
	       /* refresh the output buffer */
	       outbytesleft = BUFSIZ;
	       outbuf1 = outbuf;
	       inbuf1++; /* skip the first invalid byte and keep trying.. */
	       inbytesleft -= 1;
	       goto retry;

            case EINVAL:
	       bcopy(inbuf1, inbuf, inbytesleft);
	       /*error_msg();*/ /* don't need report the warning since we own the inbuf*/
	       break;

	    case E2BIG:
	       write(1, outbuf, BUFSIZ - outbytesleft);
	       outbytesleft = BUFSIZ;
	       outbuf1 = outbuf;
	       goto retry;
         }

      }
      write(1, outbuf, BUFSIZ - outbytesleft);
      outbytesleft = BUFSIZ;
   }
   close(fd);
   iconv_close(cd);
   }
   if (err_flag) exit(1);
   else exit(0);
}


int usage_iconv(
    int complain
) {
	_sgi_nl_usage(
	    SGINL_USAGE,
	    cmd_label,
	    gettxt(
		_SGI_MMX_iconv_usage,
		"iconv [-ac] [-ie] [-l] [[-f fromcode -t tocode] [file]]"
	    )
	);
        exit(1);
	return 0;
}

#define MAX_ZERO_WR     5

int	cat(int fi)
{
	register int bytes;		/* count for partial writes */
	register int attempts;	/* count for 0 byte writes */
	register int fi_desc=fi;
	register int nitems;
	int     errnbr = 0;

	/*
	 * While not end of file, copy blocks to stdout. 
	 */

	while ((nitems=read(fi_desc, inbuf,BUFSIZ))  > 0) {
		attempts = 0;
		bytes = 0;
		while (bytes < nitems) {
			errnbr = write(1,inbuf+bytes,(unsigned)nitems-bytes);
			if (errnbr < 0) {
			     _sgi_nl_error(SGINL_SYSERR, cmd_label,
           			    gettxt(_SGI_MMX_iconv_write,
					"Write error (%d/%d characters written): %s"),
					bytes, nitems, strerror(errno));
				/*
				 * Expect a write on the output device to always fail, exit
				 */
				return(1);
			}
			else if (errnbr == 0) {
				++attempts;
				if (attempts > MAX_ZERO_WR) {
			     	    _sgi_nl_error(SGINL_SYSERR, cmd_label,
           			        gettxt(_SGI_MMX_iconv_write,
					"Write error (%d/%d characters written): %s"),
					bytes, nitems, "repeated 0 byte writes");
				    return(1);
				}
			}
			else { /* errnbr > 0 */
				bytes += errnbr;
				attempts = 0;
			}
		}
	}
	return(0);
}
