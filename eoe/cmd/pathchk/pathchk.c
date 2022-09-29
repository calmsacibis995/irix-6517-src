/*
 * pathchk
 *
 *      pathchk - check pathnames
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.1 $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pfmt.h>
#include <locale.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

/*--------------------------------------*
 *  Global extern			*
 *--------------------------------------*/
extern int	optind;

/*--------------------------------------*
 *  Global constants and variables	*
 *--------------------------------------*/
const char msg_usage  []=":849:pathchk  [-p]  pathname ...\n";
const char e_lng_path []=":850:Pathname %s longer than %s\n";
const char e_lng_comp []=":851:Component %s longer than %s\n";
const char e_non_port []=":852:Component %s contains non-portable character\n";
const char e_nac_spath[]=":853:Subpath %s is not searchable\n";
const char e_not_dir  []=":854:%s is not a directory\n";
const char e_miss_arg []=":855:Missing arguments\n";
const char e_lak_mem  []=":856:Lack of memory\n";
const char e_err_prg  []=":857:Program error\n";

const char S_catal[]=	"uxcore";
const char S_label[]=	"UX:pathchk";
const char psx_legal_chars[]=	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-";
const char psx_pmax[]=	"_POSIX_PATH_MAX";
const char psx_nmax[]=	"_POSIX_NAME_MAX";
const char fsys_pmax[]=	"PATH_MAX";
const char fsys_nmax[]=	"NAME_MAX";
const char delm_path[]=	"/";
const char curdir[]=	".";
const char slash=	'/';
const char minus=	'-';

int	p_flag=0;

/*--------------------------------------*
 *  Local definition			*
 *--------------------------------------*/
#define TRUE		1	/* true				*/
#define FALSE		0	/* false			*/
#define SUCCESS		0	/* legal pathname		*/
#define FAIL		1	/* illegal pathname 		*/
#define MAX_ALLOC	10	/* max alloc memory 		*/

#define ERR_USE_OPT	1	/* illegal option 		*/
#define ERR_NO_ARG	2	/* missing arguments   		*/
#define ERR_LK_MEM	3	/* lack of memory		*/
#define ERR_PRGM	4	/* program error		*/
#define ERR_RV_SIG	5	/* receive signal		*/
#define ERR_PATHNM	10	/* pathname error		*/
#define ERR_MAXPATH	255	/* max pathname error		*/

int  opt_ana(int,char **),fsyscheck(char *),posixcheck(char *);
char *p_malloc(int),*piktok(char *,int *);
void p_free(),prev_pos(char *,int,int);

/*------------------------------------------------------------------
 * Function : check pathname legal or not
 * Process  : 1) get option and analize it
 *	      2) check pathname legal or not
 * Return   : 0		  = legal names
 *	      ERR_USE_OPT = illegal option
 *	      ERR_NO_ARG  = missing arguments
 *	      ERR_LK_MEM  = lack of memory
 *	      ERR_PRGM    = program error
 *	      ERR_PATHNM+n= illegal pathnames
 * Usage    : pathchk [-p] pathname ...
 *------------------------------------------------------------------*/
main(int argc, char *argv[])
{
    int		i,rtn=0,arg_rtn,(*func)();

    (void) setlocale(LC_ALL,"");
    (void) setlabel (S_label);
    (void) setcat   (S_catal);

/*  check input options			*/
    arg_rtn = opt_ana(argc,argv);
    if (arg_rtn)
    {
	if (arg_rtn == ERR_NO_ARG)
	    pfmt(stderr,MM_ERROR,e_miss_arg);
	pfmt(stderr,MM_ACTION,msg_usage);
	exit(arg_rtn);
    }

/*  check input pathnames			*/
    if (! p_flag)
	func = fsyscheck;
    else
	func = posixcheck;
    for (i = optind; i < argc; i++)
    {
        rtn += (func)(argv[i]);
	p_free();
    }
    if (rtn)
	rtn += ERR_PATHNM - 1;
    exit((rtn > ERR_MAXPATH) ? ERR_MAXPATH : rtn);
}

/*------------------------------------------------------------------
 * Function : check input options legal or not
 * Return   : 0		  = legal
 *	      ERR_USE_OPT = illegal option 
 *	      ERR_NO_ARG  = missing arguments
 *------------------------------------------------------------------*/
int opt_ana(
int argc,	/* (I) input argument number	*/
char *argv[])	/* (I) input argument list	*/
{
   int	c,rtn=0;
   
   while ((c = getopt(argc,argv,"p")) != EOF)
   {
	switch(c)
	{
	   case 'p':
		p_flag++;
		break;

	   default:
        	return(ERR_USE_OPT);
	}
   }
   if (argc == optind)
	rtn = ERR_NO_ARG;
   return rtn;
}

/*------------------------------------------------------------------
 * Function : check pathname of POSIX
 * Process  : 1) check (total pathname_length <= POSIX_PATH_MAX)
 *	      2) check (each component_length <= POSIX_NAME_MAX)
 *	      3) check each component include illegal_chars [^A-Za-z0-9.-_]
 * Return   : SUCCESS	= legal pathname
 *	      FAIL      = illegal pathname
 *------------------------------------------------------------------*/
int posixcheck(
char *pathname)		/* (I) pathname		*/
{
    char	*component,*name=pathname;
    int		len,dumlen;

    if ((int)strlen(pathname) > _POSIX_PATH_MAX)
    {
        pfmt(stderr,MM_WARNING,e_lng_path,pathname,psx_pmax);
        return FAIL;
    }

    for (;;)
    {
	component = piktok(name,&dumlen);
	if (component == NULL)
	    break;

        len = strlen(component);
        if (len > _POSIX_NAME_MAX)
	{
            pfmt(stderr,MM_WARNING,e_lng_comp,component,psx_nmax);
            return FAIL;
        }
	if ((strspn(component,psx_legal_chars) != len) || (component[0] == minus))
	{
            pfmt(stderr,MM_WARNING,e_non_port,component);
            return FAIL;
        }
	name = NULL;
    }
    return SUCCESS;
}

/*------------------------------------------------------------------
 * Function : check pathname on local file system
 * Spec	    : component is a directory
 *			last		- OK
 *			midst + searchable	- OK
 *			midst + not searchable	- Error
 *	      component is a file
 *			last 		- OK
 *			midst 		- Error
 *	      component is not exist	- check lenght only
 * Return   : SUCCESS	= legal pathname
 *	      FAIL      = illegal pathname
 * Exit     : ERR_PRGM  = program error
 * Note     : It is assumed that any character except / is legal.
 *------------------------------------------------------------------*/
int fsyscheck(
char *pathname)		/* (I) pathname		*/
{
    int		i,rtn=SUCCESS,len,staterr,pathexists=TRUE;
    int		contnamemax,contpathmax,dumlen=0;
    int		ttllen,cmplen=0;
    char	*pathsofar,*path,*component,*name;
    const char	*start;
    struct stat buf;

    len	        = strlen(pathname);
    pathsofar   = p_malloc(len+1);
    path        = p_malloc(len+1);
    pathsofar[0]= '\0';
    strcpy(path,pathname);

/*  decide path_max and name_max of current system	*/
    if (pathname[0] != slash)		/* relative pathname	*/
	start	= curdir;
    else				/* absolute pathname	*/
	start 	= delm_path;
    component	= (char *) start;
    contnamemax = pathconf(start,_PC_NAME_MAX);
    contpathmax	= pathconf(start,_PC_PATH_MAX);
    if ((contnamemax == -1) || (contpathmax == -1))
	goto err_chk;			/* never occured ??	*/

/*  check pathname status and length of component	*/
    name 	= path;
    for (;;)
    {
	component = piktok(name,&dumlen);	/* pick up component */
	if (component == NULL)
	    break;
	    
	if (name == NULL)		/* without first time	*/
	    dumlen ++;
	for (i = 0; i < dumlen; i++)	/* make input pathname	*/
	    strcat(pathsofar,delm_path);
	strcat(pathsofar,component);
	cmplen = strlen(component);

        if (pathexists)			/* exist previous path	*/
	{
            staterr = stat(pathsofar,&buf);	/* get file status */
            if (staterr == -1)
	    {
err_chk:
		ttllen = strlen(pathsofar);
                switch (errno)
		{
                case EACCES:	/* prohibit to access		*/
		    prev_pos(pathsofar,ttllen,dumlen); /* remove cur component */
                case ELOOP:	/* too many symbolic links	*/
                case EMULTIHOP:	/* need multihop but prohibit	*/
                case ENOLINK:	/* not active remote path	*/
                    pfmt(stderr,MM_WARNING,e_nac_spath,pathsofar);
                    return FAIL;

                case ENAMETOOLONG: /* too long pathname or component */
		    if (ttllen > contpathmax)
			pfmt(stderr,MM_WARNING,e_lng_path,pathsofar,fsys_pmax);
		    else
			pfmt(stderr,MM_WARNING,e_lng_comp,component,fsys_nmax);
                    return FAIL;

                case ENOENT:	/* not exist or null pathname	*/
                    pathexists = FALSE;
                    break;

                case ENOTDIR:	/* path prefix is not a directory */
		    prev_pos(pathsofar,ttllen,dumlen); /* remove cur component */
        	    pfmt(stderr,MM_WARNING,e_not_dir,pathsofar);
                    return FAIL;

                case EOVERFLOW:	/* component is too large to store */
                    assert("We stuffed up");
                    break;

		default:
		    pfmt(stderr,MM_ERROR,e_err_prg);
		    exit(ERR_PRGM);
                }		/* end -- switch(errno)		*/
            }
            else		/* staterr != 1			*/
	    {
/*  .   .   .   decide path_max and name_max of current system	*/
		contnamemax = pathconf(pathsofar,_PC_NAME_MAX);
		contpathmax = pathconf(pathsofar,_PC_PATH_MAX);
    		if ((contnamemax == -1) || (contpathmax == -1))
		    goto err_chk;	/* never occured ??	*/
	    }			/* end -- if (staterr == -1)	*/
	}			/* end -- if (pathexists)	*/
/*  .   check length of component				*/
        if (cmplen > contnamemax)
	{
	    pfmt(stderr,MM_WARNING,e_lng_comp,component,fsys_nmax);
	    return FAIL;
	}
	name = NULL;
    }				/* end -- for (;;)		*/

/*  check total pathname length	*/
    if (len > contpathmax)
    {
	pfmt(stderr,MM_WARNING,e_lng_path,pathname,fsys_pmax);
	rtn = FAIL;
    }
    return rtn;
}

/*------------------------------------------------------------------
 * Function : pick up token separated with '/'
 *	      Same as strtok, but sets the number of skipped
 *	      leading separators.
 *------------------------------------------------------------------*/
char *piktok(
char *string,	/* (I) 1st time address of data, others NULL	*/
int *dumlen)	/* (O) number of skipped leading separators	*/
{
	register char	*q, *r;
	static char	*savept;

	/*first or subsequent call*/
	if (string == NULL)
		string = savept;

	if(string == 0)		/* return if no tokens remaining */
		return(NULL);

	*dumlen = strspn(string, delm_path);
	q = string + *dumlen;	/* skip leading separators */

	if(*q == '\0')		/* return if no tokens remaining */
		return(NULL);

	if((r = strpbrk(q, delm_path)) == NULL)	/* move past token */
		savept = 0;	/* indicate this is last token */
	else
	{
		*r = '\0';
		savept = r+1;
	}
	return(q);
}

/*------------------------------------------------------------------
 * Function : search n-th prev pos of '/'
 *------------------------------------------------------------------*/
void prev_pos(
char *name,	/* (I) data string			*/
int ttllen,	/* (I) length of string			*/
int skipno)	/* (I) n-th previous position of '/'	*/
{
    int         prev,skip;

    for (prev = ttllen-1,skip = 0; prev > 0 && skip < skipno ; prev--)
    {
        if (name[prev] == slash)
	    skip ++;
    }
    name[prev+1] = '\0';
}

static char *save[MAX_ALLOC];
static int  sv_cnt=0;
/*------------------------------------------------------------------
 * Function : alloc memory
 *------------------------------------------------------------------*/
char *p_malloc(
int size)	/* (I) allocation mempry size : byte	*/
{
    char	*mem;

    mem = malloc(size);
    if (mem == NULL)
    {
	pfmt(stderr,MM_ERROR,e_lak_mem);
	exit(ERR_LK_MEM);
    }
    else
    {
	if (sv_cnt < MAX_ALLOC)
	    save[sv_cnt++] = mem;
	return mem;
    }
}

/*------------------------------------------------------------------
 * Function : free alloc memory
 *------------------------------------------------------------------*/
void p_free()
{
    int		i;

    for (i = 0; i < sv_cnt; i++)
        free(save[i]);
    sv_cnt = 0; 
}

