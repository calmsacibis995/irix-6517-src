/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1990 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
#ident "$Revision: 1.9 $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T		*/
/*	All Rights Reserved					*/
/*								*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <stdio.h>
#include <sys/types.h> 
#include <shadow.h>
#include <pwd.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef _SHAREII
#include <dlfcn.h>
#include <shareIIhooks.h>
SH_DECLARE_HOOK(SETMYNAME);
SH_DECLARE_HOOK(ISUSERADMIN);
SH_DECLARE_HOOK(BACKOUTUSER);
SH_DECLARE_HOOK(ENTERPASSWD);
SH_DECLARE_HOOK(ADDUSER);
SH_DECLARE_HOOK(MODUSER);
SH_DECLARE_HOOK(DELUSER);
SH_DECLARE_HOOK(COMMITUSER);
static int SHshareRunning = 0;
#endif /* _SHAREII */

#define CMT_SIZE	(128+1)	/* Argument sizes + 1 (for '\0') */
#define DIR_SIZE	(256+1)
#define SHL_SIZE	(256+1)
#define ENTRY_LENGTH	BUFSIZ	/* Max length of an /etc/passwd entry */
#define UID_MIN		100	/* Lower bound of default UID */

#define M_MASK		01	/* Masks for the optn_mask variable */
#define L_MASK		02	/* It keeps track of which options   */
#define C_MASK		04	/* have been entered		     */
#define H_MASK		010
#define U_MASK		020
#define G_MASK		040
#define S_MASK		0100
#define O_MASK		0200
#define A_MASK		0400
#define D_MASK		01000
#define F_MASK		02000
#if DO_PWEXPIRE
#define E_MASK		04000
#endif

/* #define DEBUG	1 */
				/* flags for info_mask */
#define LOGNAME_EXIST	01	/* logname exists */
#define BOTH_FILES	02	/* touch both password files */
#define WRITE_P_ENTRY	04	/* write out password entry */
#define WRITE_S_ENTRY	010	/* write out shadow entry */
#define NEED_DEF_UID	020	/* need default uid */
#define FOUND		040	/* found the entry in password file */
#define LOCKED		0100	/* did we lock the password file */
#define SHAD_FLAG	0200	/* running with shadow file flag */

char defdir[] = "/usr/people/";	/* default home directory for new user */
char defshell[] = "/bin/sh";	/* default shell of new entries */
char pwdflr[] =	"x" ;		/* password string for /etc/passwd */
char lkstring[] = "*LK*" ;	/* lock string for shadow password */
char nullstr[] = "" ;		/* null string */

#if DO_PWEXPIRE	/* this file used only by -e (passwd expiration opt) code */
#define DATMSK "DATEMSK=/etc/datemsk"
char getdate_msg[9][80] = {
"? no error",
"environment variable DATEMSK null/undefined",
"cannot open DATEMSK template file",
"bad status for DATEMSK template file",
"DATEMSK template file is not a regular file",
"I/O error reading DATEMSK template file",
"memory allocation failure while processing DATEMSK template file",
"no match for input string in DATEMSK template file",
"invalid time/date input"
};
#endif

/* Declare all functions that do not return integers.  This is here
   to get rid of some lint messages */

void	bad_perm(void), bad_usage(char *), bad_arg(char *),
	bad_uid(void), bad_pasf(void), file_error(void), bad_news(void), 
	no_lock(void), add_uid(uid_t), rid_tmpf(void), ck_p_sz(struct passwd *),
	ck_s_sz(struct spwd *), bad_name(char *) ;
int	rec_pwd(void);

#define _SAFESTR(_SPTR)	(_SPTR ? _SPTR : "<NULL>")

/* SGI: config file to see if system uses NIS */
#define YP_CONFIG_FILE "/etc/config/yp"
static int get_flag_state();
#define LINE_SIZE 80

#if DEBUG
static void	dump_pwent(struct passwd *);
static void	dump_spwent(struct spwd *);
/* SGI added these to allow unprivileged debugging */
static int xxlckpwdf();
static void xxulckpwdf();
#define lckpwdf() xxlckpwdf()
#define ulckpwdf() xxulckpwdf()
#undef PASSWD
#define PASSWD "/usr/tmp/passwd"
#undef SHADOW
#define SHADOW "/usr/tmp/shadow"
#undef OPASSWD
#define OPASSWD         "/usr/tmp/opasswd"
#undef OSHADOW
#define OSHADOW         "/usr/tmp/oshadow"
#undef PASSTEMP
#define PASSTEMP        "/usr/tmp/ptmp"
#undef SHADTEMP
#define SHADTEMP        "/usr/tmp/stmp"
#endif

static FILE *fp_ptemp, *fp_stemp ;

/* The uid_blk structure is used in the search for the default
   uid.  Each uid_blk represent a range of uid(s) that are currently
   used on the system. */

struct uid_blk	  { 			
		  struct uid_blk *link ;
		  uid_t low ;		/* low bound for this uid block */
		  uid_t high ; 		/* high bound for this uid block */
		  } ;

/* more forward decls, moved from above */
void	uid_bcom(struct uid_blk *), add_ublk(uid_t, struct uid_blk *); 
struct uid_blk *uid_sp ;
char *prognamp ;			/* program name */
extern int errno ;
int optn_mask = 0, info_mask = 0 ;
extern int getdate_err;
int shadow_exist = 0;
/* SGI IRIX 5.1 getpwent merges in NIS transparently, i.e. you get
 * the NIS yppasswd map entries back from getpwent.  This behavior
 * is turned off by setting _getpwent_no_yp to 1, which is what 
 * passmgmt needs.
 */
extern int _getpwent_no_yp;

/* SGI IRIX 5.1 getpwent handles shadow passwd file transparently for
 * privileged users, i.e. getpwent returns encrypted pw if euid==0.
 * This behavior is turned off by setting _getpwent_no_shadow to 1,
 * which is what passmgmt needs.
 */
extern int _getpwent_no_shadow;

/* SGI: place to save +::... entry to preserve it at the end of the file */
struct passwd savedpwent;

/* SGI IRIX 6.2 getpwent merges in SSDI, which allows custom implementations
 * of passwd database (and others as well). With SSDI, getpwent will
 * return entries from these custom implemetations, whereas passmt
 * only needs stuff from the "files". Setting _getpwent_no_ssdi to 1
 * turns off this behavior which is what passmgmt needs.
 */
extern int _getpwent_no_ssdi;

main (argc, argv)
int	argc ;
char  **argv ;
{
	int c, i ;
	char *lognamp, *char_p ;
	char *shadownamep;	/* SGI: lognamep less the leading + for YP*/
	int end_of_file = 0;
	int error;
	long date = 0;

	extern char *optarg;
	extern int optind ;
	extern struct tm *getdate();

	struct passwd *pw_ptr1p, passwd_st ;
	struct spwd *sp_ptr1p, shadow_st ;
	struct stat statbuf ;
	int ypflag = 0;
	int savedflag = 0;	/* SGI: used for +::... entry */
	struct tm *tm_ptr;
#ifdef _SHAREII
	uid_t SHoriguid;
	uid_t SHoriggid;
	static const char *Myname = "passmgmt";
#endif /* _SHAREII */
#ifdef DEBUG
	struct uid_blk *uid_ptr;
#endif

	tzset();
	/* Get program name */
	prognamp = argv[0] ;

#ifndef DEBUG
#ifdef _SHAREII
	/*
	 *
	 * The original identity check requires that the
	 * invoker's real UID == 0, i.e. the invoker is really the
	 * super-user.
	 *
	 * Share allows users other than the super-user to alter the
	 * password file, subject to careful restrictions.  At this point
	 * we check the invoker's privilege.  The function returns 1 if
	 * it is okay, 0 if the user is not allowed to add/mod/del users.
	 */
	if (sgidladd(SH_LIMITS_LIB, RTLD_LAZY))
	{
		SHshareRunning = 1;
		SH_HOOK_SETMYNAME(Myname);

		if (SH_HOOK_ISUSERADMIN() == 0)
			bad_perm();
	}
	else
	{
		/* Check identity */
		if ( getuid () != 0 )
			bad_perm () ;
	}
#else
	/* Check identity */
	if ( getuid () != 0 )
		bad_perm () ;
#endif /* _SHAREII */
#endif

	/* SGI: turn off NIS mapping for getpwent */
	_getpwent_no_yp = 1;
	/* SGI: turn off automatic fill from shadow */
	_getpwent_no_shadow = 1;
	/* SGI: turn off SSDI mapping for getpwent */
	_getpwent_no_ssdi = 1;

	/* Check for the existence of shadow password file */
	if ( !access (SHADOW,0) )		/* if shadow is there */
	{
		shadow_exist ++;
		info_mask |= SHAD_FLAG ;
	}


	/* Lock the password file(s) */

	if ( lckpwdf() != 0 )
		no_lock () ;
	info_mask |= LOCKED ;		/* remember we locked */

	/* initialize the two structures */

	passwd_st.pw_name = nullstr ;		/* login name */
	if ( SHAD_FLAG & info_mask )		/* if shadow is running */
		passwd_st.pw_passwd = pwdflr ;	/* bogus password */
	else
		passwd_st.pw_passwd = lkstring; /* bogus password */
	passwd_st.pw_uid = -1 ;			/* no uid */
	passwd_st.pw_gid = 1 ;			/* default gid */
	passwd_st.pw_age = nullstr ;		/* no aging info. */
	passwd_st.pw_comment = nullstr ;	/* no comments */
	passwd_st.pw_gecos = nullstr ;		/* no comments */
	passwd_st.pw_dir = nullstr ;		/* no default directory */

	/* 
	 * XXX: 5.1 doesn't work correctly with a NULL /etc/passwd
	 * shell field, so force passmgmt to specify /bin/sh when
	 * creating new entries, rather than defaulting to NULL.
	 */
	passwd_st.pw_shell = defshell ;		/* explicit /bin/sh */

	shadow_st.sp_namp = nullstr ; 	/* no name */
	shadow_st.sp_pwdp = lkstring ; 	/* locked password */
	shadow_st.sp_lstchg = -1 ; 	/* no lastchanged date */
	shadow_st.sp_min = -1 ; 	/* no min */
	shadow_st.sp_max = -1 ; 	/* no max */
	shadow_st.sp_warn = -1 ; 	/* no warn */
	shadow_st.sp_inact = -1 ; 	/* no inactive */
	shadow_st.sp_expire = -1 ; 	/* no expire */
	shadow_st.sp_flag = 0 ; 	/* no flag */

	/* parse the command line */

#if DO_PWEXPIRE	/* accept -e (passwd expiration) option */
	while ( (c = getopt (argc, argv, "ml:c:h:u:g:s:f:e:k:oad")) != -1 )
#else
	while ( (c = getopt (argc, argv, "ml:c:h:u:g:s:f:k:oad")) != -1 )
#endif

		switch (c) 
		   {

		   case 'm' :
			    /* Modify */

			    if ( (A_MASK|D_MASK|M_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

			    optn_mask |= M_MASK ;
			    break ;

		   case 'l' :
			    /* Change logname */

			    if ( (A_MASK|D_MASK|L_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

			    if ( strpbrk ( optarg, ":\n" ) ||
				 strlen (optarg) == 0  )
			       	bad_arg ("Invalid argument to option -l") ;

			    optn_mask |= L_MASK ;
			    passwd_st.pw_name = optarg ;
			    shadow_st.sp_namp = optarg ;
			    break ;

		   case 'f' :
			    /* set inactive */
			    
			    if ( (D_MASK|F_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;
			    if (((shadow_st.sp_inact = strtol (optarg,
				&char_p,10)) < 0) || (*char_p != '\0') 
				|| strlen (optarg) < 0) 
				bad_arg ("Invalid argument to option -f") ;
			    if (shadow_st.sp_inact == 0 )
				shadow_st.sp_inact = -1;
			    optn_mask |= F_MASK;
			    break;

#if DO_PWEXPIRE
		   case 'e' :
			    /* set expire date */

			    if ( (D_MASK|E_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

		            if ( (strlen(optarg)) <2 ) 
				shadow_st.sp_expire = -1;
			    else {
			     	if (getenv("DATEMSK") == NULL)
				    putenv(DATMSK);
   			     	if ( (tm_ptr =  getdate(optarg)) == NULL) 
				{
				    if( getdate_err < 0 || getdate_err > 8)
					bad_arg("getdate internal error for -e opt");
				    else {
					fprintf(stderr,"(%d) %s ",getdate_err,getdate_msg[getdate_err]);
					bad_arg("Invalid argument to -e opt");
				    }
				}
                               	if ((date =  mktime(tm_ptr)) < 0) 
					bad_arg ("Invalid argument to -e opt");
             		        if ((shadow_st.sp_expire = (date/DAY))<=DAY_NOW)
					bad_arg ("Invalid argument to -e opt, date must be > today");
			    }

			     optn_mask |= E_MASK;
			     break;
#endif	/* DO_PWEXPIRE */
			    
		   case 'c' :
			    /* The comment */

			    if ( (D_MASK|C_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options");

			    if ( strlen (optarg) > CMT_SIZE ||
				 strpbrk ( optarg, ":\n" ) )
				bad_arg ("Invalid argument to option -c") ;

			    optn_mask |= C_MASK ;
			    passwd_st.pw_comment = optarg ;
			    passwd_st.pw_gecos = optarg ;
			    break ;

		   case 'h' :
			    /* The home directory */

			    if ( (D_MASK|H_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

			    if ( strlen (optarg) > DIR_SIZE ||
				 strpbrk ( optarg, ":\n" ) )
				bad_arg ("Invalid argument to option -h") ;
			
			    optn_mask |= H_MASK ;
			    passwd_st.pw_dir = optarg ;
			    break ;

		   case 'u' :
			    /* The uid */

			    if ( (D_MASK|U_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

			    optn_mask |= U_MASK ;
			    passwd_st.pw_uid = (uid_t)strtol(optarg,&char_p,10);
			    if ( (*char_p != '\0') ||
			         ( passwd_st.pw_uid < 0 ) ||  
				 ( strlen (optarg) == 0 ) )
				bad_arg ("Invalid argument to option -u") ;

			    break ;

		   case 'g' :
			    /* The gid */

			    if ( (D_MASK|G_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options");

			    optn_mask |= G_MASK ;
			    passwd_st.pw_gid = (gid_t)strtol(optarg,&char_p,10);

			    if ( (*char_p != '\0') ||
			         ( passwd_st.pw_gid < 0 ) ||
				 ( strlen (optarg) == 0 ) )
				bad_arg ("Invalid argument to option -g") ;
			    break ;

		   case 's' :
			    /* The shell */

			    if ( (D_MASK|S_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

			    if ( strlen (optarg) > SHL_SIZE ||
				 strpbrk ( optarg, ":\n" ) )
				bad_arg ("Invalid argument to option -s") ;

			    optn_mask |= S_MASK ;
			    passwd_st.pw_shell = optarg ;
			    break ;

		   case 'o' :
			    /* Override unique uid  */

			    if ( (D_MASK|O_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

			    optn_mask |= O_MASK ;
			    break ;

		   case 'a' :
			    /* Add */

			    if ( (A_MASK|M_MASK|D_MASK|L_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

			    optn_mask |= A_MASK ;
			    break ;

		   case 'd' :
			    /* Delete */

			    if ( (D_MASK|M_MASK|L_MASK|C_MASK|
				  H_MASK|U_MASK|G_MASK|S_MASK|
				  O_MASK|A_MASK) & optn_mask )
			         bad_usage ("Invalid combination of options") ;

			    optn_mask |= D_MASK ;
			    break ;

		   case '?' :

			    bad_usage ("") ;
			    break ;
		   }

	/* check command syntax for the following errors */
	/* too few or too many arguments */
	/* no -a -m or -d option */
	/* -o without -u */
	/* -m with no other option */

	if ( optind == argc || argc > (optind+1) || 
	     !((A_MASK|M_MASK|D_MASK) & optn_mask) ||
	     ((optn_mask & O_MASK) && !(optn_mask & U_MASK)) ||
	     ((optn_mask & M_MASK) && !(optn_mask & 
#if DO_PWEXPIRE
	         (L_MASK|C_MASK|H_MASK|U_MASK|G_MASK|S_MASK|F_MASK|E_MASK))) )
#else
	         (L_MASK|C_MASK|H_MASK|U_MASK|G_MASK|S_MASK|F_MASK))) )
#endif

		bad_usage ("Invalid command syntax") ;

	/* null string argument or bad characters ? */
	if ( ( strlen ( argv[optind] ) == 0 ) ||
	       strpbrk ( argv[optind], ":\n" ) )
		bad_arg ("Invalid name") ;

	lognamp = argv [optind] ;
	if (*lognamp == '+') {
		ypflag = 1;
	        optn_mask &= ~U_MASK ;
	        optn_mask &= ~G_MASK ;
		/* SGI: these were 0, but -1 requested by CSD */
		passwd_st.pw_uid = -1 ;			
		passwd_st.pw_gid = -1 ;		
		shadownamep = lognamp+1;
	}
	else
	{
		shadownamep = lognamp;
	}

	/* if we are adding a new user or modifying an existing user
	   (not the logname), then copy logname into the two data
	   structures */

	if ( (A_MASK & optn_mask) || 
	     ((M_MASK & optn_mask) && !(optn_mask & L_MASK)) )
		{
		passwd_st.pw_name = argv [optind] ;
		shadow_st.sp_namp = argv [optind] ;
		}

	/* SGI: shadow file can't have leading + in NIS entries */
	if (*shadow_st.sp_namp == '+') shadow_st.sp_namp++;

	/* Put in directory if we are adding and we need a default */

	if ( !( optn_mask & H_MASK) && ( optn_mask & A_MASK ) ) {
		if ((passwd_st.pw_dir = (char *)malloc((size_t)DIR_SIZE))==NULL)
			file_error () ;

		*passwd_st.pw_dir = '\0' ;
		if (!ypflag) {
			(void)strcat(passwd_st.pw_dir, defdir);
			(void)strcat(passwd_st.pw_dir, lognamp);
		}
	}

	/* Check the number of password files we are touching */

	if ( (SHAD_FLAG & info_mask) && !( (M_MASK & optn_mask) && 
	    !(L_MASK & optn_mask) ) )
		info_mask |= BOTH_FILES ;

	if ((M_MASK & optn_mask) &&
#if DO_PWEXPIRE
		((E_MASK & optn_mask) || (F_MASK & optn_mask))) {
#else
		(F_MASK & optn_mask)) {
#endif
		if (!(SHAD_FLAG & info_mask))
		    bad_arg (
			"/etc/shadow file is not accessible for specified operation!"
		    );
		info_mask |= BOTH_FILES ;
	}

	/* Open the temporary file(s) with appropriate permission mask */
	/* and the appropriate owner */

	if ( stat ( PASSWD, &statbuf ) < 0 ) 
		file_error () ;

	(void) umask ( ~( statbuf.st_mode & (S_IRUSR|S_IRGRP|S_IROTH) ) ) ;

 	if ( (fp_ptemp = fopen ( PASSTEMP , "w" )) == NULL )
		file_error () ;

	if ( chown ( PASSTEMP, statbuf.st_uid, statbuf.st_gid ) != NULL )
		{
		(void) fclose ( fp_ptemp ) ;

		if ( unlink ( PASSTEMP ) )
			(void)fprintf(stderr, "%s: warning: cannot unlink %s\n",
					 prognamp, PASSTEMP ) ;

		file_error () ;
		}

	if ( info_mask & BOTH_FILES )
		{
		if ( stat ( SHADOW, &statbuf ) < 0 ) 
			{
			rid_tmpf () ;
			file_error () ;
			}

		(void) umask ( ~( statbuf.st_mode & S_IRUSR ) ) ;

	 	if ( (fp_stemp = fopen ( SHADTEMP , "w" )) == NULL )
			{
			rid_tmpf () ;
			file_error () ;
			}

		if (chown(SHADTEMP, statbuf.st_uid, statbuf.st_gid ) != NULL)
			{
			rid_tmpf () ;
			file_error () ;
			}
		} 

	/* Default uid needed ? */

	if ( !( optn_mask & U_MASK ) && ( optn_mask & A_MASK ) && (!ypflag) )
		{
		/* mark it in the information mask */
		info_mask |= NEED_DEF_UID ;

		/* create the head of the uid number list */
		if ((uid_sp = (struct uid_blk *)
				malloc((size_t)sizeof(struct uid_blk))) == NULL)
			{
			rid_tmpf () ;
			file_error () ;
			}

		uid_sp->link = NULL ;
		uid_sp->low = (UID_MIN -1) ;
		uid_sp->high = (UID_MIN -1) ;
		}
		

	error = errno = end_of_file = 0;
	/* The while loop for reading PASSWD entries */
	while (!end_of_file) {
	    if ( (pw_ptr1p = (struct passwd *) getpwent()) != NULL ) {
		info_mask |= WRITE_P_ENTRY ;
		/* 
		 * if shadow is running, we must overwrite the actual
		 * encrypted passwd--fetched from /etc/shadow--with
		 * the bogus value, since this entry will be written
		 * to tmp file that then becomes /etc/passwd.
		 */
		if (SHAD_FLAG & info_mask)
			pw_ptr1p->pw_passwd = pwdflr ;
#if DEBUG
		if (errno)
			fprintf(stderr, "!!!errno == %d: ", errno);
		fprintf(stderr, "Processing ");
		dump_pwent(pw_ptr1p);
#endif

		/* Set up the uid usage blocks to find the first
		   available uid above UID_MIN, if needed */

		if ( info_mask & NEED_DEF_UID )
			add_uid ( pw_ptr1p->pw_uid ) ;

		/* Check for unique UID */

		if ( strcmp ( lognamp, pw_ptr1p->pw_name )		 &&
		     ( pw_ptr1p->pw_uid == passwd_st.pw_uid )            &&
		     ( (optn_mask & U_MASK) && !(optn_mask & O_MASK) )  ) {
			rid_tmpf () ;		/* get rid of temp files */
			bad_uid () ;
			}
		/* Check for unique new logname */

		if (strcmp(lognamp, pw_ptr1p->pw_name) && (optn_mask&L_MASK) &&
		     !strcmp(pw_ptr1p->pw_name, passwd_st.pw_name)) {
		     rid_tmpf () ;
		     if ( (SHAD_FLAG & info_mask) &&
			    !getspnam ( pw_ptr1p->pw_name ))
			bad_pasf () ;
		     else
			bad_name ( "logname already exists" ) ;
		}

#ifdef _SHAREII
		/*
		 * Share builds a list of all the users in the password file
		 * for later checking whether the invoker has permission
		 * to be adding/modifying/deleting the user in question.
		 */
		if (SHshareRunning)
			SH_HOOK_ENTERPASSWD(pw_ptr1p);
#endif /* _SHAREII */

		if (!strcmp(lognamp, pw_ptr1p->pw_name) ||
		    (ypflag && !strcmp(lognamp+1, pw_ptr1p->pw_name)) ||
		    (*pw_ptr1p->pw_name == '+' &&
				!strcmp(lognamp, pw_ptr1p->pw_name+1))) {
			/* no good if we want to add an existing logname */
			if ( optn_mask & A_MASK ) {
				rid_tmpf () ;
				if ((SHAD_FLAG&info_mask) && !getspnam(shadownamep))
					bad_pasf();
				else
					bad_name("name already exists");
			}
			/* remember we found it */
			info_mask |= FOUND ;

#ifdef _SHAREII
			/*
			 * Share needs to keep track of the original
			 * UID of the user being deleted or modified,
			 * for later checking.
			 */
			SHoriguid = pw_ptr1p->pw_uid;
			SHoriggid = pw_ptr1p->pw_gid;
#endif /* _SHAREII */

			/* Do not write it out on the fly */
			if ( optn_mask & D_MASK )
				info_mask &= ~WRITE_P_ENTRY ;

			if ( optn_mask & M_MASK ) {
				if ((SHAD_FLAG & info_mask) && 
				    !getspnam(shadownamep)) {
					rid_tmpf () ;
					bad_pasf () ;
				}
				if ( optn_mask & L_MASK )
					pw_ptr1p->pw_name = passwd_st.pw_name;
				if ( optn_mask & U_MASK )
					pw_ptr1p->pw_uid = passwd_st.pw_uid;
				if ( optn_mask & G_MASK )
					pw_ptr1p->pw_gid = passwd_st.pw_gid;
				if ( optn_mask & C_MASK ) {
					pw_ptr1p->pw_comment = 
						passwd_st.pw_comment;
					pw_ptr1p->pw_gecos=passwd_st.pw_comment;
				}
				if ( optn_mask & H_MASK )
					pw_ptr1p->pw_dir = passwd_st.pw_dir ;
				if ( optn_mask & S_MASK )
					pw_ptr1p->pw_shell = passwd_st.pw_shell;
				ck_p_sz(pw_ptr1p);  /* check entry size */
			}
		}

		if ( (info_mask & WRITE_P_ENTRY) )
		{
		/* handle +::0:0::: et al, which must be/stay the last entry */
		    	if( (pw_ptr1p->pw_name[0] == '+')
		    	 && (pw_ptr1p->pw_name[1] == '\0'))
			{
				/* HUH? can't have two of these entries */
				if (savedflag == 1 ) 
				{
					bad_pasf();
				}
				savedflag = 1;
				(void)memcpy((void *)&savedpwent,
				             (void *)pw_ptr1p,
				             sizeof(struct passwd));
			}
			else
			{
				if ( putpwent ( pw_ptr1p, fp_ptemp ) ) {
					rid_tmpf () ;
					file_error () ;
				}
			}
		}

	    } else {	/* pw_ptr1p == NULL */
		if (errno == 0)	/* end of file */
			end_of_file = 1;
		else if (errno == EINVAL) { /* Bad entry found, skip it */
			error++;
			errno = 0;
#if DEBUG
			fprintf(stderr, "Bad passwd entry (NULL pw_ptr1p)\n");
#endif
		} else	/* unexpected error found */
			end_of_file = 1;		
	    }
	}

	if (error >= 1)
		fprintf(stderr,
			"%s: Bad entry found in /etc/passwd.  Run pwconv.\n",
			prognamp);
		
	/* Cannot find the target entry and we are deleting or modifying */

	if ( !(info_mask & FOUND) && ( optn_mask & (D_MASK|M_MASK) ) ) 
		{
		rid_tmpf () ;
		if ( (SHAD_FLAG & info_mask) && getspnam ( shadownamep ) )
			bad_pasf () ;
		else
			bad_name ( "name does not exist" ) ;
		}

	/* First available uid above UID_MIN is ... */

	if ( info_mask & NEED_DEF_UID )
		passwd_st.pw_uid = uid_sp->high + 1 ;
#ifdef DEBUG1
	for (uid_ptr = uid_sp; uid_ptr; uid_ptr = uid_ptr->link)
	{
		printf("UID Block: 0x%x hi: %d lo: %d\n",uid_ptr,uid_ptr->high, uid_ptr->low);
	}
#endif

	/* Write out the added entry now */

	if ( optn_mask & A_MASK )
		{
		ck_p_sz ( &passwd_st ) ;	/* Check entry size */
		if ( putpwent ( &passwd_st, fp_ptemp ) )
			{
			rid_tmpf () ;
			file_error () ;
			}
		}
	/* SGI: put out saved +::0:0::: entry */
	if (savedflag)
	{
		if ( putpwent ( &savedpwent, fp_ptemp) )
		{
			rid_tmpf ();
			file_error ();
		}
	}

	(void) fclose ( fp_ptemp ) ;

	/* Now we are done with PASSWD */

#ifdef _SHAREII
	/*
	 * Before committing to the operation, do a permission check
	 * that the invoker is allowed to do what they specified.
	 * If a normal user adds a new user, then a new lnode must
	 * be created and placed under the invoker's control.
	 * Normal users are not permitted to modify or delete the accounts
	 * of users who are not under their direct control.
	 *
	 * If permission is granted, then an lnode is created and/or deleted
	 * to keep the lnode file in line with the new state of the passwd
	 * file.  If a new UID is added then a new lnode must be created.
	 * If a UID is no longer used, then its lnode must be deleted.
	 *
	 * Before passmgmt exits, it must call either SHbackoutuser()
	 * or SHcommituser() to finalise these lnode operations.  If
	 * neither is called, then it will not spell disaster, but some
	 * lnodes may be left in existence with no password file entries.
	 * This may cause a few warning messages in the future, but no
	 * failures.
	 */
	if (SHshareRunning)
	{
		int error = 0;

		if (optn_mask & A_MASK)
			error = SH_HOOK_ADDUSER(passwd_st.pw_uid, passwd_st.pw_gid);
		else if (optn_mask & M_MASK)
			error = SH_HOOK_MODUSER
				(
					SHoriguid,
					(optn_mask & U_MASK) ? passwd_st.pw_uid : SHoriguid,
					SHoriggid,
					(optn_mask & G_MASK) ? passwd_st.pw_gid : SHoriggid
				);
		else if (optn_mask & D_MASK)
			error = SH_HOOK_DELUSER(SHoriguid);

		switch (error)
		{
		case 0:	break;
		case 1:	bad_perm();
			break;
		case 2: bad_uid();
			break;
		default:
			file_error();
		}
	}
#endif /* _SHAREII */

	/* Do this if we are touching both password files */
	
	if ( info_mask & BOTH_FILES ) {
		info_mask &= ~FOUND ;		/* Reset FOUND flag */

		/* The while loop for reading SHADOW entries */
		info_mask |= WRITE_S_ENTRY ;

		end_of_file = errno = error = 0;

		while (!end_of_file) {
		if ( (sp_ptr1p = (struct spwd *) getspent()) != NULL ) {
#if DEBUG
			fprintf(stderr, "Processing ");
			dump_spwent(sp_ptr1p);
#endif
			/* See if the new logname already exist in the
			   shadow passwd file */
			if ( ( optn_mask & M_MASK ) &&
			     strcmp ( shadownamep, shadow_st.sp_namp ) &&
			     ( !strcmp ( sp_ptr1p->sp_namp,
					 shadow_st.sp_namp ) ) )
				{
				rid_tmpf () ;
				bad_pasf () ;
				}

			if ( !strcmp ( shadownamep, sp_ptr1p->sp_namp ) )
			      {
			      info_mask |= FOUND ;
			      if ( optn_mask & A_MASK ) 
					{
					rid_tmpf () ;
					bad_pasf () ;	/* password file
							   inconsistent */
					}

			      if ( optn_mask & M_MASK )
					{
			      		sp_ptr1p->sp_namp = shadow_st.sp_namp ;
					if ( F_MASK & optn_mask)
			      			sp_ptr1p->sp_inact = 
							shadow_st.sp_inact;
#if DO_PWEXPIRE
					if ( E_MASK & optn_mask)
			      			sp_ptr1p->sp_expire =
							shadow_st.sp_expire;
#endif
					ck_s_sz ( sp_ptr1p ) ;
					}

			      if ( optn_mask & D_MASK )
			      	    	info_mask &= ~WRITE_S_ENTRY ;
			      }

			if ( info_mask & WRITE_S_ENTRY )
				{
				if ( putspent ( sp_ptr1p, fp_stemp ) )
					{
					rid_tmpf () ;
					file_error () ;
					}
				}
			else	
				info_mask |= WRITE_S_ENTRY ;

			} else {
				if (errno == 0) 
					end_of_file = 1;
				else if (errno==EINVAL) { /*bad entry, skip it*/
					error++;
					errno = 0;
#if DEBUG
					fprintf(stderr,
					    "Bad shadow ent (NULL sp_ptr1p)\n");
#endif
				} else 
					/* unexpected error found */
					end_of_file = 1;
			}	
		}
		
	if (error >= 1)
		fprintf(stderr,
			"%s: Bad entry found in /etc/shadow.  Run pwconv.\n",
			prognamp);

		/* If we cannot find the entry and we are deleting or
		   modifying */

		if ( !(info_mask & FOUND) && (optn_mask & (D_MASK|M_MASK)) )
			{
			rid_tmpf () ;
			bad_pasf () ;
			}

		if ( optn_mask & A_MASK )
			{
			ck_s_sz ( &shadow_st ) ;
			if ( putspent ( &shadow_st, fp_stemp ) )
				{
				rid_tmpf () ;
				file_error () ;
				}
			}

		(void) fclose (fp_stemp) ;

		/* Done with SHADOW */
		} /* End of if info_mask */

	/* ignore all signals */

	for ( i = 1 ; i < NSIG ; i++ )
		(void ) sigset ( i, SIG_IGN ) ;

	errno = 0 ;		/* For correcting sigset to SIGKILL */

	if (unlink (OPASSWD) && access (OPASSWD, 0) == 0)
		file_error () ;

	if (rename(PASSWD, OPASSWD) == -1)
		file_error () ;


	if (rename(PASSTEMP, PASSWD) == -1)
		{
		if (link (OPASSWD, PASSWD))
			bad_news () ;

		file_error () ;
		}


	if ( info_mask & BOTH_FILES )
		{

		if (unlink (OSHADOW) && access (OSHADOW, 0) == 0)
			{
			if ( rec_pwd() ) 
				bad_news () ;
			else
				file_error () ;
			}

		if (rename(SHADOW, OSHADOW) == -1)
			{
			if ( rec_pwd() ) 
				bad_news () ;
			else
				file_error () ;
			}
		

		if (rename(SHADTEMP, SHADOW) == -1)
			{
			if (rename(OSHADOW, SHADOW) == -1)
				bad_news () ;

			if ( rec_pwd() ) 
				bad_news () ;
			else
				file_error () ;
			}

		}

	ulckpwdf () ;

#ifdef _SHAREII
	/*
	 * At this point, we know the password/shadow file operations have
	 * succeeded.  We cn now finally commit to the lnode deletions
	 * that are required.
	 */
	if (SHshareRunning)
	{
		SH_HOOK_COMMITUSER();
	}
#endif /* _SHAREII */

}  /* end of main */

/* Try to recover the old password file */

int
rec_pwd ()
{
	if ( unlink ( PASSWD ) || link ( OPASSWD, PASSWD ) )
		return (-1) ;

	return (0) ;
}

/* combine two uid_blk's */

void
uid_bcom ( uid_p )
struct uid_blk *uid_p ;
{
	struct uid_blk *uid_tp ;

	uid_tp = uid_p->link ;
	uid_p->high = uid_tp->high ;
	uid_p->link = uid_tp->link ;

	free ( uid_tp ) ;
}

/* add a new uid_blk */

void
add_ublk ( num, uid_p )
uid_t num ;
struct uid_blk *uid_p ;
{
	struct uid_blk *uid_tp ;

	if ( (uid_tp = (struct uid_blk *) malloc((size_t) 
		       sizeof(struct uid_blk)) ) == NULL )
		{
		rid_tmpf () ;
		file_error () ;
		}

	uid_tp->high = uid_tp->low = num ;
	uid_tp->link = uid_p->link ;
	uid_p->link = uid_tp ;
}

/*
 *	Here we are using a linked list of uid_blk to keep track of all
 *	the used uids.  Each uid_blk represents a range of used uid,
 *	with low represents the low inclusive end and high represents
 *	the high inclusive end.  In the beginning, we initialize a linked
 *	list of one uid_blk with low = high = (UID_MIN-1).  This was
 *	done in main(). 
 *	Each time we read in another used uid, we add it onto the linked 
 *	list by either making a new uid_blk, decrementing the low of 
 *	an existing uid_blk, incrementing the high of an existing 
 *	uid_blk, or combining two existing uid_blks.  After we finished
 *	building this linked list, the first available uid above or
 *	equal to UID_MIN is the high of the first uid_blk in the linked
 *	list + 1.
 */

/* add_uid() adds uid to the link list of used uids */
void
add_uid ( uid )
uid_t uid ;
{
   struct uid_blk *uid_p ;
   /* Only keep track of the ones above UID_MIN */

   if ( uid >= UID_MIN )
      {
      uid_p = uid_sp ;

      while ( uid_p != NULL )
   	{

   	if ( uid_p->link != NULL )
   	   {

	   if ( uid >= uid_p->link->low )
	   	uid_p = uid_p->link ;

	   else if ( uid >= uid_p->low && 
		     uid <= uid_p->high )
		   {
		   uid_p = NULL ;
		   }

	        else if (uid == (uid_p->high+1))
		        {

		        if ( ++uid_p->high == (uid_p->link->low - 1) )
			   {
			   uid_bcom (uid_p) ;
			   }
		           uid_p = NULL ;
		         }

		     else if ( uid == (uid_p->link->low - 1) )
			     {
			     uid_p->link->low -- ;
			     uid_p = NULL ;
			     }

			  else if ( uid < uid_p->link->low )
			     	  {
			     	  add_ublk ( uid, uid_p ) ;
			     	  uid_p = NULL ;
			     	  }
	   } /* if uid_p->link */

	else

	   {
	
	   if ( uid == (uid_p->high + 1) )
	      {
	      uid_p->high++ ;
	      uid_p = NULL ;
	      }

	   else if ( uid >= uid_p->low && 
		     uid <= uid_p->high )
		   {
		   uid_p = NULL ;
		   }

		else
		   {
		   add_ublk ( uid, uid_p ) ;
		   uid_p = NULL ;
		   }

	   } /* else */
	
	} /* while uid_p */

      } /* if uid */
}

void
bad_perm()
{
	(void) fprintf (stderr, "%s: Permission denied\n", prognamp ) ;
#ifdef _SHAREII
	if (SHshareRunning)
	{
		SH_HOOK_BACKOUTUSER();
	}
#endif /* _SHAREII */	
	exit (1) ;
}

void
bad_usage(sp)
char *sp ;
{
	if ( strlen (sp) != 0 )
		(void) fprintf (stderr,"%s: %s\n", prognamp, sp) ;
	(void) fprintf (stderr,"Usage:\n") ;
	(void) fprintf (stderr,
		"%s -a [-c comment] [-h homedir] [-u uid [-o]] [-g gid] \n",
		prognamp ) ;
	(void) fprintf (stderr,
#if DO_PWEXPIRE
		"            [-s shell] [-f inactive] [-e expire] name\n") ;
#else
		"            [-s shell] [-f inactive] name\n") ;
#endif
	(void) fprintf (stderr,
		"%s -m  -c comment | -h homedir | -u uid [-o] | -g gid |\n",
		prognamp ) ;
	(void) fprintf (stderr,
#if DO_PWEXPIRE
		"             -s shell | -f inactive | -e expire | -l logname  name\n") ;
#else
		"             -s shell | -f inactive | -l logname  name\n") ;
#endif
	(void) fprintf (stderr,"%s -d name\n", prognamp ) ;
	if ( info_mask & LOCKED )
		ulckpwdf () ;
	exit (2) ;
}

void
bad_arg(s)
char *s ;
{
	(void) fprintf (stderr, "%s: %s\n",prognamp, s) ;

	if ( info_mask & LOCKED )
		ulckpwdf () ;
	exit (3) ;
}

void
bad_name(s)
char *s ;
{
	(void) fprintf (stderr, "%s: %s\n",prognamp, s) ;
	ulckpwdf () ;
	exit (9) ;
}

void
bad_uid()
{
	(void) fprintf (stderr, "%s: UID in use\n", prognamp ) ;

	ulckpwdf () ;
	exit (4) ;
}

void
bad_pasf()
{
	(void) fprintf (stderr, "%s: Inconsistent password files\n", prognamp);

	ulckpwdf () ;
#ifdef _SHAREII
	if (SHshareRunning)
	{
		SH_HOOK_BACKOUTUSER();
	}
#endif /* _SHAREII */	
	exit (5) ;
}

void
file_error()
{
	(void) fprintf (stderr, 
		"%s: Unexpected failure.  Password files unchanged\n",
		prognamp ) ;

	ulckpwdf () ;
#ifdef _SHAREII
	if (SHshareRunning)
	{
		SH_HOOK_BACKOUTUSER();
	}
#endif /* _SHAREII */
	exit (6) ;
}

void
bad_news()
{
	(void) fprintf (stderr,
		"%s: Unexpected failure.  Password file(s) missing\n",prognamp);

	ulckpwdf () ;
#ifdef _SHAREII
	if (SHshareRunning)
	{
		SH_HOOK_BACKOUTUSER();
	}
#endif /* _SHAREII */
	exit (7) ;
}

void
no_lock ()
{
	(void) fprintf(stderr,"%s: Password file(s) busy.  Try again later\n",
		prognamp);

	exit (8) ;
}

/* Check for the size of the whole passwd entry */
void
ck_p_sz ( pwp )
struct passwd *pwp ;
{
	char ctp[128] ;

	/* Ensure that the combined length of the individual */
	/* fields will fit in a passwd entry. The 1 accounts for the */
	/* newline and the 6 accounts for the colons (:'s) */
	if ( (	strlen  ( pwp->pw_name ) + 1 +
		sprintf ( ctp, "%d", pwp->pw_uid ) +
		sprintf ( ctp, "%d", pwp->pw_gid ) +
		strlen  ( pwp->pw_comment ) +
		strlen  ( pwp->pw_dir )	+
		strlen  ( pwp->pw_shell ) + 6) > (ENTRY_LENGTH-1) )
		{
		rid_tmpf () ;
		bad_arg ("New password entry too long") ;
		}
}

/* Check for the size of the whole passwd entry */
void
ck_s_sz ( ssp )
struct spwd *ssp ;
{
	char ctp[128] ;

	/* Ensure that the combined length of the individual */
	/* fields will fit in a shadow entry. The 1 accounts for the */
	/* newline and the 7 accounts for the colons (:'s) */
	if ( (	strlen  ( ssp->sp_namp ) + 1 +
		strlen  ( ssp->sp_pwdp ) +
		sprintf ( ctp, "%d", ssp->sp_lstchg ) +
		sprintf ( ctp, "%d", ssp->sp_min ) +
		sprintf ( ctp, "%d", ssp->sp_max ) + 
		sprintf ( ctp, "%d", ssp->sp_warn ) + 
		sprintf ( ctp, "%d", ssp->sp_inact ) + 
		sprintf ( ctp, "%d", ssp->sp_expire ) + 7) > (ENTRY_LENGTH - 1))
		{
		rid_tmpf () ;
		bad_arg ("New password entry too long") ;
		}
}

/* Get rid of the temp files */
void
rid_tmpf ()
{
	(void) fclose ( fp_ptemp ) ;

	if ( unlink ( PASSTEMP ) )
		(void) fprintf ( stderr, "%s: warning: cannot unlink %s\n", 
				 prognamp, PASSTEMP ) ;

	if ( info_mask & BOTH_FILES )
		{
		(void) fclose ( fp_stemp ) ;

		if ( unlink ( SHADTEMP ) )
			(void) fprintf ( stderr, 
					 "%s: warning: cannot unlink %s\n", 
					 prognamp, SHADTEMP ) ;
		}
}

#if DEBUG
static void
dump_pwent(struct passwd *pwp)
{
    fprintf(stderr,
	"passwd entry: name \"%s\" pw \"%s\", %d.%d, dir \"%s\" shell \"%s\"\n",
	_SAFESTR(pwp->pw_name), _SAFESTR(pwp->pw_passwd), pwp->pw_uid,
	pwp->pw_gid, _SAFESTR(pwp->pw_dir), _SAFESTR(pwp->pw_shell));
}

static void
dump_spwent(struct spwd *spwp)
{
    fprintf(stderr,
	"shadow pwentry: name \"%s\", passwd \"%s\"\n",
	_SAFESTR(spwp->sp_namp), _SAFESTR(spwp->sp_pwdp));
}

static int
xxlckpwdf()
{
	return(0);
}

static void
xxulckpwdf()
{
}
#endif /* DEBUG */
