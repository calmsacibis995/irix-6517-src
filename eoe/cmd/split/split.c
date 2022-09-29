/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright (c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved. 					*/

/* #ident	"@(#)split:split.c	1.6.1.2" */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/split/RCS/split.c,v 1.7 1998/08/03 15:42:23 morimoto Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <locale.h>
#include <pfmt.h>
#include <string.h>
#include <errno.h>

unsigned count = 1000;        /* default # of lines in each output file */
int     suffix_length = 2;    /* default suffix length */    
int     nbytes;               /* bytes in each output file if -b is specified*/
int     setlines = 0;
int     setbytes = 0;         /* default split a file according to line # */
int	fnumber;              /* actual # of output file */
char	fname[100];           /* name of output file */
char	head[1024];           /* output file directory */
char	*ifil;                /* input filename */
char	*ofil;                /* output full filename */
char	*tail;                /* output file basename */
char	*last;
FILE	*is;                  /* input file stream */
FILE	*os;                  /* output file stream */
int     *base;

main(argc, argv)
char *argv[];
{
    register i, j, c, f;
    int iflg = 0;
    struct statvfs64 stbuf;
    int maxfile;
    int delimit = 0;

    (void)setlocale(LC_ALL, "");
    (void)setcat("uxdfm");
    (void)setlabel("UX:split");

    for (argc--, argv++; argc > 0; argc--, argv++)
	if(**argv == '-' && delimit == 0){
	    if(argv[0][1] == '-') 
	      delimit = 1;
	    else if(isdigit(argv[0][1])) 
	      count = atoi(argv[0]+1);
	    else 
	      switch(argv[0][1]){
		case '\0':
		  iflg = 1;
		  break;

		case 'a':
		  if(argv[0][2] == '\0'){
		      argc--; argv++;
		      suffix_length = atoi(*argv);
		  } else
		    suffix_length = atoi(argv[0]+2);
		  if(suffix_length <= 0){
		      pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
		      pfmt(stderr, MM_ERROR, ":65:suffix_length should be positive.\n");
		      exit(1);
		  }
		  break;
		  
		case 'l':
		  if(setbytes == 1){
		      pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
		      pfmt(stderr, MM_ERROR, ":67:-b and -l shouldn't be used together.\n");
		      exit(1);
		  }
		  setlines = 1;
		  if(argv[0][2] == '\0'){
		      argc--; argv++;
		      count = atoi(*argv);
		  }
		  else
		    count = atoi(argv[0]+2);
		  break;

		case 'b':
		  if(setlines == 1){
		      pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
		      pfmt(stderr, MM_ERROR, ":67:-b and -l shouldn't be used together.\n");
		      exit(1);
		  }
		  setbytes = 1;
		  if(argv[0][2] == '\0'){
		      argc--; argv++;
		      nbytes = atoi(*argv);
		  }
		  else
		    nbytes = atoi(argv[0]+2);
		  if(nbytes <= 0){
		      pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
		      pfmt(stderr, MM_ERROR, ":68:Number of bytes should be positive.\n");
		      exit(1);
		  }
		  if(argv[0][strlen(argv[0])-1] == 'k')
		    nbytes *= 1024;
		  else if (argv[0][strlen(argv[0])-1] == 'm')
		    nbytes *= 1048576;
		  else if (!isdigit(argv[0][strlen(argv[0])-1]))
		    usage();
		  break;
		  
		default:
		  usage();
	      }
	    
	    if(count <= 0){
		pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
		pfmt(stderr, MM_ERROR, ":66:line_count should be positive.\n");
		exit(1);
	    }
	} else if(iflg)         /* process input and output filename */
	  ofil = *argv;
	else {
	    ifil = *argv;
	    iflg = 2;
	} /* finish command line processing */
	  
    if(iflg != 2)
      is = stdin;
    else
      if((is=fopen(ifil,"r")) == NULL) {
	  pfmt(stderr, MM_ERROR, ":3:Cannot open %s: %s\n",
	       ifil, strerror(errno));
	  exit(1);
      }

    if(ofil == 0)
      ofil = "x";
    else {
	if ((tail = strrchr(ofil, '/')) == NULL) {
	    tail = ofil;
	    getcwd(head, sizeof(head));
	}
	else {
	    tail++;
	    strcpy(head, ofil);
	    last = strrchr(head, '/');
	    *++last = '\0';
	}
		
	if (statvfs64(head, &stbuf) < 0) {
	    pfmt(stderr, MM_ERROR, ":56:%s: %s\n", head, strerror(errno));
	    exit(1);
	}

	if (strlen(tail) > (stbuf.f_namemax-suffix_length) ) {
	    pfmt(stderr, MM_ERROR, ":57:More than %d characters in output file name\n",
		 stbuf.f_namemax-suffix_length);
	    exit(1);
	}
    }

    if((base = (int *)malloc(suffix_length * sizeof(int))) == NULL){
	pfmt(stderr, MM_ERROR, ":69:Cannot allocate enough memory.\n");
	exit(1);
    }
    base[0] = 1;
    for (i=1; i<suffix_length; i++)
      base[i] = base[i-1]*26;

    maxfile = base[suffix_length-1]*26;
    
    for(;;){
	f = 1;
	if(!setbytes)        /* set line limit */
	  for(i=0; i<count; i++)
	    do {
		c = getc(is);
		if(c == EOF) {
		    if(f == 0)
		      fclose(os);
		    exit(0);
		}
		if(f) { /* need a new output file */
		    int temp = fnumber;
		    for(f=0; ofil[f]; f++)
		      fname[f] = ofil[f];
		    for(j=suffix_length-1; j>=0; j--){
			fname[f++] = temp/base[j] + 'a';
			temp %= base[j];
		    }
		    fname[f] = '\0';
		    if(++fnumber > maxfile) {
			pfmt(stderr, MM_ERROR,
			     ":58:More output files needed, aborting split\n");
			exit(1);
		    }
		    if((os=fopen(fname,"w")) == NULL) {
			pfmt(stderr, MM_ERROR,
			     ":12:Cannot create %s: %s\n",
			     fname, strerror(errno));
			exit(1);
		    }
		    f = 0;
		}
		putc(c, os);
	    } while(c != '\n');
	else             /* set bytes limit */
	  for(i=0; i<nbytes; i++){
	      c = getc(is);
	      if(c == EOF) {
		  if(f == 0)
		    fclose(os);
		  exit(0);
	      }
	      if(f) { /* need a new output file */
		  int temp = fnumber;
		  for(f=0; ofil[f]; f++)
		    fname[f] = ofil[f];
		  for(j=suffix_length-1; j>=0; j--){
		      fname[f++] = temp/base[j] + 'a';
		      temp %= base[j];
		  }
		  fname[f] = '\0';
		  if(++fnumber > maxfile) {
		      pfmt(stderr, MM_ERROR,
			   ":58:More output files needed, aborting split\n");
		      exit(1);
		  }
		  if((os=fopen(fname,"w")) == NULL) {
		      pfmt(stderr, MM_ERROR,
			   ":12:Cannot create %s: %s\n",
			   fname, strerror(errno));
		      exit(1);
		  }
		  f = 0;
	      }
	      putc(c, os);
	  }
	fclose(os);
    }
}

usage()
{
    pfmt(stderr, MM_ERROR, ":2:Incorrect usage\n");
    pfmt(stderr, MM_ACTION, ":55:Usage: split [-l line_count][-a suffix_length][file[name]]\nor\nsplit -b n[k|m][-a suffix_length][file[name]]\n");
    exit(1);
}


