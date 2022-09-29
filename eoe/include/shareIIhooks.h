/* IDENT-H */
/*
 *	$Id: shareIIhooks.h,v 1.3 1998/01/10 02:39:55 ack Exp $	(Softway:Share II)	
 *
 *	Copyright (C) 1996-1997 Softway Pty Ltd, Sydney Australia.
 *	All Rights Reserved.
 *
 *	This is unpublished proprietary source code of Softway Pty Ltd.
 *	The contents of this file are protected by copyright laws and
 *	international copyright treaties, as well as other intellectual
 *	property laws and treaties.  These contents may not be extracted,
 *	copied,	modified or redistributed either as a whole or part thereof
 *	in any form, and may not be used directly in, or to assist with,
 *	the creation of derivative works of any nature without prior
 *	written permission from Softway Pty Ltd.  The above copyright notice
 *	does not evidence any actual or intended publication of this file.
 */
#ident	"$Revision: 1.3 $"
/*
 *	shareIIhooks.h
 *
 *	Share System Utilities Hooks header.
 */

#ifndef	__SHAREIIHOOKS__
#define	__SHAREIIHOOKS__

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * shareIIhooks.h provides definitions of the names and types of the
 *	Share System Utilities Hook Functions. This file exists to provide
 *	some degree of type-safety between callers and the Share library
 *	implementation of this part of the Share API. It is only needed when
 *	compiling modules that make hook calls and in the construction of
 *	the parts of the Share library which implement these functions.
 *	The usual technique of simply giving declarations of the functions
 *	is not applicable since we want the callers to link and be runnable
 *	without even stub versions of the hook functions.
 *
 *	A typical hook call looks like this:
 *
 *		SH_DECLARE_HOOK(SETMYNAME);
 *		SH_DECLARE_HOOK(XXX);
 *
 *		...
 *
 *		if (sgidladd(SH_LIMITS_LIB, RTLD_LAZY))
 *		{
 *			static const char *Myname = "progname";
 *			SH_HOOK_SETMYNAME(Myname);
 *			SH_HOOK_XXX(XXXARGS...);
 *		}
 *
 *	Use of the SETMYNAME hook is optional but recommended. It should be
 *	the first hook called and need only be called once. The string pointer
 *	passed must be valid for the duration of all subsequent hook calls;
 *	the given string should be the program name which the Share library
 *	may use in constructing error messages.
 *
 *	Generally, if a sgidladd() fails then the action should be to
 *	fall-back to the normal behaviour.
 */

/*
 * WARNING: SH_LIMITS_LIB must be an absolute path because setuid-root
 *	programs will access it as illustrated above.
 */


#define	SH_LIMITS_LIB	"/usr/lib/liblim.so"


/*
 * SH_DECLARE_HOOK must be used at the top of any source file where that hook
 * is to be used so as to ensure proper type checking.  For example:
 *
 *	SH_DECLARE_HOOK(ISACTIVE);
 *
 * Then you can use the hook directly, for example:
 *
 *	if (SH_HOOK_ISACTIVE(uid)) ...
 *
 * NEVER use the actual hook function name directly - it may change in
 * the future.  Instead, always use the macro "SH_HOOK_..." as above.
 */
#define	SH_DECLARE_HOOK(hook)		extern SH_HOOK_##hook##_D(SH_HOOK_##hook)

/*
 * Hooks and data for passmgmt and/or useradd, userdel, usermod.
 *	SH_SYM_SHMYUID is initialised by SH_HOOK_ISUSERADMIN.
 */

#define SH_HOOK_ISUSERADMIN		SHisuseradmin
#define SH_HOOK_ISUSERADMIN_STR		"SHisuseradmin"
#define SH_HOOK_ISUSERADMIN_D(hook)	int	(hook)(void)

#define	SH_SYM_SHMYUID			SHmyuid
#define	SH_SYM_SHMYUID_STR	       	"SHmyuid"
#define	SH_SYM_SHMYUID_D(sym)		uid_t	(sym)

#define SH_HOOK_ENTERPASSWD		SHenterpasswd
#define SH_HOOK_ENTERPASSWD_STR		"SHenterpasswd"
#define SH_HOOK_ENTERPASSWD_D(hook)	void	(hook)(const struct passwd *)

#define SH_HOOK_ADDUSER			SHadduser
#define SH_HOOK_ADDUSER_STR		"SHadduser"
#define SH_HOOK_ADDUSER_D(hook)		int	(hook)(uid_t, gid_t)

#define SH_HOOK_MODUSER			SHmoduser
#define SH_HOOK_MODUSER_STR	       	"SHmoduser"
#define SH_HOOK_MODUSER_D(hook)		int	(hook)(uid_t, uid_t, gid_t, gid_t)

#define SH_HOOK_DELUSER			SHdeluser
#define SH_HOOK_DELUSER_STR	       	"SHdeluser"
#define SH_HOOK_DELUSER_D(hook)		int	(hook)(uid_t)

#define	SH_HOOK_PWPERM			SHpwperm
#define	SH_HOOK_PWPERM_STR	       	"SHpwperm"
#define	SH_HOOK_PWPERM_D(hook)		int	(hook)(uid_t, uid_t)

#define SH_HOOK_BACKOUTUSER		SHbackoutuser
#define SH_HOOK_BACKOUTUSER_STR		"SHbackoutuser"
#define SH_HOOK_BACKOUTUSER_D(hook)	int	(hook)(void)

#define SH_HOOK_COMMITUSER		SHcommituser
#define SH_HOOK_COMMITUSER_STR		"SHcommituser"
#define SH_HOOK_COMMITUSER_D(hook)	int	(hook)(void)

/*
 * Hooks for useradd, usermod, userdel and passmgmt.
 */

#define	SH_HOOK_LOCKLNODE		SHlocklnode
#define	SH_HOOK_LOCKLNODE_STR		"SHlocklnode"
#define	SH_HOOK_LOCKLNODE_D(hook)	int	(hook)(uid_t)

#define	SH_HOOK_UNLOCKLNODE		SHunlocklnode
#define	SH_HOOK_UNLOCKLNODE_STR		"SHunlocklnode"
#define	SH_HOOK_UNLOCKLNODE_D(hook)	int	(hook)(void)

#define	SH_HOOK_ISACTIVE_LOCKLNODE		SHisactive_locklnode
#define	SH_HOOK_ISACTIVE_LOCKLNODE_STR		"SHisactive_locklnode"
#define	SH_HOOK_ISACTIVE_LOCKLNODE_D(hook)	int	(hook)(uid_t)

#define	SH_HOOK_ISACTIVE		SHisactive
#define	SH_HOOK_ISACTIVE_STR		"SHisactive"
#define	SH_HOOK_ISACTIVE_D(hook)	int	(hook)(uid_t)

/*
 * Hooks for login and login-like system access.
 */

#define SH_HOOK_LOGIN			SHlogin
#define SH_HOOK_LOGIN_STR	       	"SHlogin"
#define SH_HOOK_LOGIN_D(hook)		int	(hook)(uid_t, const char *)

#define SH_HOOK_REXEC			SHrexec
#define SH_HOOK_REXEC_STR	       	"SHrexec"
#define SH_HOOK_REXEC_D(hook)		int	(hook)(uid_t)

#define SH_HOOK_RSH			SHrsh
#define SH_HOOK_RSH_STR			"SHrsh"
#define SH_HOOK_RSH_D(hook)		int	(hook)(uid_t)

#define SH_HOOK_FTP			SHftp
#define SH_HOOK_FTP_STR			"SHftp"
#define SH_HOOK_FTP_D(hook)		int	(hook)(uid_t)

/*
 * Hook for passwd.
 */

#define	SH_HOOK_ISADMINOK		SHisadminok
#define	SH_HOOK_ISADMINOK_STR		"SHisadminok"
#define	SH_HOOK_ISADMINOK_D(hook)	int	(hook)(uid_t)

/*
 * Hooks for su(1M)
 */

#define SH_HOOK_SU			SHsu
#define SH_HOOK_SU_STR			"SHsu"
#define SH_HOOK_SU_D(hook)		int	(hook)(uid_t, int)

#define SH_HOOK_SU0			SHsu0
#define SH_HOOK_SU0_STR			"SHsu0"
#define SH_HOOK_SU0_D(hook)		void	(hook)(void)

#define SH_HOOK_SUMSG			SHsumsg
#define SH_HOOK_SUMSG_STR		"SHsumsg"
#define SH_HOOK_SUMSG_D(hook)		char *(hook)(void)
/*
 * Hook for establishing program name.
 */
#define SH_HOOK_SETMYNAME		SHSetMyname
#define SH_HOOK_SETMYNAME_STR		"SHSetMyname"
#define SH_HOOK_SETMYNAME_D(hook)	void	(hook)(const char *)

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* __SHAREIIHOOKS__ */
