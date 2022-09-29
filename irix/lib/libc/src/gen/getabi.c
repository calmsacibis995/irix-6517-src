/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994 Silicon Graphics, Inc.           	  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * getabi() routine for determining which abi to use.
 *
 */

#ident "$Revision: 1.12 $"

#ifdef __STDC__
        #pragma weak getabi = _getabi
#endif
#include "synonyms.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <getabi.h>

typedef int boolean;
#define FALSE 0
#define TRUE 1

#define USE_HINV_METHOD

typedef char* string;
typedef const char* const_string;
#define same_string(a,b)	(strcmp(a,b) == 0)
#define same_string_prefix(a,b)	(strncmp(a,b,strlen(b)) == 0)

static string program_name _INITBSS;
static boolean has_errors = FALSE;

/* make constant to minimize data in libc */
static const char abi_names[lastabi][5] = { "", "-32", "-o32", "-n32", "-64"};
	
/* utility to copy string, allocates new space */ 
static string
string_copy (const_string s)
{
        string new;
        new = (string) malloc(strlen(s)+1);
        strcpy(new, s);
        return new;
}


/* 
 * Check for various forms of abi in string.
 * Can handle variants in form -32 or -a32 or 32.
 */
static abi_t
get_abi (string str)
{
	abi_t i;
	string s = str;
	if (*s == '-') s++;
	/* check for -a<abi> style option (svr4 syntax) */
	if (*s == 'a') s++;
	for (i = (abi_t)(noabi + 1); i < lastabi; i++) {
		if (same_string(s, abi_names[i]+1)) {
			return i;
		}
	}
	return noabi;
}

/* abio32 and abi32 are compatible */
#define COMPATIBLE_ABI(a,b) \
	((a == abio32 || a == abi32) && (b == abio32 || b == abi32)) 

/*
 * Set abi and check for mixing of abi's.
 */
static abi_t
set_abi (abi_t old_abi, abi_t new_abi)
{
	if (old_abi != noabi && old_abi != new_abi && !COMPATIBLE_ABI(old_abi,new_abi)) {
		fprintf(stderr, "%s ERROR:  cannot mix %s abi with %s abi\n", 
			program_name, abi_names[new_abi], abi_names[old_abi]);
		has_errors = TRUE;
	}
	return new_abi;
}


/* set isa if -mips* flag */ 
static int
get_isa (string s)
{
	if (*s == '-') s++;
	if (same_string_prefix(s, "mips")) {
	  if ( *(s+4) == 'x' ) return 15;
	  else return atoi(s+4);
	}
	return 0;
}

/* given an isa, set the abi */
static abi_t
set_abi_from_isa (int mips_isa, abi_t default_abi)
{
	if (mips_isa >= 3) {
		if (default_abi == abi64)
			return abi64;
		else
			return abin32;
	} 
	else if (mips_isa >= 1) {
		return abio32;
	}
	return noabi;
}


#ifdef USE_HINV_METHOD

/* the following inventory code is modified from hinv() */
#include <invent.h>
#include <sys/sbd.h>

#define TFP_INV_CTYPE 0x10
#define T5_INV_CTYPE 0x9

static int
search_invent_func (inventory_t *pinvent, void *arg) 
{
	unsigned short ctype;
	if (arg) ;	/* dummy use to avoid fec warning */
        if ((pinvent->inv_class == INV_PROCESSOR) &&
            (pinvent->inv_type  == INV_CPUCHIP))
	{
		/* inv_state contains fill, chip, revision # bitfields. */
		ctype = ((pinvent->inv_state & C0_IMPMASK) >> C0_IMPSHIFT);
		if (ctype == TFP_INV_CTYPE) {
			return (int) TRUE;
		} else {
			return (int) FALSE;
		}
        }
        return 0;
}

static boolean
is_r8k_machine (void)
{
	int rc;
        rc = scaninvent((int (*)()) search_invent_func, NULL);
        if (rc < 0) {
		fprintf(stderr, "%s ERROR:  can't read inventory table\n", program_name);
		has_errors = TRUE;
		return FALSE;
	} else {
		return (boolean) rc;
	}
}
/* end inventory code */

#endif	/* USE_HINV_METHOD */


/* check for anything other than -DEFAULT */
static void
check_default_options (string s)
{
	int i = 0;
	while (isspace(s[i])) i++;	/* skip leading space */
	if (same_string_prefix(&s[i], "-DEFAULT:")) {
		while (!isspace(s[i])) i++;	/* skip -DEFAULT: option */
	}
	while (isspace(s[i])) i++;	/* skip trailing space */
	if (s[i] != '\0') {		/* should be end of line */
		fprintf(stderr, "WARNING:  compiler.defaults file should only have -DEFAULTS: option, not %s\n", &s[i]);
	}
}

#define DEFAULT_DEFAULTS_FILE	"/etc/compiler.defaults"
/* note that we don't look under toolroot by default */

/* Get option string from compiler.defaults file. */
/* We use $COMPILER_DEFAULTS_PATH to find the compiler.defaults file */
static string
get_driver_defaults (void)
{
	struct stat sbuf;
	char s[MAXPATHLEN];
	char fname[MAXPATHLEN];	/* file name */
	string pname;	/* path name */
	string paths;	/* list of paths */
	FILE *f;
	paths = getenv("COMPILER_DEFAULTS_PATH");
	if (paths == NULL) {
		sprintf(fname, "%s", DEFAULT_DEFAULTS_FILE);
	} else {
		/* use environment variable search path */
		pname = strtok(paths, ":");
		while (pname != NULL) {
			sprintf(fname, "%s/compiler.defaults", pname);
			if (stat(fname, &sbuf) != -1)
				break;	/* file exists */
			else
				pname = strtok(NULL, ":");
		}
	}
	f = fopen (fname, "r");
	if (f == NULL) {
		return NULL;
	}
	if (fgets (s, MAXPATHLEN, f) == NULL) return NULL;
	check_default_options(s);
	return string_copy(s);
}

/* Parse strings like -DEFAULT:abi=n32:isa=mips4:proc=r5000 */
static abi_t
get_default_abi_arg (const_string s)
{
	abi_t default_abi = noabi;
	string p;
	p = string_copy(s + strlen("-DEFAULT:"));
	p = strtok(p, "=");
	while (p != NULL) {
		if (same_string(p, "abi")) {
			p = strtok(NULL, ":");
			default_abi = get_abi (p);
		}
		else {
			p = strtok(NULL, ":");
		}
		if (p != NULL) {
			p = strtok(NULL, "=");
		}
	}
	return default_abi;
}

/*
 * Routines to modify argv list
 */

/* append option name to argv list */
static char**
append_to_argv (char **argv, int *argc, const_string option)
{
	char **p;
	int size = ((*argc)+2)*(int)sizeof(char*);	/* +1 opt +2 nul */
	/* argv is on stack, not malloc'ed, so can't realloc */
	p = (char**) malloc(size);
	memcpy(p, argv, (*argc)*sizeof(char*));
	p[*argc] = (string) option;
	(*argc)++;
	p[*argc] = NULL;
	return p;
}

/* prepend option name to front of argv list */
static char **
prepend_to_argv (char **argv, int *argc, const_string option)
{
	char **p;
	int size = ((*argc)+2)*(int)sizeof(char*);	/* +1 opt +2 nul */
	/* argv is on stack, not malloc'ed, so can't realloc */
	p = (char**) malloc(size);
	p[0] = argv[0];
	memcpy(p+2, argv+1, (*argc)*sizeof(char*));
	p[1] = (string) option;
	(*argc)++;
	p[*argc] = NULL;
	return p;
}

/* remove option from argv list */
static void
remove_from_argv (char **argv, int *indx)
{
	/* shift args to the left after the abi-arg */
	string *p, *q;
	p = &(argv[*indx]);
	for (q = p + 1; *p != NULL; p++, q++) {
		*p = *q;
	}
	(*indx)--;	/* because we have shifted args */
}


/* 
 * Get default abi.
 * First check SGI_ABI,
 * then -DEFAULT option
 * then machine-type.
 */
static abi_t
get_default_abi (string default_arg)
{
	abi_t default_abi = noabi;
	string p;

	/* This variable could be set in /etc/cshrc or by user */
	if ((p = getenv ("SGI_ABI")) != NULL) {
		default_abi = get_abi (p);
	}

	if (default_abi == noabi && default_arg != NULL) {
		/* implicitly sets default_abi */
		default_abi = get_default_abi_arg(default_arg);
	}

	/*
	 *  If no abi was specified on the commandline nor in the
	 *  SGI_ABI environment variable, then choose based upon
	 *  the kind of machine we're on.
	 */
	if (default_abi == noabi) {
		/* check what hardware you are running */
		if (is_r8k_machine()) {
			default_abi = abi64;
		} else {
			default_abi = abio32;
		}
	}
	return default_abi;
}

/*
 * Main routine.
 * Searches the argv list and also checks the environment and
 * hardware to see what the abi is.  The modify_abi_arg
 * says to either leave the arg alone, or remove it from 
 * argv, or add the default value to argv. 
 */
extern abi_t 
getabi (boolean generic_size, int modify_abi_arg, char ***argv, int *argc)
{
	int i;
	abi_t abi = noabi;
	abi_t nabi;
	abi_t default_abi = noabi;
	string default_arg = NULL;
	boolean use_default = TRUE;
	string p;
	int mips_isa = 0;

	program_name = *argv[0];

	default_arg = get_driver_defaults();

	if (modify_abi_arg != IGNORE_ABI_ARG) {
		/*
	 	 *  Scan the commandline for the which ABI to use
	 	 */
		for (i = 1; i < *argc; i++) {
			p = (*argv)[i];

			if (*p != '-') continue;
			nabi = get_abi(p);
			if (nabi != noabi) {
			    abi = set_abi (abi, nabi);
			    if (modify_abi_arg == ADD_ABI_ARG) {
			    	/* if used obsolete name, 
				 * replace with new name */
			    	if (!same_string(p, abi_names[abi])) {
					p = string_copy(abi_names[abi]);
					(*argv)[i] = p;
			    	}
			    } else if (modify_abi_arg == REMOVE_ABI_ARG) {
				remove_from_argv(*argv, &i);
				(*argc)--;
			    }
			    use_default = FALSE;
			} 
			else if (same_string_prefix(p, "-mips")) {
			    mips_isa = get_isa(p);
			}
    		}
	}

	/*
	 *  If no abi was specified on the commandline,
	 *  find the default abi.
	 */
	if (abi == noabi) {
		default_abi = get_default_abi (default_arg);
	}

	/*
	 *  If no abi was specified on the commandline,
	 *  use the -mips value if that was given.
	 */
	if (abi == noabi && mips_isa != 0) {
		abi = set_abi_from_isa (mips_isa, default_abi);
	}

	/*
	 *  If no explicit abi was specified, use the default value.
	 */
	if (abi == noabi) {
		abi = default_abi;
	}

	if (abi == abi32) {
		/* for now, -32 means -o32; may later mean -n32 */
		abi = abio32;
	}
	
	if (generic_size == GENERIC_ABI_SIZE) {
		switch (abi) {
		case abio32:
		case abin32:
			abi = abi32;
			break;
		}
	}

	if (use_default && modify_abi_arg == ADD_ABI_ARG) {
		/* add abi option to argv, so don't have to recalculate it */
		*argv = append_to_argv (*argv, argc, abi_names[abi]);
	}
	else if (use_default && modify_abi_arg == ADD_FRONT_ABI_ARG) {
		/* add the abi option to the front of the list */
		*argv = prepend_to_argv (*argv,argc, abi_names[abi]);
	}

	if (has_errors) {
		return noabi;
	} else {
		return abi;
	}
}

