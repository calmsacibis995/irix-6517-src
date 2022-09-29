/*
 * confstr.c
 *
 * 	get configurable variables
 *
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

#ident "$Revision: 1.10 $"

#ifdef __STDC__
	#pragma weak confstr = _confstr
#endif

#include "synonyms.h"
#include <unistd.h>
#include <errno.h>
#include <sgidefs.h>
#include <sys/systeminfo.h>

static const char dflt_path[] = "/usr/bin";
static const char str_path[] = "PATH=";
#define DEFLT

#ifdef DEFLT
static const char dflt_file[] = "/etc/default/login"; /* determine path */
#include <stdio.h>
#include <limits.h>
#endif /* DEFLT */

size_t 
confstr(int name, char *buf, size_t len)
{
	size_t i=0;
	int c;
	int save_errno;
#ifdef DEFLT
	size_t ret = 1;
	FILE *dfile;
#endif
	char *s;

	switch(name) {
	case _CS_PATH:
#ifdef DEFLT
		if ((dfile = fopen(dflt_file, "r")) == (FILE *)NULL)
			ret = 0;
		else
		{
			int i = 0;
			while (i < (sizeof(str_path) - 1))
			{
				if ((c=getc(dfile)) != str_path[i])
				{
					while(c != EOF && c != '\n'
				       	    && (c = getc(dfile)) != '\n'
				            && c != EOF);
					if (c == EOF)
					{
						ret = 0;
						break;
					}
					i = 0;
				}
				else
					i++;
			}
		}
			/* try to read the value from /etc/default/login */
#endif
		if (len > 0)
		{	
#ifdef DEFLT
			if (ret)
			{
				while (--len > 0
		       	       	&& (buf[i] = c = getc(dfile)) != '\n'
		       	       	&& c != EOF)
						i++;
				if (buf[i] == '\n')
					ungetc('\n', dfile);
				buf[i++] = '\0';
				/* may have an extra call to getc if at EOF */
				while ((c = getc(dfile)) != '\n' && c != EOF)
					i++;
			}
			else
			{
#endif
				strncpy(buf, dflt_path, len -1);
	  			/* null terminate the string */
	  			buf[len -1] = '\0';
				i = strlen(dflt_path) + 1;
#ifdef DEFLT
	  		}
#endif
		}
		else
		{
#ifdef DEFLT
			if (ret)
			{
				while((c = getc(dfile)) != '\n' && c != EOF)
					i++;
				i++;
			}
			else
#endif
				i = 9;  	/*strlen(dflt_path) + 1 */
		}
#ifdef DEFLT
		fclose(dfile);
#endif
		ret = i;
		break;

	case _CS_SYSNAME:
		name = SI_SYSNAME;
	case _CS_HOSTNAME:		/* == SI_HOSTNAME     */
	case _CS_RELEASE:		/* == SI_RELEASE      */
	case _CS_VERSION:		/* == SI_VERSION      */
	case _CS_MACHINE:		/* == SI_MACHINE      */
	case _CS_ARCHITECTURE:		/* == SI_ARCHITECTURE */
	case _CS_HW_SERIAL:		/* == SI_HW_SERIAL    */
	case _CS_HW_PROVIDER:		/* == SI_HW_PROVIDER  */
	case _CS_SRPC_DOMAIN:		/* == SI_SRPC_DOMAIN  */
	case _CS_INITTAB_NAME:		/* == SI_INITTAB_NAME */
	case _MIPS_CS_VENDOR:
	case _MIPS_CS_OS_PROVIDER:
	case _MIPS_CS_OS_NAME:
	case _MIPS_CS_HW_NAME:
	case _MIPS_CS_NUM_PROCESSORS:
	case _MIPS_CS_HOSTID:
	case _MIPS_CS_OSREL_MAJ:
	case _MIPS_CS_OSREL_MIN:
	case _MIPS_CS_OSREL_PATCH:
	case _MIPS_CS_PROCESSORS:
	case _MIPS_CS_AVAIL_PROCESSORS:
	case _MIPS_CS_SERIAL:
		/*
		 * XPG explicitly says that if something doesn't have
		 * a config value, 0 is returned and errno is NOT changed
		 */
		save_errno = errno;
		ret = sysinfo(name, buf, (long)len);
		if (ret == -1)
		{
			setoserror(save_errno);
			ret = 0;
		}
		break;

	case _CS_LFS_CFLAGS:
	case _CS_LFS_LDFLAGS:
	case _CS_LFS_LIBS:
	case _CS_LFS_LINTFLAGS:
#if (_MIPS_SIM == _MIPS_SIM_ABI64) || (_MIPS_SIM == _MIPS_SIM_NABI32)
		/* these support large files by default */
		s = "";
		ret = 1;
		goto copys;
#else
		ret = 0;
		setoserror(EINVAL);
		break;
#endif

	case _CS_LFS64_CFLAGS:
	case _CS_LFS64_LDFLAGS:
	case _CS_LFS64_LIBS:
	case _CS_LFS64_LINTFLAGS:
		/* we always support these - no flags required! */
		s = "";
		ret = 1;
		goto copys;

	case _CS_XBS5_ILP32_OFF32_CFLAGS:
	case _CS_XBS5_ILP32_OFF32_LDFLAGS:
		s = "-o32";
		ret = 4;
		goto copys;
	case _CS_XBS5_ILP32_OFFBIG_CFLAGS:
	case _CS_XBS5_ILP32_OFFBIG_LDFLAGS:
		s = "-n32";
		ret = 4;
		goto copys;
	case _CS_XBS5_LP64_OFF64_CFLAGS:
	case _CS_XBS5_LP64_OFF64_LDFLAGS:
	case _CS_XBS5_LPBIG_OFFBIG_CFLAGS:
	case _CS_XBS5_LPBIG_OFFBIG_LDFLAGS:
		s = "-64";
		ret = 3;
		goto copys;
	case _CS_XBS5_ILP32_OFF32_LIBS:
	case _CS_XBS5_ILP32_OFF32_LINTFLAGS:
	case _CS_XBS5_ILP32_OFFBIG_LIBS:
	case _CS_XBS5_ILP32_OFFBIG_LINTFLAGS:
	case _CS_XBS5_LP64_OFF64_LIBS:
	case _CS_XBS5_LP64_OFF64_LINTFLAGS:
	case _CS_XBS5_LPBIG_OFFBIG_LIBS:
	case _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS:
		s = "";
		ret = 1;
		goto copys;
	copys:
		if (len >= ret)
			strcpy(buf, s);
		else if (len) {
			strncpy(buf, s, len - 1);
			buf[len - 1] = '\0';
		}
		break;

	default: 
		ret = 0;
		setoserror(EINVAL);
	}

	return(ret);
}
