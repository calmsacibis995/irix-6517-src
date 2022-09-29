/* simLib.h - miscellaneous simulator routines header */

/*
modification history
--------------------
02c,14dec93,gae  removed u_printf() declaration (u_Lib.c) conflict for non stdc.
02b,14jul93,gae  fixed banner.
02a,09jan93,gae  renamed from sysULib.h; discarded major portions.
01c,05aug92,gae  added non-ANSI forward declarations.
01b,29jul92,gae  added more prototypes; ANSIfied.
01a,04jun92,gae  written.
*/

/*
This header contains the declarations for miscellaneous simulator routines.
*/

#ifndef	INCsimLibh
#define	INCsimLibh

/* prototypes */

#ifdef __STDC__

#if	0
extern int u_printf (const char *fmt, ...);
#endif	/* 0 */
extern void s_userGet (char *hostname, char *hostip, char *username, char *cwd);
extern void s_fdint (int fd, int raw);
extern void s_uname (char *sysname, char *nodename, char *release, char *version, char *machine);

#else

#if	0
extern int u_printf (/* const char *fmt, ... */);
#endif	/* 0 */
extern void s_userGet (/* char *hostname, char *hostip, char *username, char *cwd */);
extern void s_fdint (/* int fd, int raw */);
extern void s_uname (/* char *sysname, char *nodename, char *release, char *version, char *machine */);

#endif	/* __STDC__ */

#endif	/* INCsimLibh */
