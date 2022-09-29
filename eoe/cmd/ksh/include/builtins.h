/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ksh:include/builtins.h	1.4.4.1"


#define SYSDECLARE	0x100

#define	SYSLOGIN	(sh.bltin_cmds)
#define SYSEXEC		(sh.bltin_cmds+1)
#define SYSSET		(sh.bltin_cmds+2)
#define SYSNULL		(sh.bltin_cmds+3)
#define SYSTRUE		(sh.bltin_cmds+4)
#define SYSNEWGRP	(sh.bltin_cmds+5)

extern int b_login();
extern int b_exec();
extern int b_set();
extern int b_null();
#ifdef	 apollo
#   ifndef LDYNAMIC
#	define LDYNAMIC	1
#   endif /* !LDYNAMIC */
    extern int b_rootnode();
    extern int b_ver();
#endif	/* apollo */
#ifdef LDYNAMIC
    extern int b_inlib();
#   ifndef apollo
	extern int b_builtin();
#   endif /* !apollo */
#endif /* LDYNAMIC */
extern int b_dot();
extern int b_test();
extern int b_alias();
extern int b_hash();
extern int b_break();
extern int b_chdir();
extern int b_command();
extern int b_continue();
#ifndef ECHOPRINT
    extern int b_echo();
#endif /* ECHOPRINT */
extern int b_ret_exit();
extern int b_export();
extern int b_eval();
extern int b_fc();
#ifdef JOBS
    extern int b_jobs();
    extern int b_kill();
#   ifdef SIGTSTP
	extern int b_bgfg();
#   endif	/* SIGTSTP */
#endif	/* JOBS */
extern int b_let();
extern int b_print();
extern int b_pwd();
extern int b_read();
extern int b_readonly();
extern int b_shift();
extern int b_test();
extern int b_times();
extern int b_trap();
extern int b_typeset();
extern int b_ulimit();
extern int b_limit();
extern int b_unlimit();
extern int b_umask();
extern int b_unalias();
#ifdef UNIVERSE
    extern int b_universe();
#endif /* UNIVERSE */
extern int b_unset();
extern int b_wait();
extern int b_whence();
extern int b_type();
extern int b_getopts();
#ifdef FS_3D
    extern int b_vpath_map();
#endif /* FS_3D */


/* structure for builtin shell variable names and aliases */
struct name_value
{
#ifdef apollo
	/* you can't readonly pointers */
	const char	nv_name[12];
	const char	nv_value[20];
#else
	const char	*nv_name;
	const char	*nv_value;
#endif	/* apollo */
	const unsigned short	nv_flags;
};

#ifdef cray
    /* The cray doesn't allow casting of function pointers to char* */
    struct name_fvalue
    {
	const char	*nv_name;
	const int	(*nv_value)();
	const unsigned short	nv_flags;
    };
#endif /* cray */

/* The following defines are coordinated with data in msg.c */

#define	PATHNOD		(sh.bltin_nodes)
#define PS1NOD		(sh.bltin_nodes+1)
#define PS2NOD		(sh.bltin_nodes+2)
#define IFSNOD		(sh.bltin_nodes+3)
#define PWDNOD		(sh.bltin_nodes+4)
#define HOME		(sh.bltin_nodes+5)
#define MAILNOD		(sh.bltin_nodes+6)
#define REPLYNOD	(sh.bltin_nodes+7)
#define SHELLNOD	(sh.bltin_nodes+8)
#define EDITNOD		(sh.bltin_nodes+9)
#define MCHKNOD		(sh.bltin_nodes+10)
#define RANDNOD		(sh.bltin_nodes+11)
#define ENVNOD		(sh.bltin_nodes+12)
#define HISTFILE	(sh.bltin_nodes+13)
#define HISTSIZE	(sh.bltin_nodes+14)
#define FCEDNOD		(sh.bltin_nodes+15)
#define CDPNOD		(sh.bltin_nodes+16)
#define MAILPNOD	(sh.bltin_nodes+17)
#define PS3NOD		(sh.bltin_nodes+18)
#define OLDPWDNOD	(sh.bltin_nodes+19)
#define VISINOD		(sh.bltin_nodes+20)
#define COLUMNS		(sh.bltin_nodes+21)
#define LINES		(sh.bltin_nodes+22)
#define PPIDNOD		(sh.bltin_nodes+23)
#define L_ARGNOD	(sh.bltin_nodes+24)
#define TMOUTNOD	(sh.bltin_nodes+25)
#define SECONDS		(sh.bltin_nodes+26)
#define ERRNO		(sh.bltin_nodes+27)
#ifdef apollo
#   define LINENO		(sh.bltin_nodes+28)
#   define OPTIND		(sh.bltin_nodes+29)
#   define OPTARG		(sh.bltin_nodes+30)
#else
	/* LINENO is 28 */
	/* OPTIND is 29 */
#define OPTARGNOD		(sh.bltin_nodes+30)
#endif /* apollo */
#define PS4NOD		(sh.bltin_nodes+31)
#define FPATHNOD	(sh.bltin_nodes+32)
#define LANGNOD		(sh.bltin_nodes+33)
#define LCTYPENOD	(sh.bltin_nodes+34)
#ifdef VPIX
#   define DOSPATHNOD	(sh.bltin_nodes+35)
#   define VPIXNOD	(sh.bltin_nodes+36)
#   define NVPIX	2
#else
#   define NVPIX	0
#endif /* VPIX */
#ifdef ACCT
#   define ACCTNOD 	(sh.bltin_nodes+35+NVPIX)
#   define NACCT	NVPIX+1
#else
#   define NACCT	NVPIX
#endif	/* ACCT */
#ifdef MULTIBYTE
#   define CSWIDTHNOD 	(sh.bltin_nodes+35+NACCT)
#   define NMULTI	NACCT+1
#else
#   define NMULTI       NACCT
#endif /* MULTIBYTE */
#ifdef apollo
#   define SYSTYPENOD	(sh.bltin_nodes+35+NMULTI)
#endif /* apollo */

#define is_rbuiltin(t)	((t)->com.comnamp && \
			nam_istype((t)->com.comnamp,N_BLTIN|BLT_FSUB)==N_BLTIN)

extern const char	e_unlimited[];
extern const char	e_ulimit[];
extern const char	e_notdir[];
extern const char	e_direct[];
extern const char	e_defedit[];
#ifdef EMULTIHOP
    extern const char	e_multihop[];
#endif /* EMULTIHOP */
#ifdef ENAMETOOLONG
    extern const char	e_longname[];
#endif /* ENAMETOOLONG */
#ifdef ENOLINK
    extern const char	e_link[];
#endif /* ENOLINK */
#ifdef VPIX
    extern const char	e_vpix[];
    extern const char	e_vpixdir[];
#endif /* VPIX */
#ifdef apollo
    extern MSG	e_rootnode;
    extern MSG	e_nover;
    extern MSG	e_badver;
#endif /* apollo */
#ifdef LDYNAMIC
    extern MSG	e_badinlib;
#endif /* LDYNAMIC */
extern const struct name_value node_names[];
extern const struct name_value alias_names[];
extern const struct name_value tracked_names[];
#ifdef cray
    extern const struct name_fvalue	built_ins[];
#else
    extern const struct name_value	built_ins[];
#endif /* cray */
#ifdef _sys_resource_
    extern const struct sysmsgnod limit_names[];
    extern const struct sys2msgnod blimit_names[];
#else
#   ifdef VLIMIT
	extern struct sysmsgnod limit_names[];
#   endif	/* VLIMIT */
#endif	/* _sys_resource_ */

