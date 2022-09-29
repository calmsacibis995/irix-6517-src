/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.6 $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.

		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 
/*
 * Copyright (C) 1989 Sun Microsystems Inc.
 */

/*
 * C shell
 */

/*
 * These wchar_t constants used to be defined as
 * character string constants. 
 */

#include "sh.h"

const wchar_t S_[] = {0};
const wchar_t S_0[]={'0', 0};
const wchar_t S_1[]={'1', 0};
const wchar_t S_ABRT[]={'A','B','R','T', 0};	/* ABRT */
const wchar_t S_ALRM[]={'A','L','R','M', 0};	/* ALRM */
const wchar_t S_AND[] = {'&', 0};	/* & */
const wchar_t S_ANDAND[] = {'&', '&', 0};	/* && */
const wchar_t S_AST[]={'*', 0};
const wchar_t S_AT[] = { '@', 0 };
const wchar_t S_BAR[] = {'|', 0};	/* | */
const wchar_t S_BARBAR[] = {'|','|', 0};	/* || */
const wchar_t S_BRABRA[] = {'{', '}', 0};	/* {} */
const wchar_t S_BRAPPPBRA[] = {'{', ' ', '.', '.', '.', ' ', '}', 0};	/* { ... } */
const wchar_t S_BUS[]={'B','U','S', 0};	/* BUS */
const wchar_t S_CHLD[]={'C','H','L','D', 0};	/* CHLD */
const wchar_t S_COLON[] = {':', 0}; /*:*/
const wchar_t S_CONT[]={'C','O','N','T', 0};	/* CONT */
const wchar_t S_DASHl[] = {'-', 'l', 0};	/*-l */
const wchar_t S_DELIM[] = {' ','\'','"','\t',';','&','<','>','(',')','|','`',0};
const wchar_t S_DOT[] = {'.', 0};
const wchar_t	S_DOTDOT[] = {'.', '.', 0};
const wchar_t S_DOTDOTSLA[]={'.', '.', '/', 0};
const wchar_t S_DOTSLA[]={'.', '/', 0};
const wchar_t S_EMT[]={'E','M','T', 0};	/* EMT */
const wchar_t S_EQ[] = {'=', 0};	/*=*/
const wchar_t S_EXAS[] = {'!', 0};       /* ! */
const wchar_t S_FPE[]={'F','P','E', 0};	/* FPE */
const wchar_t S_HAT[] = {'^', 0};	/* ^ */
const wchar_t S_HOME[] = {'H','O','M','E',0};/*HOME*/
const wchar_t S_HUP[]={'H','U','P', 0};	/* HUP */
const wchar_t S_ILL[]={'I','L','L', 0};	/* ILL */
const wchar_t S_INT[]={'I','N','T', 0};	/* INT */
const wchar_t S_IO[]={'I','O', 0};	/* IO */
const wchar_t S_IOT[] = {'I', 'O', 'T', 0}; /*IOT*/
const wchar_t S_KILL[]={'K','I','L','L', 0};	/* KILL */
const wchar_t S_LANG[]={'L', 'A', 'N', 'G', 0}; /*LANG*/
const wchar_t S_LBRA[] = {'{', 0};	/* { */
const wchar_t S_LBRASP[] = {'(', ' ', 0};	/*( */
const wchar_t S_LC_CTYPE[]={'L', 'C', '_', 'C', 'T', 'Y', 'E', 0}; /*LC_CTYPE*/
const wchar_t S_LC_MESSAGES[]={'L', 'C', '_', 
		   'M', 'E', 'S', 'S', 'A', 'G', 'E', 'S', 0}; /*LC_MESSAGES*/
const wchar_t S_LESLES[]={'<', '<', 0};
const wchar_t S_LOST[]={'L','O','S','T', 0};	/* LOST */
const wchar_t S_LPAR[] = {'(', 0};	/* ( */
const wchar_t S_MINUS[] = {'-',0};/*"-"*/
const wchar_t S_MINl[]={'-', 'l', 0};
const wchar_t S_NDOThistory[] = {'~','/','.','h','i','s','t','o','r','y',0};
const wchar_t S_OTHERSH[] = {'/','b','i','n','/','s','h',0};
const wchar_t S_PARCENTMINUS[] = {'%', '-', 0}; /*%-*/
const wchar_t S_PARCENTPARCENT[] = {'%', '%', 0}; /*%%*/
const wchar_t S_PARCENTPLUS[] = {'%', '+', 0}; /*%+*/
const wchar_t S_PARCENTSHARP[] = {'%', '#', 0}; /*%#*/
const wchar_t S_PATH[] = {'P','A','T','H',0};/*"PATH"*/
const wchar_t S_PERSENTSP[] = {'%',' ',0};
const wchar_t S_PIPE[]={'P','I','P','E', 0};	/* PIPE */
const wchar_t S_PROF[]={'P','R','O','F', 0};	/* PROF */
const wchar_t S_PWD[]={'P', 'W', 'D', 0};
const wchar_t S_Pjob[] = {'%','j','o','b', 0}; /*"%job"*/
const wchar_t S_PjobAND[] = {'%','j','o','b',' ','&',0}; /*"%job &"*/
const wchar_t S_QPPPQ[] = {'`', ' ', '.', '.', '.', ' ', '`', 0}; /*` ... `*/
const wchar_t S_QUIT[]={'Q','U','I','T', 0};	/* QUIT */
const wchar_t S_RBRA[] = {'}', 0};	/* } */
const wchar_t S_RPAR[] = {')', 0}; /*)*/
const wchar_t S_SEGV[]={'S','E','G','V', 0};	/* SEGV */
const wchar_t S_SEMICOLONSP[] = {';', ' ', 0};	/* | */
const wchar_t S_SHARPSP[] = {'#',' ',0};
const wchar_t S_SHELLPATH[] = {'/','b','i','n','/','c','s','h',0};
const wchar_t S_SLADOTcshrc[] = {'/','.','c','s','h','r','c', 0};
const wchar_t S_SLADOThistory[] = {'/','.','h','i','s','t','o','r','y', 0};
const wchar_t S_SLADOTlogin[] = {'/','.','l','o','g','i','n', 0};
const wchar_t S_SLADOTlogout[] = {'/','.','l','o','g','o','u','t', 0};
const wchar_t S_SLASH[] = {'/', 0}; /* "/" */
const wchar_t S_SP[] = {' ', 0};	/* */
const wchar_t S_SPANDANDSP[] = {' ', '&', '&', ' ', 0};	/* && */
const wchar_t S_SPBARBARSP[] = {' ', '|', '|', ' ', 0};	/* || */
const wchar_t S_SPBARSP[] = {' ', '|', ' ', 0};	/* | */
const wchar_t S_SPGTRGTRSP[] = {' ', '>', '>', ' ', 0};	/* >> */
const wchar_t S_SPGTR[] = {' ', '>',0};	/* > */
const wchar_t S_SPLESLESSP[] = {' ', '<', '<', ' ', 0};	/* << */
const wchar_t S_SPLESSP[] = {' ', '<', ' ', 0};	/* < */
const wchar_t S_SPPPP[] = {' ', '.', '.', '.', 0};	/* ... */
const wchar_t S_SPRBRA[] = {' ', ')', 0};	/* )*/
const wchar_t S_STOP[]={'S','T','O','P', 0};	/* STOP */
const wchar_t S_SYS[]={'S','Y','S', 0};	/* SYS */
const wchar_t S_TERM[] = {'T','E','R','M',0};/*TERM*/
const wchar_t S_TIL[] = {'~', 0};	/* ~ */
const wchar_t S_TOPBIT[] = {QUOTE, 0};	/* Was "\200".  A hack! */
const wchar_t S_TRAP[]={'T','R','A','P', 0};	/* TRAP */
const wchar_t S_TSTP[]={'T','S','T','P', 0};	/* TSTP */
const wchar_t S_TTIN[]={'T','T','I','N', 0};	/* TTIN */
const wchar_t S_TTOU[]={'T','T','O','U', 0};	/* TTOU */
const wchar_t S_URG[]={'U','R','G', 0};	/* URG */
const wchar_t S_USAGEFORMAT[] = {'%','U','u',' ','%','S','s',' ','%','E',' ','%','P', ' ','%','X','+','%','D','k',' ','%','I','+','%','O','i','o',' ','%','F','p','f','+', '%','W','w',0};
const wchar_t S_USER[] = {'U','S','E','R',0};/*USER*/
const wchar_t S_USR1[]={'U','S','R','1', 0};	/* USR1 */
const wchar_t S_USR2[]={'U','S','R','2', 0};	/* USR2 */
const wchar_t S_VTALRM[]={'V','T','A','L','R','M', 0};	/* VTALRM */
const wchar_t S_WINCH[]={'W','I','N','C','H', 0};	/* WINCH */
const wchar_t S_XCPU[]={'X','C','P','U', 0};	/* XCPU */
const wchar_t S_XFSZ[]={'X','F','S','Z', 0};	/* XFSZ */
const wchar_t S_PWR[]={'P','W','R', 0};	/* PWR */
const wchar_t S_POLL[]={'P','O','L','L', 0};	/* POLL */
const wchar_t S_alias[] = { 'a','l','i','a','s', 0 };
const wchar_t S_alloc[] = { 'a','l','l','o','c', 0};
const wchar_t S_aout[] = {'a','.','o','u','t',0};
const wchar_t S_argv[]={'a', 'r', 'g', 'v', 0};
const wchar_t S_bg[] = { 'b','g', 0};
const wchar_t S_bin[] = {'/','b','i','n',0};
const wchar_t S_break[] = { 'b','r','e','a','k', 0};
const wchar_t S_breaksw[] = { 'b','r','e','a','k','s','w', 0};
const wchar_t S_bye[] = { 'b','y','e', 0};
const wchar_t S_case[] = { 'c','a','s','e', 0};
const wchar_t S_cd[] = { 'c','d', 0};
const wchar_t S_cdpath[]={'c', 'd', 'p', 'a', 't', 'h', 0};
const wchar_t S_chdir[] = { 'c','h','d','i','r', 0};
const wchar_t S_child[] = {'c', 'h', 'i', 'l', 'd', 0};
const wchar_t S_continue[] = { 'c','o','n','t','i','n','u','e', 0};
const wchar_t S_coredumpsize[] = {'c','o','r','e','d','u','m','p','s','i','z','e',0};/*"coredumpsize"*/
const wchar_t S_cputime[] = {'c','p','u','t','i','m','e',0};/*"cputime"*/
const wchar_t S_csh[]={'c', 's', 'h', 0};
const wchar_t S_cwd[]={'c', 'w', 'd', 0};
const wchar_t S_datasize[] = {'d','a','t','a','s','i','z','e',0};/*"datasize"*/
const wchar_t S_default[] =  { 'd','e','f','a','u','l','t', 0 };
const wchar_t S_descriptors[] = {'d', 'e', 's', 'c', 'r', 'i', 'p', 't', 'o', 'r', 's', 0};
const wchar_t S_devfd[] = { '/', 'd', 'e', 'v', '/', 'f', 'd', '/', 0 };
const wchar_t S_dirs[] =  { 'd','i','r','s', 0 };
const wchar_t S_echo[] = {'e','c','h','o', 0};
const wchar_t S_else[] =  { 'e','l','s','e', 0 };
const wchar_t S_end[] =  { 'e','n','d', 0 };
const wchar_t S_endif[] =  { 'e','n','d','i','f', 0 };
const wchar_t S_endsw[] =  { 'e','n','d','s','w', 0 };
const wchar_t S_erwxfdzo[] = {'e', 'r', 'w', 'x', 'f', 'd', 'z', 'o', 0}; /* erwxfdzo */
const wchar_t S_eval[] =  { 'e','v','a','l', 0 };
const wchar_t S_exec[] =  { 'e','x','e','c', 0 };
const wchar_t S_exit[] =  { 'e','x','i','t', 0 };
const wchar_t S_fg[] =  { 'f','g', 0 };
const wchar_t S_fignore[] = {'f','i','g','n','o','r','e',0};
const wchar_t S_filec[] = {'f','i','l','e','c',0};/*filec*/
const wchar_t S_filesize[] = {'f','i','l','e','s','i','z','e',0};/*"filesize"*/
const wchar_t S_foreach[] =  { 'f','o','r','e','a','c','h', 0 };
const wchar_t S_gd[] =  { 'g','d', 0 };
const wchar_t S_glob[] =  { 'g','l','o','b', 0 };
const wchar_t S_goto[] =  { 'g','o','t','o', 0 };
const wchar_t S_h[] = {'-','h',0};
const wchar_t S_hardpaths[]={'h', 'a', 'r', 'd', 'p', 'a', 't', 'h', 's', 0};
const wchar_t S_hashstat[] =  { 'h','a','s','h','s','t','a','t', 0 };
const wchar_t S_histchars[] = {'h','i','s','t','c','h','a','r','s',0}; /*histchars*/
const wchar_t S_history[] = {'h','i','s','t','o','r','y',0};
const wchar_t S_home[]={'h', 'o', 'm', 'e', 0};
const wchar_t S_hours[] = {'h','o','u','r','s',0};/*"hours"*/
const wchar_t S_htrqxe[]={'h', 't', 'r', 'q', 'x', 'e', 0};
const wchar_t S_if[] =  { 'i','f', 0 };
const wchar_t S_ignoreeof[] = {'i','g','n','o','r','e','e','o','f',0};	/*"ignoreeof"*/
const wchar_t S_jobs[] = {'j','o','b','s', 0};
const wchar_t S_kbytes[] = {'k','b','y','t','e','s',0};/*"kbytes"*/
const wchar_t S_kill[] =  { 'k','i','l','l', 0 };
const wchar_t S_label[] =  { 'l','a','b','e','l', 0 };
const wchar_t S_limit[] =  { 'l','i','m','i','t', 0 };
const wchar_t S_login[] =  { 'l','o','g','i','n', 0 };
const wchar_t S_logout[] =  { 'l','o','g','o','u','t', 0 };
const wchar_t S_mail[] = {'m','a','i','l', 0};
const wchar_t S_megabytes[] = {'m','e','g','a','b','y','t','e','s',0};/*"megabytes"*/
const wchar_t S_memorysize[] = {'m','e','m','o','r','y','u','s','e',0};/*"memoryuse"*/
const wchar_t S_minutes[]={'m','i','n','u','t','e','s',0};/*"minutes"*/
const wchar_t S_n[] = {'-','n',0};/*"-n"*/
const wchar_t S_newgrp[] =  { 'n','e','w','g','r','p', 0 };
const wchar_t S_nice[] =  { 'n','i','c','e', 0 };
const wchar_t S_nobeep[] = {'n', 'o', 'b', 'e', 'e', 'p', 0};
const wchar_t S_noclobber[] = {'n','o','c','l','o','b','b','e','r',0};/*noclobber*/
const wchar_t S_noglob[] = {'n', 'o', 'g', 'l', 'o', 'b', 0}; /*noglob */
const wchar_t S_nohup[] = {'n', 'o', 'h', 'u', 'p', 0};	/*nohup */
const wchar_t S_nonomatch[] = {'n', 'o', 'n', 'o', 'm', 'a', 't', 'c', 'h', 0}; /*nonomatch */
const wchar_t S_notify[] = {'n', 'o', 't', 'i', 'f', 'y', 0};	/*nofify */
const wchar_t S_onintr[] =  { 'o','n','i','n','t','r', 0 };
const wchar_t S_path[] = {'p','a','t','h', 0}; /*path*/
const wchar_t S_popd[] =  { 'p','o','p','d', 0 };
const wchar_t S_prompt[] = {'p','r','o','m','p','t', 0};
const wchar_t S_pushd[] =  { 'p','u','s','h','d', 0 };
const wchar_t S_rd[] =  { 'r','d', 0 };
const wchar_t S_rehash[] =  { 'r','e','h','a','s','h', 0 };
const wchar_t S_repeat[] =  { 'r','e','p','e','a','t', 0 };
const wchar_t S_savehist[] = {'s','a','v','e','h','i','s','t', 0};
const wchar_t S_seconds[] = {'s','e','c','o','n','d','s',0};/*"seconds"*/
const wchar_t S_set[] =  { 's','e','t', 0 };
const wchar_t S_setenv[] =  { 's','e','t','e','n','v', 0 };
const wchar_t S_shell[] = {'s','h','e','l','l', 0};
const wchar_t S_shift[] =  { 's','h','i','f','t', 0 };
const wchar_t S_source[] = {'s','o','u','r','c','e',0};
const wchar_t S_stacksize[] = {'s','t','a','c','k','s','i','z','e',0};/*"stacksize"*/
const wchar_t S_status[]={'s', 't', 'a', 't', 'u', 's', 0};
const wchar_t S_stop[] =  { 's','t','o','p', 0 };
const wchar_t S_suspend[] =  { 's','u','s','p','e','n','d', 0 };
const wchar_t S_switch[] =  { 's','w','i','t','c','h', 0 };
const wchar_t S_term[] = {'t','e','r','m', 0};
const wchar_t S_then[] = {'t','h','e','n',0}; /*"then"*/
const wchar_t S_time[] = {'t', 'i', 'm', 'e', 0};	/*time*/
const wchar_t S_tmpshell[] = {'/','t','m','p','/','s','h', 0};
const wchar_t S_threads[] = {'t','h','r','e','a','d','s',0};
const wchar_t S_umask[] =  { 'u','m','a','s','k', 0 };
const wchar_t S_unalias[] =  { 'u','n','a','l','i','a','s', 0 };
const wchar_t S_unhash[] =  { 'u','n','h','a','s','h', 0 };
const wchar_t S_unlimit[] =  { 'u','n','l','i','m','i','t', 0 };
const wchar_t S_unlimited[] = {'u','n','l','i','m','i','t','e','d',0};/*"unlimited"*/
const wchar_t S_unset[] =  { 'u','n','s','e','t', 0 };
const wchar_t S_unsetenv[] =  { 'u','n','s','e','t','e','n','v', 0 };
const wchar_t S_user[] = {'u','s','e','r', 0};
const wchar_t S_usrbin[] = {'/','u','s','r','/','b','i','n',0};
const wchar_t S_usrucb[] = {'/','u','s','r','/','u','c','b',0};
const wchar_t S_verbose[] = {'v','e','r','b','o','s','e', 0};
const wchar_t S_vmemoryuse[] = {'v','m','e','m','o','r','y','u','s','e',0};
const wchar_t S_wait[] =  { 'w','a','i','t', 0 };
const wchar_t S_while[] =  { 'w','h','i','l','e', 0 };
const wchar_t S_etc[] = {'/','e','t','c', 0};
const wchar_t S_cshrc[] = {'/','c','s','h','r', 'c', 0};
const wchar_t S_DOTCSHRC[] = {'/','c','s','h','.','c','s','h','r', 'c', 0};
const wchar_t S_usrsbin[] = {'/','u','s','r','/','s','b','i','n', 0};
const wchar_t S_usrbsd[] = {'/','u','s','r','/','b','s','d',0};
const wchar_t S_erwxfdzocbpugkstl[] = {'e', 'r', 'w', 'x', 'f', 'd', 'z', 'o', 'c', 'b', 'p', 'u', 'g', 'k', 's', 't', 'l', 0}; /* erwxfdzo */
const wchar_t S_RELPAR[] = {'<', '>', '(', ')', 0};
const wchar_t S_DOLLESS[] = { '$', '<' };
