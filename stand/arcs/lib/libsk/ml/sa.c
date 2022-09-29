/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if SA_DIAG
/*
 * stand-alone diag support functions
 */

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

#include "sa.h"
/*
 * not implemented message
 */
static int
sa_notimpl(char *cmd, char *str)
{
	printf("%s: %s - not implemented\n", cmd, str);
	return(0);
}

#if defined(SFLASH) || defined(IP30)
/*
 * dump data in hex/ascii format
 */
void
show_bytes(char *ptr, int count)
{
	extern void dump_bytes(char *ptr, int count, int flag /*show addr*/);
	dump_bytes(ptr, count, 1);
}
#endif /* SFLASH || IP30 */

/*
 * print byte field
 */
void
sa_prbytes(char *str, u_int addr, u_char *p, int nb)
{
	int n;

	if( !str)
	    str = "";
	printf("%s0x%x ", str, addr);
	for(n = 0; n < nb; n++) {
	    printf("0x%x ", *p++);
	    addr++;
	    if((n & 0x7) == 7)
		printf("\n%s0x%x ", str, addr);
	}
	if(nb & 0x7)
	    printf("\n");
}

/*
 * check number
 */
static int
sa_chknbr(unchar c, int dec)
{
	if((c >= '0') && (c <= '9'))
	    return(1);
	if(dec)
	    return(0);
	if((c >= 'A') && (c <= 'F'))
	    return(1);
	if((c >= 'a') && (c <= 'f'))
	    return(1);
	return(0);
}

/*
 * check number as argument
 */
int
sa_chkarg(char *p)
{
	int dec;

	if(*p == '-')
	    p++;
	if( !(dec = strncmp(p, "0x", 2)))
	    p += 2;
	do {
	    if( !sa_chknbr(*p++, dec))
		return(0);
	} while(*p);
	return(1);
}

/*
 * error message for illegal arg
 */
int
sa_illarg(const char *cmd, char *s)
{
	printf("%s: illegal argument '%s'\n", cmd, s);
	return(0);
}

/*
 * error message for insufficient args
 */
int
sa_nargs(char *cmd)
{
	printf("%s: not enough arguments\n", cmd);
	return(0);
}

/*
 * error message for illegal command
 */
int
sa_illcmd(const char *cmd, char *str)
{
	printf("%s: '%s' illegal command\n", cmd, str);
	printf("%s ? lists the valid commands\n", cmd);
	return(0);
}

/*
 * error message for illegal test command
 */
static int
sa_badcmd(char *cmd)
{
	printf("unknown command \"%s\"\n", cmd);
	return(1);
}

/*
 * find a varibale name
 */
struct SaVar *
sa_findvar(struct SaVar *vp, char *s)
{
	for(; vp->name; vp++) {
	    if( !strcmp(s, vp->name))
		return(vp);
	}
	return(0);
}

/*
 * parse variable setting
 */
void
sa_getvars(struct SaVar *vp, char **ap, const char *cmd)
{
	register char *s;
	struct SaVar *var;
	int val;

	for(; *ap; ap++) {
	    if( !(s = strchr(*ap, '='))) {
		printf("%s: syntax error on '%s'\n", cmd, *ap);
		continue;
	    }
	    *s++ = 0;
	    if( !(var = sa_findvar(vp, *ap))) {
		printf("%s: '%s' - unknown variable\n", cmd, *ap);
		continue;
	    }
	    if( !sa_chkarg(s)) {
		(void)sa_illarg(cmd, s);
		continue;
	    }
	    val = atoi(s);
	    if(var->func)
		(void)(*var->func)(var, val);
	    else
		*var->val = val;
	}
}

/*
 * list all variables
 */
int
sa_listvars(struct SaVar *vp, char *str)
{
	printf("%s variables:\n", str);
	for(; vp->name; vp++)
	    printf("  %s=%d (0x%x)\n", vp->name, *vp->val, *vp->val);
	return(0);
}

/*
 * call handler for command
 */
int
sa_cmd(int argc, char **argv, struct SaCmd *cmdp,
       const char *cmd, int (*probe)())
{
	for(; cmdp->cmd; cmdp++) {
	    if( !strcmp(cmdp->cmd, argv[1])) {
		if(probe && (cmdp->flags & SCF_PROBE)) {
		    if( !(*probe)())
			return(1);
		}
		return((*cmdp->func)(argc, argv));
	    }
	}
	return(sa_illcmd(cmd, argv[1]));
}

/*
 * print general usage message
 */
int
sa_usage(const char *cmd, char **strp)
{
	while(*strp)
	    printf("%s %s\n", cmd, *strp++);
	return(0);
}

/*
 * print string list
 */
int
sa_prstr(char **strp)
{
	while(*strp)
	    printf("%s\n", *strp++);
	return(0);
}

/*
 * usage strings for flags
 */
static	char *sa_fstr[] = {
	"flag                       - print status of flags",
	"flag all|off               - all flags on/off",
	"flag +name ...             - switch flag on",
	"flag -name ...             - switch flag off",
	"flag +name -name ...       - mixed setting of flags",
	0,
};

/*
 * print usage of flags
 */
int
sa_fusage(const char *cmd)
{
	return(sa_usage(cmd, sa_fstr));
}

/*
 * usage strings for variables
 */
static	char *sa_vstr[] = {
	"set                        - print variables and values",
	"set var=val ...            - set a variable",
	0,
};

/*
 * print usage of set
 */
int
sa_vusage(const char *cmd)
{
	return(sa_usage(cmd, sa_vstr));
}

/*
 * find a flag name
 */
static struct SaFlag *
sa_findflg(struct SaFlag *lfp, char *name)
{
	for(; lfp->name; lfp++) {
	    if( !strcmp((const char *)lfp->name, name))
		return(lfp);
	}
	return(0);
}

/*
 * print status of flags
 */
static int
sa_prflags(struct SaFlag *fxx, uint *lflags)
{
	uint flags = *lflags;

	for(; fxx->name; fxx++) {
	    printf("  %10s  %s    - %s\n", fxx->name,
		(fxx->bit & flags)? "on " : "off", fxx->help);
	}
	return(0);
}

/*
 * call all functions for flags
 */
static int
sa_call(struct SaFlag *fxx, int flag)
{
	for(; fxx->name; fxx++) {
	    if(fxx->func)
		(void)(*fxx->func)(fxx, flag);
	}
	return(0);
}

/*
 * handle flag command line
 */
int
sa_flag(int argc, char **argv, const char *cmd, struct SaFlag *fxx, uint *lflags)
{
	int flag;
	char *s;
	struct SaFlag *lfp;

	if(argc < 3)
	    return(sa_prflags(fxx, lflags));
	if( !strcmp(argv[2], "off")) {
	    *lflags = 0;
	    return(sa_call(fxx, 0));
	} else if( !strcmp(argv[2], "all")) {
	    *lflags = 0xffffffff;
	    return(sa_call(fxx, 1));
	}
	for(argc -= 2, argv += 2; argc > 0; argc--) {
	    flag = 0;
	    s = *argv++;
	    switch(*s++) {
		case '+' :			/* switch on */
		    flag = 1;
		case '-' :			/* switch off */
		    break;
		default :
		    printf(",i%s: '%s' - illegal flag\n", cmd, s - 1);
		    continue;
	    }
	    if( !(lfp = sa_findflg(fxx, s)))
		printf("%s: '%s' - illegal flag\n", cmd, s);
	    else {
		if(flag)
		    *lflags |= lfp->bit;
		else
		    *lflags &= ~lfp->bit;
		if(lfp->func)
		    (void)(*lfp->func)(lfp, flag);
	    }
	}
	return(0);
}
#endif /* SA_DIAG */
