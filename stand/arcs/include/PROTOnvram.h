/* NVRAM proto routines declarations */

#ident "$Revision: 1.2 $"
#ifndef	_PROTOnvram_h_
#define	_PROTOnvram_h_

/* Define NVRAM_FUNC *before* including this file.
 * See lib/libsk/ml/IP22nvram.c for an example.
 *
 * Note many of these are also be defined in include/libsk.h
 */

NVRAM_FUNC(char *,	cpu_get_nvram_offset, (int off,int len), (int,int),
				(off,len), return)

NVRAM_FUNC(void,	cpu_get_nvram_buf,
				(int off,int len,char buf[]), (int,int,char buf[]),
				(off,len,buf),)

NVRAM_FUNC(int,		cpu_set_nvram_offset,
				(int off,int len,char *string), (int,int,char *),
				(off,len,string), return)

NVRAM_FUNC(int,		cpu_set_nvram,
				(char *match,char *newstr), (char *,char *),
				(match,newstr), return)

NVRAM_FUNC(int,		cpu_is_nvvalid, (void), (void), (), return)

NVRAM_FUNC(void,	cpu_set_nvvalid,
				(void (*delstr)(char *str,struct string_list *),
				struct string_list *env),
				(void (*)(char *,struct string_list *),
				struct string_list *),
				(delstr, env),)

NVRAM_FUNC(void,	cpu_nv_lock, (int lock), (int), (lock),)

NVRAM_FUNC(void,	cpu_get_eaddr, (u_char eaddr[]), (u_char eaddr[]), (eaddr),)

NVRAM_FUNC(int,		nvram_is_protected, (void), (void), (), return)

NVRAM_FUNC(void,	cpu_get_nvram_persist,
				(int (*putstr)(char *,char *,struct string_list *), struct string_list * env),
				(int (*)(char *,char *,struct string_list *),struct string_list *),
				(putstr, env),)

NVRAM_FUNC(void,	cpu_set_nvram_persist,
				(char *match, char *newstring), (char *, char *),
				(match, newstring),)

NVRAM_FUNC(void,	cpu_nvram_init, (void), (void), (),) 

#endif /* _PROTOnvram.h_ */
