#define VERSION(v, l) ((VendorCode == (v)) && (ConfigLevel == (l)))
#define VENDOR(v)  (VendorCode == (v))

/* Each of the following #define represent a SGI extension */
/* In theory if you dont define SGI_EXTENSIONS you will get a virgin 
   sendmail */
    
#if defined(SGI_EXTENSIONS)

/* these are bugfixes or extensions that need to submitted back to sendmail 
   maintainers */ 
#define SGI_BUGFIX

/* defines 'u' by default for the prog mailer */
#define SGI_OLD_MAILER_SETTING 

/* Support pathalias database */
#define SGI_MAP_PA			/* was part of SGI_OLD_MAPS */

/* pa map look up via $[..$] */
#if defined(SGI_MAP_PA)
#define SGI_BAD_PA_HACK			
#endif

/* Support DNS lookups via ${ and $} */
/* #define SGI_MAP_NAMESERVER  */	/* was part of SGI_OLD_MAPS */

/* Support aliases lookups from nsd */
/* #define SGI_MAP_NSD */		/* Autodefined by ../Makefile */

/* old maps '@', '.' */
#if defined(SGI_MAP_NAMESERVER)
#define SGI_DONT_ADD_DOT_IN_MX_PROBE	
#endif

/* use resolv.conf "hostresorder" */
#define SGI_HOST_RES_ORDER

#define SGI_ALIAS_EXPN_FILE_ADDR

/* extend 'D' to support pipe */
#define SGI_DEFINE_VIA_PROG	

/* freeze file into striped text */
#define SGI_FREEZE_FILE		

#if defined(SGI_FREEZE_FILE)
#define _PATH_SENDMAILFC  "/etc/sendmail.fc"
#endif

/* This changes the level that refuesing connection messages get logged. */
/* Needed since SGI does not support setproctitle */
#ifdef LOG
#define SGI_CONNECTION_LOGLEVEL 4
#endif

/* this work on both 32 & 64 bit cpu -- Supports SGI_CONUNT_PROC */
#define SGI_GETLA		

/* count sendmail process adding it to the load avarage reuturned */
#if defined(SGI_GETLA)
/* #define SGI_COUNT_PROC */
#define SGI_SENDMAIL_LA_SEM 
#endif

/* Support SGI's sysctl interface to IPalias */
/* #define SGI_IP_ALIAS */		/* Autodefined by ../Makefile */

#if defined(SGI_IP_ALIAS)
#define EXTERNAL_LOAD_IF_NAMES
#endif 

/* show line# when in "-d21.12" */
#define SGI_SHOW_LINE_NUMBER	

/* Don't set UnixFromLine to Envelope sender address */
#define SGI_UNIXFROMLINE_SAME_AS_FROMHEADER 

#define SGI_INSIST_VALID_HOME_DIRECTORY	

/* reason unknown */
#define SGI_NEVER_EXPN_ALIAS_OWNER			

/* reason unknown */
#define SGI_NO_ERROR_CODE_FOR_GETSERVBYNAME_ERROR	

/* reason unknown */
#define SGI_QFILE_MUST_BE_REGULAR 			

/* A group of Trusted irix (standard in 6.5+) support extensions */
#if defined(SGI_SECURITY)	/* Autodefined by ../Makefile */
#define SGI_AUDIT		/* Audit sendmail operation */
#define SGI_CAP			/* Posix capability support */
#define SGI_MAC			/* Mandatory Access Control */

#if defined(SGI_MAC)
#define SGI_CHECKCOMPAT
#define SGI_SESMGR
#endif
#endif /* SGI_SECURITY*/

/* The following two "#define" are to deal with problem of wildcard */
/* MX record: 							    */
/* if you have wildcard MX, you will lose the capability to         */
/* extract canonical name from MX record. (This is because a match  */
/* due to wildcard MX and/or explict MX is indistinguishable).      */
/* The first "#define" is the "bsd" way of dealing with the problem */
/*     it ignore all MX in the $[...$] operator 		    */
/* The second "#define" is the "sgi" way of dealing with the problem*/
/*     it ignore all non-local MX record (i.e no srch )	 	    */
#ifdef notdef
#define SGI_IGNOR_MX_IN_HOST_MAP      /* bsd solution: ignore mx in $[...$] */
#endif
#define SGI_IGNOR_MX_SRCH_IN_HOST_MAP /* sgi solution: ignore mx srch in $[... $] */

/* looks for ^+: in /etc/aliases and if found, sets NIS aliases */
#define SGI_NIS_ALIAS_KLUDGE

/* trap <> and turn it into '\n' */
#define SGI_UGLY_NULL_ADDR_HACK		

/* warning: could cause name service look up storm */
#define SGI_MATCHGECOS_EXTENSION	

#ifdef SGI_MATCHGECOS_EXTENSION
#define MATCHGECOS 1
#endif

#define sleep(x) sm_sleep(x) /* to avoid compiler warning */

#ifdef NEWDB
#define SGI_NEWDB_NOT_IMPLICIT
#endif

/* Use our version of getmxrr */
#define SGI_PROBEMXRR
#define getmxrr probemxrr

#endif /* SGI_EXTENSIONS */

