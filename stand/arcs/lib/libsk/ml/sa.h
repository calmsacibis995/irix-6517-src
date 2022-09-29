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
 * stand-alone diag functions for settops
 */

#ifndef _SA_H
#define _SA_H

/*
 * the variable structure
 */
struct SaVar {
	char	*name;			/* name of variable */
	int	*val;			/* value of variable */
	void	(*func)(struct SaVar *, int);/* fct for variable handling */
	int	arg;			/* a general argument */
};

/*
 * command structure
 */
struct SaCmd {
	uint	flags;			/* flags */
	char	*cmd;			/* cmd string */
	int	(*func)(int, char **);	/* function to handle command */
};

/* flags */

#define	SCF_PROBE	0x80000000	/* call probe before handler */

/*
 * flags structure
 */
struct SaFlag {
	unchar	*name;			/* name of flag */
	uint	bit;			/* flag bit */
	unchar	*help;			/* help string */
	void	(*func)(struct SaFlag *, int);	/* function for flag */
};

/*
 * support functions
 */
#if defined(SFLASH) || defined(IP30)
extern	void show_bytes(char *ptr, int count);
#endif /* SFLASH || IP30 */
extern	void sa_prbytes(char *str, u_int addr, u_char *p, int nb);
extern	int sa_chkarg(char *p);
extern	int sa_nargs(char *cmd);
extern	int sa_illarg(const char *cmd, char *s);
extern	int sa_illcmd(const char *cmd, char *str);
extern	struct SaVar *sa_findvar(struct SaVar *, char *);
extern	void sa_getvars(struct SaVar *, char **, const char *);
extern	int sa_listvars(struct SaVar *, char *);
extern	int sa_cmd(int, char **, struct SaCmd *, const char *, int (*)(void));
extern	int sa_usage(const char *, char **strp);
extern	int sa_prstr(char **strp);
extern	int sa_flag(int, char **, const char *, struct SaFlag *, uint *);
extern	int sa_fusage(const char *);
extern	int sa_vusage(const char *);

/* flags */

#endif /* _SA_H */

#endif /* SA_DIAG */
