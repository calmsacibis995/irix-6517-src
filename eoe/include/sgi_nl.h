#ifndef	__SGI_NL_H__
#define	__SGI_NL_H__
#ifdef	__cplusplus
extern	"C" {
#endif
#ident	"$Revision: 1.6 $"
/*
 * @[$] sgi_nl.h 1.0 frank@ceres.esd.sgi.com Oct 29 1992
 * SGI specific include file for international support
 */

/*
 * _sgi_nl_error
 */
extern	void _sgi_nl_error(int, char *, char *, ...);

#define	SGINL_NOSYSERR	0	/* don't add system error message */
#define	SGINL_SYSERR	1	/* add system error message */
#define	SGINL_SYSERR2	2	/* add system error message at the same line */

/*
 * _sgi_nl_usage
 */
extern	void _sgi_nl_usage(int, char *, char *, ...);

#define	SGINL_USAGESPC	0	/* only space instead of Usage message */
#define	SGINL_USAGE	1	/* print usage message */

/*
 * _sgi_nl_query and _sgi_nl_query_fd
 */
extern	int  _sgi_nl_query(FILE *,char *,int,char *,char *,char *,char *);
extern	int  _sgi_nl_query_fd(FILE *,char *,int,char *,char *,char *,char *,FILE *);

#define	SGINL_DEFNO	0	/* default is no */
#define	SGINL_DEFYES	1	/* default is yes */

/*
 * formatted messages
 */
extern	char *_sgi_dofmt(char *, int, char *, int, char *, va_list);
extern	char *_sgi_sfmtmsg(char *, int, char *, int, char *, ...);
extern	int _sgi_ffmtmsg(FILE *, int, char *, int, char *, ...);

/*
 * catalogue stuff from pfmt.h. pfmt.h is obsolete and its defines
 * conflict with fmtmsg.h which we like to use.
 * So we copy the few prototypes we need over here
 */
const char *setcat(const char *);
int setlabel(const char *);

#ifdef	__cplusplus
}
#endif
#endif	/* ! __SGI_NL_H__ */
