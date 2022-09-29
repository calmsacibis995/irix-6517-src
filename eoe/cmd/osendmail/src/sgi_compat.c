#include "sendmail.h"
#include <sysexits.h>

static void
sgi_init_old_maps(ENVELOPE *e)
{
	char buf[MAXLINE];

#if defined(SGI_MAP_NAMESERVER)
	strcpy(buf, "@ nameserver");
	makemapentry(buf);
	strcpy(buf, ". nameserver");
	makemapentry(buf);
	strcpy(buf, ": nameserver");
	makemapentry(buf);
	strcpy(buf, "? nameserver");
	makemapentry(buf);
	strcpy(buf, "C nameserver");
	makemapentry(buf);
	strcpy(buf, "c nameserver");
	makemapentry(buf);
	strcpy(buf, "H nameserver");
	makemapentry(buf);
	strcpy(buf, "h nameserver");
	makemapentry(buf);
#endif

#if defined(SGI_MAP_PA)
	if (stab("pa", ST_MAP, ST_FIND) == NULL)
	{
		char *p;

		strcpy(buf, "pa pa -o ");
		if ((p = macvalue('P', e)) != NULL)
			strcat(buf, p);
		else
			strcat(buf, "/dev/null");
		makemapentry(buf);
	}
#endif

}

void
sgi_pre_defaults(ENVELOPE *e)
{
	 /* default is V1/sgi mode */
	ConfigLevel= 1;

	/* if we are building a freeze file          */
	/* do not do the default setoption() stuff below */
	if (OpMode == MD_FREEZE) 
		return;

#ifdef SGI_IGNOR_MX_IN_HOST_MAP
	setoption('I', "+HasWildcardMX", TRUE, FALSE, e);
#endif
	/* if we are the best MX, try host directly */
/*	setoption('w', "", TRUE, FALSE, e); */

	/* use local time zone */
	setoption('t', "", TRUE, FALSE, e);
}

void
sgi_post_defaults(ENVELOPE *e)
{
#if defined(SGI_EXTENSIONS)
	if (VERSION(VENDOR_SGI,1)) {
		sgi_init_old_maps(e);
#if defined(SGI_NIS_ALIAS_KLUDGE) 
		sgi_nis_alias_kludge(e);
#endif
#if defined(SGI_OLD_MAILER_SETTING)
		sgi_fix_mailers_flags();
#endif
	}
#endif
}



#if defined(SGI_EXTENSIONS) && defined(SGI_MAP_NAMESERVER)

#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <resolv.h>
#include <arpa/nameser.h>

#include <netdb.h>
/*
** Name Server (BIND) support for sendmail.
**
** This code is derived from sendmail(5.59) and named(4.8) code.
**
** NOTE: Must use malloc/free to hold answers!
*/

typedef union
{
    HEADER qb1;
    char qb2[PACKETSZ];
} querybuf;

extern int h_errno;

/*
 * Buffers used by various lookup functions.
 *
 * Should use malloc/free for this...
 *
 */

static char mb_buf[BUFSIZ];
static char mr_buf[BUFSIZ];

/*
 * Set to the value of the last (successfull) lookup.
 */

int NS_lookup_type = 0;

/*
 * getM_rr:
 *
 *  Makes a "q_t" query and returns result in "m_names".
 *  Used for T_{MR,MB,MG} lookups. (Only looks for answers
 *  of "q_t" type.
 */

int
getM_rr (char *name, int q_t, char **m_names, int max_names, char *bp,
	 int buflen, bool *tempfail)
{
    HEADER *hp;
    char *eom;
    char *cp;
    querybuf answer;
    int n;
    int n1;
    int count;
    int ancount;
    int qdcount;
    u_short type;

    *tempfail = FALSE;
    n = res_query (name, C_IN, q_t, (u_char *)&answer, sizeof (answer));
    if (n < 0)
    {
#ifdef DEBUG
    if (tTd (8, 1) || _res.options & RES_DEBUG)
        printf ("reskquery failed\n");
#endif
        *tempfail = (h_errno == TRY_AGAIN
		     && (errno != ECONNREFUSED || UseNameServer));
#ifdef LOG
        if (*tempfail && LogLevel > 1)
            sm_syslog(LOG_WARNING,NOQID,"getM_rr: Name server lookup failed for %s",name);
#endif /* LOG */
        return -1;
    }

    eom = (char *) &answer + n;

    /*
     * Find first satisfactory answer
     */

    hp = (HEADER *) &answer;
    ancount = ntohs (hp->ancount);


    count = 0;
    cp = (char *) &answer + sizeof (HEADER);

    for (qdcount = ntohs (hp->qdcount); qdcount--; cp += n+QFIXEDSZ)
        if ((n = dn_skipname((const u_char *)cp,(const u_char *)eom)) < 0)
            return 0;

    while (--ancount >= 0 && cp < eom && count < max_names)
    {
        if ((n = dn_expand((u_char *)&answer, (u_char *)eom, (u_char *)cp,
			   (char *)bp, buflen)) < 0)
            break;

        cp += n;
        GETSHORT(type,cp);
        cp += sizeof (u_short) + sizeof (u_long);
        GETSHORT(n,cp);

        if (type != q_t)
        {
#ifdef DEBUG
            if (tTd (8, 1) || _res.options & RES_DEBUG)
                    printf ("unexpected answer type %d, size %d\n",
                        type, n);
#endif
            cp += n;
            continue;
        }

        if ((n = dn_expand ((u_char *) &answer, (u_char *)eom, (u_char *)cp,
			    (char *)bp, buflen)) < 0)
            break;

        cp += n;
        m_names[count++] = bp;
        n1 = strlen (bp) + 1;
        bp += n1;
        buflen -= n1;
    }

    return count;
}

/*
 * ns_expand_mbox:
 *
 *  Input:  a domain name, foo.bar.frotz.
 *      Output: a mail address, foo@bar.frotz.
 *
 *      Dots in the local part must be quoted, i.e. foo\.bar.frotz
 *      would result in foo.bar@frotz.
 *
 * BUGS: don't knows about quoted strings.
 *
 */

void
ns_expand_mbox (char *s)
{
    int found_dot = 0;
    char *tmp;

    tmp = s;

    while (s && *s)
    {
        switch (*s)
        {
            case '\\':
                s++;
                if (!*s)
                    syserr ("ns_expand_mbox: unexpected end of string");
                break;

            case '.':
                if (!found_dot)
                {
                    found_dot++;      
                    *s = '@';
                }
                break;
        }
        *tmp++ = *s++;
    }
    *tmp = '\0';
}

bool
gen_address (char *name, int namesize, bool *tempfail)
{
    char global[BUFSIZ];
    char buf[BUFSIZ];
    char *at;
    char *bp[1024];


    (void) strncpy (global, name, BUFSIZ);

    if ((at = strchr (name, '@')))
        *at = '.';

    if (getM_rr (name, T_MB, bp, 1024, mb_buf, BUFSIZ,tempfail) == 1 && !*tempfail)
    {
        if (at)
            *at = '\0';

#ifdef LOG
        if (LogLevel > 9)
            sm_syslog (LOG_DEBUG, NOQID,"%s: [MB (generic)]: %s => %s",
                CurEnv->e_id, global, bp[0]);
#endif
        (void) sprintf (buf, "%s@%s", name, bp[0]);

        if (strlen (buf) > namesize)
            syserr ("gen_address: result \"%s\" too long after expansion",
                    buf, namesize);
        else
            (void) strcpy (name, buf);

        return TRUE;
    }
    else
    {
        if (at)
            *at = '@';

        return FALSE;
    }
    /* NOTREACHED */
}

bool
gen_name (char *name, int namesize, bool *tempfail)
{
    char global[BUFSIZ];
    char *at;
    char *bp[1024];

    (void) strncpy (global, name, BUFSIZ);

    if ((at = strchr (name, '@')))
        *at = '.';

    if (getM_rr (name, T_MR, bp, 1024, mr_buf, BUFSIZ,tempfail) == 1 && !*tempfail)
    {
        if (at)
            *at = '@';

        ns_expand_mbox (bp[0]);
#ifdef LOG
        if (LogLevel > 9)
            sm_syslog (LOG_DEBUG, NOQID,"%s: [MR (generic)]: %s => %s\n",
                CurEnv->e_id, global, bp[0]);
#endif
        if (strlen (bp[0]) > namesize)
            syserr ("gen_name: result \"%s\" too long after expansion",
                    bp[0], namesize);
        else
            (void) strcpy (name, bp[0]);

        return TRUE;
    }
    else
    {
        if (at)
            *at = '@';
        return FALSE;
    }
    /* NOTREACHED */
}

bool
gen_canon_name(char *host, int hbsize, bool *tempfail, int ns_type)
{
	register u_char *eom, *cp;
	register int n; 
	HEADER *hp;
	querybuf answer;
	u_short type;
	int first, ancount, qdcount, loopcnt;
	char nbuf[PACKETSZ];

	loopcnt = 0;
	*tempfail = FALSE;		/* NS */
loop:
	n = res_search(host, C_IN, ns_type, (u_char *)&answer, sizeof(answer));
	if (n < 0) {
		if (tTd(8, 1))
			printf("gen_canon_name:  res_search failed (errno=%d, h_errno=%d)\n",
			    errno, h_errno);
	        *tempfail = (h_errno == TRY_AGAIN
			     && (errno != ECONNREFUSED || UseNameServer));
#ifdef LOG
                if (*tempfail && LogLevel > 1)
                    sm_syslog(LOG_WARNING,NOQID,"Name server lookup failed for %s",host);
#endif /* LOG */
		return FALSE;
	}

	/* find first satisfactory answer */
	hp = (HEADER *)&answer;
	ancount = ntohs(hp->ancount);

	/* we don't care about errors here, only if we got an answer */
	if (ancount == 0) {
		if (tTd(8, 1))
			printf("rcode = %d, ancount=%d\n", hp->rcode, ancount);
		return FALSE;
	}
	cp = (u_char *)&answer + sizeof(HEADER);
	eom = (u_char *)&answer + n;
	for (qdcount = ntohs(hp->qdcount); qdcount--; cp += n + QFIXEDSZ)
		if ((n = dn_skipname((const u_char *)cp,
				     (const u_char *)eom)) < 0) {
#ifdef LOG
			if (LogLevel > 1)
				sm_syslog(LOG_WARNING,NOQID,"gen_canon_name: dn_skipname failed");
#endif /* LOG */
			return FALSE;
		};

	/*
	 * just in case someone puts a CNAME record after another record,
	 * check all records for CNAME; otherwise, just take the first
	 * name found.
	 */
	for (first = 1; --ancount >= 0 && cp < eom; cp += n) {
		if ((n = dn_expand((u_char *)&answer, eom,
				   cp, (char *)nbuf, sizeof(nbuf))) < 0) {
#ifdef LOG
			if (LogLevel > 1)
				sm_syslog(LOG_WARNING,NOQID,"gen_canon_name: dn_expand failed");
#endif /* LOG */
			return FALSE;
		};
		if (first) {			/* XXX */
			(void)strncpy(host, nbuf, hbsize);
			host[hbsize - 1] = '\0';
			first = 0;
		}
		cp += n;
		GETSHORT(type, cp);
 		cp += sizeof(u_short) + sizeof(u_long);
		GETSHORT(n, cp);
		if (type == T_CNAME)  {
			/*
			 * assume that only one cname will be found.  More
			 * than one is undefined.  Copy so that if dn_expand
			 * fails, `host' is still okay.
			 */
			if ((n = dn_expand((u_char *)&answer, eom, cp,
					   (char *)nbuf, sizeof(nbuf))) < 0) {
#ifdef LOG
				if (LogLevel > 1)
					sm_syslog(LOG_WARNING,NOQID,"gen_canon_name: dn_expand failed");
#endif /* LOG */
				return FALSE;
			};
			(void)strncpy(host, nbuf, hbsize); /* XXX */
			host[hbsize - 1] = '\0';
			if (++loopcnt > 8)	/* never be more than 1 */
				return TRUE;    /* No really right  Dan */
			goto loop;
		}
	}
	return TRUE;
}

/* ######################################################## */

/*
 * Generic DNS lookups for backward-compatibility with earlier IRIX
 * sendmail BIND lookup operators.
 */

char *
ns_map_lookup(MAP *map, char *name, char **av, int *statp)
{
	char *mxhosts[MAXMXHOSTS + 1];
	char namebuf[MAXNAME + 1];
	u_long OLDF;
	int rc;


	if (strlen(map->map_mname) > 1) {
		sm_syslog(LOG_ERR,NOQID,
		       "ns_map_lookup: illegal map name (%s)",
		       map->map_mname);
		return (NULL);
	}

	strncpy(namebuf, name, sizeof namebuf);
	OLDF = _res.options;
#ifdef NAMED_BIND
	_res.options &= ~(RES_DEFNAMES | RES_DNSRCH);
#endif /* NAMED_BIND */


	switch(*map->map_mname)
	{
	  case '@':
		_res.options &= ~RES_DNSRCH;
#ifdef SGI_DONT_ADD_DOT_IN_MX_PROBE
		sgi_mxrr_no_append_dot = 1;
#endif
		rc = probemxrr(name, mxhosts, TRUE, statp);
#ifdef SGI_DONT_ADD_DOT_IN_MX_PROBE
		sgi_mxrr_no_append_dot = 0;
#endif
		_res.options = OLDF;
		if (rc < 0)
			return(NULL);
		return (map_rewrite(map, mxhosts[0], strlen(mxhosts[0]), av));
		
	  case '.':
		_res.options |= RES_DNSRCH;
#ifdef SGI_DONT_ADD_DOT_IN_MX_PROBE
		sgi_mxrr_no_append_dot = 1;
#endif
		rc = probemxrr(name, mxhosts, TRUE, statp);
#ifdef SGI_DONT_ADD_DOT_IN_MX_PROBE
		sgi_mxrr_no_append_dot = 0;
#endif
		_res.options = OLDF;
		if (rc < 0)
			return(NULL);
		return (map_rewrite(map, mxhosts[0], strlen(mxhosts[0]), av));

	  case ':':
		rc = gen_name(namebuf, sizeof namebuf, statp);
		break;

	  case '?':
		rc = gen_address(namebuf, sizeof namebuf, statp);
		break;

	  case 'C':
		_res.options |= RES_DNSRCH;
		rc = gen_canon_name(namebuf, sizeof namebuf, statp, T_MX);
		break;

	  case 'c':
		_res.options &= ~RES_DNSRCH;
		rc = gen_canon_name(namebuf, sizeof namebuf, statp, T_MX);
		break;

	  case 'H':
		_res.options |= RES_DNSRCH;
		rc = gen_canon_name(namebuf, sizeof namebuf, statp, T_A);
		break;

	  case 'h':
		_res.options &= ~RES_DNSRCH;
		rc = gen_canon_name(namebuf, sizeof namebuf, statp, T_A);
		break;

	  default:
		sm_syslog(LOG_ERR,NOQID,
		       "ns_map_lookup: illegal map name (%s)",
		       map->map_mname);
		rc = FALSE;
		break;
	}
	_res.options = OLDF;
	
	if (rc == FALSE)
		return (NULL);
	return (map_rewrite(map, namebuf, strlen(namebuf), av));
}

/* See if a pathalias entry exists for the named host. */
#endif /* SGI_MAP_NAMESERVER */

#if defined(SGI_MAP_PA)

#include <resolv.h>
#include <ndbm.h>

/* ARGSUSED */
char *
pa_map_lookup(MAP *map, char *name, char **av, int *statp)
{
	DBM *db;
	datum key, res;
	char tbuf[MAXNAME];
	register char *p;

	db = (DBM *)map->map_db1;

	strcpy(tbuf,name);
	makelower(tbuf);

	res.dptr = NULL;
	if (NULL != db) {
		p = &tbuf[0];
		for (;;) {

			if (tTd(9,1)) {
				printf("pa_map_lookup(%s) =>", p);
			}

			key.dptr = p;
			/* include trailing null */
			key.dsize = strlen(p) + 1;
			res = dbm_fetch(db, key);
			if (res.dptr != NULL
			    || key.dsize < 3)
			        break;
			/* try without trailing null */
			key.dsize = strlen(p);
			res = dbm_fetch(db, key);
			if (res.dptr != NULL)
			        break;
			if (*p == '.')
			        p++;
			if (!(p = strchr(p,'.')))
			        break;
			if (tTd(9,1)) {
				printf("FAIL\n");
			}
		}
	}
	if (res.dptr != NULL
	    && 0 != strcmp("%s", &((char *)res.dptr)[0])) {
		if (res.dsize >= 4
		    && !strcmp("!%s",&((char *)res.dptr)[res.dsize-4])) {
			((char *)res.dptr)[res.dsize-4]  = '\0';
			res.dsize -=3;
		}
		if (res.dsize < sizeof(tbuf)) {
			(void)strcpy(tbuf, res.dptr);
			if (tTd(9,1)) {
				printf("%s\n", tbuf);
			}
			return (map_rewrite(map, tbuf, sizeof(tbuf), av));
		}
	}
	return (NULL);
}
#endif  /* SGI_MAP_PA */

#if defined(SGI_EXTENSIONS) && defined(SGI_DEFINE_VIA_PROG)
void 
define_via_prog(char n, char *val, ENVELOPE *e)
{
	char pipe[MAXLINE];

	if (sgi_prog_safe) {
		FILE *f;
		auto int fd;
		int i;
		char *argv[MAXPV +1];
		register char *p;
		pid_t pid;
		
		pipe[0] = '\0';
		i = 0;
		for (p = strtok(&val[1], " \t"); p != NULL; p = strtok(NULL, " \t"))
			{
				if (i >= MAXPV)
					break;
				argv[i++] = p;
			}
		argv[i] = NULL;
		pid = prog_open(argv, &fd, CurEnv);
		if (pid < 0) {
			f = NULL;
		} else {
			f = fdopen(fd, "r");
		}
		
		if (f == NULL) {
			val="";
		}
		if (fgets(pipe, sizeof pipe, f) != NULL) {
			p = &pipe[strlen(pipe)-1];
			while ((p >= pipe) && (*p == '\n')) *p-- = '\0';
		(void)fclose(f);
			if (pid > 0) 
				
				(void) waitfor (pid);
		} else {
			if (tTd(35, 1))
				printf(" (eval of pipe unsafe)");
		}
	}
	define(n, newstr(pipe), e);
}

/* this is currently not used  ... popen is considered UNSAFE */
void
sgi_eval_pipe(char *v)
{
	FILE *fp;
	char buf[MAXLINE - 2], *p;

        buf[0] = '\0';
	if ((fp = popen(&v[1], "r")) == NULL) {
        	syserr("cannot open %s", &v[1]);
		return;
	}
        fgets(buf, sizeof buf, fp);
        fclose(fp);
	p = &buf[strlen(buf)-1];
	/* delete trailing '\n' */
        while ((p >= buf) && (*p == '\n')) *p-- = '\0';
	strcpy(v, buf);
}
#endif

#if defined(SGI_EXTENSIONS) && defined(SGI_NIS_ALIAS_KLUDGE)
/* ARGSUSED */
void
sgi_nis_alias_kludge(ENVELOPE *e)
{
	char buf[MAXLINE];
	FILE *fp;
	int want_nis_alias = 0;

	/*
	 * support for old +:+ alias.
	 */
	fp = fopen("/etc/aliases", "r");
        while (fgets(buf, sizeof buf, fp)) {
		if (strncmp( "+:", buf, 2) == 0)  {
			want_nis_alias= 1;
			break;
		}

	}
	fclose(fp);
	if (want_nis_alias) {
		extern void setalias __P((char *));
		strcpy(buf, "nis:-o mail.aliases");
		setalias(buf);
#ifdef LOG
		if (LogLevel > 9) {
			sm_syslog(LOG_NOTICE, NOQID, "Added nis mail.aliases due to +: in /etc/aliases");
		}
#endif
        }
}
#endif

#if defined(SGI_EXTENSIONS) && defined(SGI_FREEZE_FILE)
#include <sys/types.h>
#include <sys/stat.h>

/*
**  GETFCNAME -- return the name of the .fc file.
*/

char *
getfcname(void)
{
        if (FreezeFile != NULL)
                return FreezeFile;
        return _PATH_SENDMAILFC;
}

int
thaw(char *freezefile)
{
	struct stat stb;
	extern void readcf __P((char *, bool, ENVELOPE *));

	if (stat(freezefile, &stb) < 0)
		return 0;

	if (!S_ISREG(stb.st_mode)) {
		sm_syslog(LOG_WARNING,NOQID,
		       "thaw: %s not a plain file.  Frozen config. not used.",
		       freezefile);
		return 0;
	}
	if (!stb.st_size) {
	    if (!(OpMode == MD_DELIVER) ||
		 (OpMode == MD_SMTP) ||
		 (OpMode == MD_PRINT)) {
		sm_syslog(LOG_WARNING,NOQID,
		       "thaw: %s is zero length.  Frozen config. not used.",
		       freezefile);
	    }
	    return 0;
	}

	readcf(freezefile, TRUE, CurEnv);
	return 1;
}

#endif

#if defined(SGI_EXTENSIONS) && defined(SGI_GETLA)
#include <limits.h>
#include <paths.h>
#include <dirent.h>
#include <sys/procfs.h>
#include <sys/sysmp.h>
#include <sys/syssgi.h>
#include <sys/fcntl.h>

#define CNTPROCS_INTERVAL	60

static int memfd = -1;
static int pcnt = 0;

int
sgi_kmem_init(void)
{
	if (memfd >= 0)
		return memfd;
	memfd = open("/dev/kmem", 0, 0);
	if (memfd < 0)
		return -1;
	(void) fcntl(memfd, F_SETFD, 1);
	errno = 0;
#if defined(SGI_EXTENSIONS) && defined(SGI_COUNT_PROC)
	if (!load_avg_only)
		sgi_do_cntprocs();
	else
		pcnt = 0;
#endif
	return memfd;
}


#if defined(SGI_EXTENSIONS) && defined(SGI_SENDMAIL_LA_SEM)
#include <sys/ipc.h>
#include <sys/sem.h>

static int
sgi_sendmail_semget(int salt) 
{
	static sgi_la_sem = -2;

	if (sgi_la_sem == -2) {
		key_t key;

		key = ftok("/usr/lib/sendmail", salt);
		if (key < 0) {
			sgi_la_sem = -1;
		} else {
			sgi_la_sem = semget(key, 1, 0600|IPC_CREAT);
#ifdef LOG
			if (LogLevel > 20) {
				sm_syslog(LOG_NOTICE, NOQID, 
			       "sgi_sendmail_semget salt %dkey %d, semid %d\n",
					  salt, key, sgi_la_sem);
			}
#endif
		}
	}
	return sgi_la_sem;
}

void
sgi_register_self_sem(void)
{
	struct sembuf sop;
	int i,sem;

	if (load_avg_only) 
		return;

	sop.sem_num=0;
	sop.sem_op=+1;
	sop.sem_flg=SEM_UNDO;
	
	sem = sgi_sendmail_semget(0);
	if (sem < 0) 
		return;

	i = semop(sem,&sop,1);
#ifdef LOG
	if (i < 0 && LogLevel > 2) {
		sm_syslog(LOG_INFO, NOQID,
			  "sgi_register_self_sem: failed to register self with sendmail count semaphore %d: %s", sem, strerror(errno));
	}
#endif
}			  
#endif /* SGI_SENDMAIL_LA_SEM */


int
sgi_getla(void)
{
	int avenrun[3];
	int la;
	int rtn;

	sgi_kmem_init();

	if (memfd < 0) {
		if (tTd(3, 1))
			printf("sgi_getla: /dev/kmem not open\n");
		return -1;
	}

	if (lseek(memfd, (sysmp(MP_KERNADDR, MPKA_AVENRUN)&0x7fffffff), 0) == -1
	    || read(memfd, (char *)avenrun, sizeof(avenrun)) < sizeof(avenrun))
		la = -1;
	else
		la = ((avenrun[0]+512)/1024);

	if (tTd(3, 10)) {
		printf("sgi_getla: la %d pcnt %d rtn %d\n",
		       la, pcnt, (la + pcnt));
	}

#if defined(SGI_EXTENSIONS) && defined(SGI_COUNT_PROC)
	rtn = la + pcnt + CurChildren - PcntChildren;
	if (rtn < 0) 
		rtn=0;
#ifdef LOG
	if (LogLevel > 20)
		sm_syslog(LOG_INFO, NOQID,"sgi_getla: la %d pcnt %d PcntChildren %d CurChildren %d rtn %d\n",
		       la, pcnt, PcntChildren, CurChildren, rtn);
#endif /* LOG */

#else 
#if defined(SGI_EXTENSIONS) && defined(SGI_SENDMAIL_LA_SEM)
	if (load_avg_only) {
		rtn = la;
	} else {
		int sem, cnt;

		sem = sgi_sendmail_semget(0);
		
		if (sem >= 0) {
			cnt = semctl(sem, 0, GETVAL);
			if (cnt < 0) {
				cnt = 0;
			}

			rtn = la + cnt;
#ifdef LOG
			if (LogLevel > 20)
				sm_syslog(LOG_INFO, NOQID,
					  "sgi_getla: la %d cnt %d rtn %d\n", 
					  la, cnt, rtn);
#endif /* LOG */
		} else {
			rtn = la;
		}
	}
#else
	rtn=la;
#endif /* SGI_SENDMAIL_LA_SEM */
#endif /* SGI_COUNT_PROC */
	return (rtn);
}
#endif /* SGI_GETLA */

#if defined(SGI_EXTENSIONS) && defined(SGI_COUNT_PROC)
int
sgi_cntprocs(char *pname)
{
	int fd, cnt;
	struct dirent *ent;
	DIR *dd;
	char prefix[PATH_MAX+1];
#ifdef IRIX4
	user_t uarea;
#else
	char path[PATH_MAX+1];
	prpsinfo_t psi;
#endif

	if (pname == NULL || *pname == '\0') {
		if (tTd(3, 1))
			printf("sgi_cntprocs: no pname\n");
		return 0;
	}

#ifdef _PATH_PROCFSPI
	strcpy(prefix, _PATH_PROCFSPI);
#else
	strcpy(prefix, "/proc");
#endif
        if ((dd = opendir(prefix)) == NULL) {
		strcpy(prefix, "/debug");
                dd = opendir(prefix);
	}
	if (dd == NULL) {
		if (tTd(3,1))
			printf("opendir(%s) failed: %s\n",
			       prefix, strerror(errno));
		return 0;
	}
	cnt = 0;
	while (ent = readdir(dd)) {
		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
			continue;
#ifdef IRIX4
		if (syssgi(SGI_RDUBLK, atoi(ent->d_name),
			   &uarea, sizeof(uarea)) < 0) {
			if (tTd(3, 1))
				perror("Can't get user area");
			continue;
		}
		if (!strncmp(uarea.u_comm, pname, strlen(pname)))
			cnt++;
#else /* not IRIX4 */
		if (strlen(prefix) + strlen(ent->d_name) + 1  > PATH_MAX) {
			if (tTd(3, 1))
				printf("Pathname %s/%s too long\n",
				       prefix, ent->d_name);
			continue;
		}
		strcpy(path, prefix);
		strcat(path, "/");
		strcat(path, ent->d_name);
		if ((fd = open(path, O_RDONLY)) == -1) {
			if (tTd(3,1))
				printf("Can't open process %s: %s\n",
				       path, strerror(errno));
			continue;
		}
		if (ioctl(fd, PIOCPSINFO, &psi) == -1) {
			if (tTd(3,1))
                                printf("Can't get info for process %s: %s\n",
				       path, strerror(errno));
			close(fd);
			continue;
		}
                if (psi.pr_zomb) {
			close(fd);
			continue;
		}
		if (!strncmp(psi.pr_fname, pname, strlen(pname)))
			cnt++;
		close(fd);
#endif /* IRIX4 */
	}
	closedir(dd);
	if (tTd(3, 10))
		printf("sgi_cntprocs(%s) returns %d\n", pname, cnt);
#ifdef LOG
	if (LogLevel > 20)
		sm_syslog(LOG_INFO, NOQID,"sgi_cntprocs(%s) returns %d\n", pname, cnt);
#endif /* LOG */
	return cnt;
}

void
sgi_do_cntprocs(void)
{
	extern void proc_list_probe __P((void));

	proc_list_probe();
        PcntChildren = CurChildren;
	pcnt = sgi_cntprocs("sendmail");
	setevent(CNTPROCS_INTERVAL, sgi_do_cntprocs, 0);
}
#endif /* SGI_COUNT_PROC */

#if defined(SGI_EXTENSIONS) && defined(SGI_OLD_MAILER_SETTING)
void
sgi_fix_mailers_flags(void)
{
	STAB *st;
	MAILER *m;

	if (!VERSION(VENDOR_SGI, 1)) 
		return;

	/* for backward compatibility*/
	/* turn on 'u' flag for prog mailer */
	st = stab("prog", ST_MAILER, ST_FIND);
        if (st != NULL)
                m = st->s_mailer;
	if (m) setbitn(M_USR_UPPER, m->m_flags);
}
#endif

#if defined(SGI_EXTENSIONS) && defined(SGI_MATCHGECOS_EXTENSION)
/*
**  PARTIALSTRING -- is one string of words contained by another?
**
**	See if one string of words can be found as part of
**	another string of words.  All substrings delimited by
**	one or more non-alphanumeric characters are considered
**	"words", and a partial match is such that all the words
**	of the pattern string are either full prefixes
**	of the target string.  Upper or lower case letters are
**	considered equal.
**
**	Parameters:
**		target -- target string
**		pattern -- pattern string
**
**	Returns:
**		The number of fully matched words, or -1 if none.
**
**	Side Effects:
**		None.
**
*/

int
partialstring(char *target, char *pattern)
{
    register char *t, *p, *q;
    int full_words = 0;

    /* skip initial delimiters */
    for (t = target; *t != '\0' && !(isascii(*t) && isalnum(*t)); t++);
    for (p = pattern; *p != '\0' && !(isascii(*p) && isalnum(*p)); p++);
    q = p;

    while (*t != '\0' && *p != '\0') {
	/*
	 * if at end of pattern word, find next, remember it,
	 * and eat the current target word
	 */
	if (!(isascii(*p) && isalnum(*p))) {
	    while (*p != '\0' && !(isascii(*p) && isalnum(*p)))
		p++;
	    if (*p == '\0')
		continue;
	    q = p;
	    if (!(isascii(*t) && isalnum(*t))) {
		full_words++;
	    }
	    while (*t != '\0' && isascii(*t) && isalnum(*t)) t++;
	    while (*t != '\0' && !(isascii(*t) && isalnum(*t))) t++;
	    continue;
	}

	/*
	 * if match, advance both pointers
	 */
	if ((isascii(*t) && isupper(*t) ? tolower(*t) : *t) ==
	    (isascii(*p) && isupper(*p) ? tolower(*p) : *p)) {
	    t++, p++;
	    continue;
	}

	/*
	 * if no match, backtrack to last unmatched pattern word and
	 * eat current target word
	 */
	p = q;
	while (*t != '\0' && isascii(*t) && isalnum(*t)) t++;
	while (*t != '\0' && !(isascii(*t) && isalnum(*t))) t++;
    }

    /*
     * now, the pattern should be fully consumed if there was a match
     */
    if (*p == '\0')
	return ((isascii(*t) && isalnum(*t)) ? full_words : full_words + 1);
    else
	return -1;
}


/*
 * For backward compatibility of passwd matching and GECOS fuzzy-searching,
 * use the old IRIX version of finduser() and its support routine
 * partialstring() (see above).
 */

#define WORST_MATCH	-2		/* even worse than no match */
#define NO_UID		-999		/* any "impossible" uid will do */

struct passwd *
finduser(char *name, bool *fuzzyp)
{
	register struct passwd *pw;
	int best_match = WORST_MATCH;
	int best_uid = NO_UID;

	errno = 0;
	*fuzzyp = FALSE;

	if (tTd(29, 4))
		printf("%s password entry for \"%s\"\n",
		       getpwnam(name) ? "found" : "can't find", name);

	/* look up this login name using fast path */
	if ((pw = getpwnam(name)) != NULL)
		return (pw);

	makelower(name);
	/* try lower case version of the name */
	if ((pw = getpwnam(name)) != NULL)
		return (pw);

#ifdef MATCHGECOS
	if (!MatchGecos)
	{
		if (tTd(29, 4))
			printf("name %s not found (fuzzy disabled)\n", name);
		return NULL;
	}
	if (tTd(29, 4))
		printf("looking for partial match to \"%s\"\n", name);

	(void) setpwent();
	while ((pw = getpwent()) != NULL)
	{
		char buf[MAXNAME + 1];
		register int this_match;

		if (strcasecmp(pw->pw_name, name) == 0) {

		    if (tTd(29, 4))
			printf("found password entry for \"%s\" as \"%s\"\n",
				   name, pw->pw_name);
		    *fuzzyp = TRUE;
		    return (pw);
		}

		buildfname(pw->pw_gecos, pw->pw_name, buf, sizeof buf);

		if ((this_match = partialstring(buf, name)) <= 0)
			continue;	/* don't accept partial word matches */

		if (tTd(29, 4 && this_match >= 0))
			printf("matched on level %d with \"%s\"\n",
			       this_match, buf);

		if (this_match < best_match)
			continue;
		else if (this_match > best_match) {
			best_match = this_match;
			best_uid = pw->pw_uid;
		} else if (best_uid != pw->pw_uid)
			best_uid = NO_UID;
	}

	if (tTd(29, 4))
		if (best_match == WORST_MATCH)
			printf("no match, failing...\n");
		else if (best_uid == NO_UID)
			printf("ambiguous match, failing...\n");
		else
			printf("succeding on level %d...\n",
			       best_match);

	if (best_uid == NO_UID)
		return (NULL);

	pw = getpwuid(best_uid);
	message("sending to login name %s", pw->pw_name);
	*fuzzyp = TRUE;
	return (pw);
#else
	return(NULL);
#endif
}
#endif



#if defined(SGI_EXTENSIONS) && defined(SGI_HOST_RES_ORDER)
#ifdef NAMED_BIND

bool
sgi_getcanonname(char *host, int hbsize, bool trymx)
{
	struct hostent *hp;

	/* This will look for hostname in the order      */
	/* defined by "hostresorder" in /etc/resolv.conf */
	hp = gethostbyname(host);
	if (hp == NULL) {
		if (trymx) {
        		char *mxhosts[MAXMXHOSTS + 1];
        		int rcode;
#if defined(SGI_EXTENSIONS) && defined(SGI_IGNOR_MX_SRCH_IN_HOST_MAP)
			u_long oldf;

			/* turn off search when doing */
			/* MX record lookup. 	      */
			/* mainly to avoid picking up */
			/* unwanted wildcard MX       */
			/* in non-local domain 	      */
			oldf = _res.options;
			_res.options &= ~RES_DNSRCH;
#endif
        		if (probemxrr(host, mxhosts, TRUE, &rcode) < 0) {
#if defined(SGI_EXTENSIONS) && defined(SGI_IGNOR_MX_SRCH_IN_HOST_MAP)
				_res.options = oldf;
#endif
                		return (FALSE);
			}
#if defined(SGI_EXTENSIONS) && defined(SGI_IGNOR_MX_SRCH_IN_HOST_MAP)
			_res.options = oldf;
#endif
			return(TRUE);
		}
		return (FALSE);
	}

	if (strlen(hp->h_name) >= hbsize)
		return (FALSE);

	(void) strcpy(host, hp->h_name);
	return (TRUE);
}

#endif /* NAMED_BIND */
#endif /* SGI_HOST_RES_ORDER */

#if defined(SGI_EXTENSIONS) && (defined(SGI_MAP_NAMESERVER) || defined(SGI_HOST_RES_ORDER)) || defined(SGI_PROBEMXRR)
#undef getmxrr

static int
chkconfig_named(void) {
	char buf[16];
	int fd;

	fd = open("/etc/config/named", O_RDONLY, 0);
	if (fd < 0) {
		return FALSE;
	}
	if (read(fd, buf, sizeof(buf)) < 0) {
		close(fd);
		return FALSE;
	}
	close(fd);
	if (strncasecmp(buf, "on", 2) == 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}

int
probemxrr(char *host, char **mxhosts, bool droplocalhost, int *rcode)
{
	int rr;
	static int no_resolvConf = 0;
	extern int h_errno;

	if (no_resolvConf == 0 && UseNameServer && 
	    access("/etc/resolv.conf", F_OK) == -1 && !chkconfig_named()) {
		UseNameServer = 0;
		no_resolvConf=1;
		sgi_mxrr_probe_only = 0;
	} else {
		no_resolvConf=2;
		sgi_mxrr_probe_only = 1;
	}

	rr = getmxrr(host, mxhosts, droplocalhost, rcode);
        sgi_mxrr_probe_only = 0;

	return (rr);
}

#endif /* SGI_MAP_NAMESERVER | SGI_HOST_RES_ORDER */


#if defined(SGI_EXTENSIONS) && defined(SGI_UNIXFROMLINE_SAME_AS_FROMHEADER)
char *
tidyaddr(char *addr)
{
        register char *p, *q, c;
	register int inquote, inangle, cmntcnt;

        if (!addr)
                return(NULL);
	
	p = q = addr;
	inquote = inangle = cmntcnt = 0;
	while(isascii(*q) && isspace(*q))
		q++;
	for (; (c = *q) != '\0'; q++) {
		switch (c)
		{
		  case '(':
			if (!inquote) {
				cmntcnt++;
				continue;
			}
			break;

		  case ')':
			if (!inquote) {
				if (cmntcnt-- <= 0)
					return(NULL);
				else
					continue;
			}
			break;

		  case '"':
			inquote = !inquote;
		}

		if (cmntcnt > 0)
			continue;
		if (inquote) {
			*p++ = c;
			continue;
		}

		switch (c)
		{
		  case '<':
			p = addr;
			inangle++;
			continue;

		  case '>':
			if (!inangle)
				return(NULL);
			/* fall through... */
		  case ',':
			if (p == addr)
				return(NULL);
			*p = '\0';
			return(p);

		  default:
			if (isascii(c) && isspace(c))
				continue;
			*p++ = c;
		}
	}

	if (p == addr)
		return(NULL);
	*p = '\0';
        return(p);
}
#endif /* SGI_UNIXFROMLINE_SAME_AS_FROMHEADER */

#if defined (EXTERNAL_LOAD_IF_NAMES)

#include <net/if.h>
#include <sys/sysctl.h>
#include <net/route.h>

void
load_if_names(void)
{
	int loop;
	size_t needed;
	char *buf, *next, *lim, *cp;
	struct if_msghdr *ifm;
	int mib[6];
	struct ifa_msghdr *ifam;
	struct sockaddr_in *mask, *addr;
	struct sockaddr *sa;
	extern void load_if_names_setclass __P((struct in_addr *));
	
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		if (tTd(0, 4))
			printf("failed first sysctl: %s\n", errstring(errno));
		return;
	}
	buf = malloc(needed);
	if (! buf) {
		if (tTd(0,4))
			printf("getnetconf: failed to malloc memory for sysctl\n");
		return;
	}
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		if (tTd(0, 4))
			printf("failed second sysctl: %s\n", errstring(errno));
		return;
	}

	if (tTd(0,40)) 
		printf("scanning interfaces via sysctl table\n");
	
	lim = buf + needed;
	for (next = buf; next < lim; next += ifm->ifm_msglen) {
		addr = mask = (struct sockaddr_in *)0;
		loop = 0;
		ifm = (struct if_msghdr *)next;

		ifam = (struct ifa_msghdr *)ifm;
		loop = (ifm->ifm_flags & IFF_LOOPBACK);

		if (loop) 
			continue;
		  
		if (ifam->ifam_addrs & RTA_NETMASK) {
			mask = (struct sockaddr_in *)(ifam + 1);
		}
		if (ifam->ifam_addrs & RTA_IFA) {
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
	    : sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif
			cp = (char *)mask;
			sa = (struct sockaddr *)mask;
			ADVANCE(cp, sa);
			addr = (struct sockaddr_in *)cp;
		}
		if (! addr)
			continue;

		load_if_names_setclass(&(addr->sin_addr));
	}
}

#endif /* EXTERNAL_LOAD_IF_NAMES */

#if defined(SGI_CHECKCOMPAT)
/* ARGSUSED */
int
sgi_checkcompat(ADDRESS *to, ENVELOPE *e)
{
	if (to != (ADDRESS *) NULL)
	{
		if (bitset (QGOODUID, to->q_flags))
		{
			char *user;

			user = to->q_ruser ? to->q_ruser : to->q_user;
			if (tTd(49, 1))
				printf ("Checking clearance of user %s\n",
					user);
			if (!mac_user_cleared (user, ProcLabel))
			{
#if defined(SGI_AUDIT)
				audit_sendmail (user, "warning: user not cleared for mail");
#endif
				return (EX_NOPERM);
			}
		}
	}
	else
	{
		if (tTd(49, 1))
			printf ("Checking clearance of user %s\n", DefUser);
		if (!mac_user_cleared (DefUser, ProcLabel))
		{
#if defined(SGI_AUDIT)
			audit_sendmail (DefUser,
					"warning: user not cleared for mail");
#endif
			return (EX_NOPERM);
		}
	}
	return (EX_OK);
}
#endif /* SGI_CHECKCOMPAT */

#if defined(SGI_SESMGR)
#include <t6net.h>

void
sesmgr_session(int fd)
{
	mac_t socket_label;

	/* get the label of incoming data */
	if (tsix_get_mac (fd, &socket_label) == -1)
	{
#if defined(SGI_AUDIT)
		audit_sendmail (RealUserName, "fatal: can't get socket label");
#endif
		exit(EX_SOFTWARE);
	}

	/* set our label to that of the incoming data */
	if (mac_proc_setlabel (socket_label) == -1)
	{
#if defined(SGI_AUDIT)
		audit_sendmail (RealUserName,
				"fatal: can't set process label");
#endif
		exit(EX_SOFTWARE);
	}
	mac_label_free (ProcLabel);
	ProcLabel = socket_label;

	/*
	 * set the label of outgoing data to
	 * that of incoming data.
	 */
	if (tsix_set_mac (fd, socket_label) == -1)
	{
#if defined(SGI_AUDIT)
		audit_sendmail (RealUserName,
				"fatal: can't change socket label");
#endif
		exit (EX_SOFTWARE);
	}
}

void
sesmgr_socket(int *fd)
{
	if (*fd != -1 && tsix_on(*fd) == -1)
	{
		close(*fd);
		*fd = -1;
	}
}
#endif /* SGI_SESMGR */

#if defined(SGI_EXTENSIONS) && defined(SGI_AUDIT)
#include <sat.h>

void
audit_sendmail (const char *user, const char *msg)
{
    int status;
    char *prog = "SENDMAIL";
    static int audit_enabled = -1;

    if (audit_enabled == -1)
	audit_enabled = (sysconf(_SC_AUDIT) > 0);
    if (!audit_enabled)
	return;

#if defined(SGI_CAP)
    status = cap_satvwrite(SAT_AE_CUSTOM, SAT_FAILURE, "%s|%c|%s|%s",
			   prog, '-',  user, msg);
#else
    status = satvwrite(SAT_AE_CUSTOM, SAT_FAILURE, "%s|%c|%s|%s",
		       prog, '-',  user, msg);
#endif
    if (status < 0)
	sm_syslog(LOG_ERR|LOG_AUTH, NOQID,"%s: satvwrite failure", prog);
}
#endif /* SGI_AUDIT */

#if defined(SGI_EXTENSIONS) && defined(SGI_CAP)
#include <sys/capability.h>
#include <grp.h>

int
cap_setuid(uid_t uid)
{
	cap_t ocap;
	const cap_value_t cap_setuid = CAP_SETUID;
	int r;

	ocap = cap_acquire(1, &cap_setuid);
	r = setuid(uid);
	cap_surrender(ocap);
	return(r);
}

int
cap_seteuid(uid_t uid)
{
	cap_t ocap;
	const cap_value_t cap_setuid = CAP_SETUID;
	int r;

	ocap = cap_acquire(1, &cap_setuid);
	r = seteuid(uid);
	cap_surrender(ocap);
	return(r);
}

int
cap_setreuid(uid_t ruid, uid_t euid)
{
	cap_t ocap;
	const cap_value_t cap_setuid = CAP_SETUID;
	int r;

	ocap = cap_acquire(1, &cap_setuid);
	r = setreuid(ruid, euid);
	cap_surrender(ocap);
	return(r);
}

int
cap_setgid(gid_t gid)
{
	cap_t ocap;
	const cap_value_t cap_setgid = CAP_SETGID;
	int r;

	ocap = cap_acquire(1, &cap_setgid);
	r = setgid(gid);
	cap_surrender(ocap);
	return(r);
}

int
cap_initgroups(char *name, gid_t group)
{
	cap_t ocap;
	const cap_value_t cap_setgid = CAP_SETGID;
	int r;

	ocap = cap_acquire(1, &cap_setgid);
	r = initgroups(name, group);
	cap_surrender(ocap);
	return(r);
}

int
cap_chroot(const char *path)
{
	cap_t ocap;
	const cap_value_t cap_chroot = CAP_CHROOT;
	int r;

	ocap = cap_acquire(1, &cap_chroot);
	r = chroot(path);
	cap_surrender(ocap);
	return(r);
}

int
cap_bind (int s, const struct sockaddr *name, int namelen)
{
	cap_t ocap;
	const cap_value_t cap_priv_port = CAP_PRIV_PORT;
	int r;

	ocap = cap_acquire(1, &cap_priv_port);
	r = bind(s, name, namelen);
	cap_surrender(ocap);
	return(r);
}

#if defined(SGI_AUDIT)
#include <sat.h>

int
cap_satvwrite(int event, int outcome, char *fmt, ...)
{
	cap_t ocap;
	const cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	int r;
	va_list ap;

	va_start(ap, fmt);
	ocap = cap_acquire(1, &cap_audit_write);
	r = satvwrite(event, outcome, fmt, ap);
	cap_surrender(ocap);
	va_end(ap);
	return(r);
}
#endif /* SGI_AUDIT */

#if defined(SGI_MAC)
#include <sys/mac.h>
#include <sys/attributes.h>

int
cap_mac_set_proc(void *l)
{
	cap_t ocap;
	const cap_value_t cap_mac_relabel_subj = CAP_MAC_RELABEL_SUBJ;
	int r;

	ocap = cap_acquire(1, &cap_mac_relabel_subj);
	r = mac_set_proc((mac_t) l);
	cap_surrender(ocap);
	return(r);
}

void *
cap_mac_get_file (const char *path)
{
	cap_t ocap;
	const cap_value_t cap_mac_read = CAP_MAC_READ;
	void *p;

	ocap = cap_acquire(1, &cap_mac_read);
	p = mac_get_file(path);
	cap_surrender(ocap);
	return(p);
}

int
cap_attr_get (const char *path, const char *name, char *value, int *len,
	      int flags)
{
	cap_t ocap;
	const cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	int r;

	ocap = cap_acquire(1, &cap_device_mgt);
	r = attr_get(path, name, value, len, flags);
	cap_surrender(ocap);
	return(r);
}

int
cap_attr_set (const char *path, const char *name, const char *value,
	      int len, int flags)
{
	cap_t ocap;
	const cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	int r;

	ocap = cap_acquire(1, &cap_device_mgt);
	r = attr_set(path, name, value, len, flags);
	cap_surrender(ocap);
	return(r);
}
#endif /* SGI_MAC */
#endif /* SGI_CAP */

#if defined(SGI_EXTENSIONS) && defined(SGI_MAC)
#include <sys/attributes.h>
#include <clearance.h>
#include <dirent.h>

mac_t ProcLabel;
int   TrustQ;

static double f;
static mac_t static_label = (mac_t) &f;
static mac_t QueueLabel;
static int mac_enabled = -1;

mac_t
mac_proc_getlabel (void)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    return (mac_enabled ? mac_get_proc () : static_label);
}

int
mac_proc_setlabel (mac_t label)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
#if defined(SGI_CAP)
    return (mac_enabled ? cap_mac_set_proc (label) : 0);
#else
    return (mac_enabled ? mac_set_proc (label) : 0);
#endif
}

mac_t
mac_dblow_label (void)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    return (mac_enabled ? mac_from_text ("dblow") : static_label);
}

mac_t
mac_dbadmin_label (void)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    return (mac_enabled ? mac_from_text ("dbadmin") : static_label);
}

mac_t
mac_wildcard_label (void)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    return (mac_enabled ? mac_from_text ("wildcard") : static_label);
}

void
mac_label_free (mac_t label)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    if (mac_enabled && label != (mac_t) NULL)
	(void) mac_free ((void *) label);
}

int
mac_label_dominate (mac_t labela, mac_t labelb)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    return (mac_enabled ? mac_dominate (labela, labelb) : 1);
}

int
mac_label_equal (mac_t labela, mac_t labelb)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    return (mac_enabled ? mac_equal (labela, labelb) : 1);
}

#define LEN_SUFFIX "_LEN"

int
mac_file_setattr (const char *file, const char *name, mac_t label)
{
    int r = 0;
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    if (mac_enabled)
    {
	int size, ssz = (int) sizeof (size);
	char namelen[256];

	size = (int) mac_size (label);
	if (size == -1)
	    return (-1);

	strcpy (namelen, name);
	strcat (namelen, LEN_SUFFIX);

#if defined(SGI_CAP)
	r = (cap_attr_set (file, namelen, (char *) &size, ssz, ATTR_ROOT) == -1 || cap_attr_set (file, name, (char *) label, size, ATTR_ROOT) == -1);
#else
	r = (attr_set (file, namelen, (char *) &size, ssz, ATTR_ROOT) == -1 || attr_set (file, name, (char *) label, size, ATTR_ROOT) == -1);
#endif
    }
    return (r ? -1 : 0);
}

mac_t
mac_file_getattr (const char *file, const char *name)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    if (mac_enabled)
    {
	int r, size, size_size = (int) sizeof (size), label_size;
	mac_t label;
	char namelen[256];

	strcpy (namelen, name);
	strcat (namelen, LEN_SUFFIX);

#if defined(SGI_CAP)
	r = cap_attr_get (file, namelen, (char *) &size, &size_size,
			  ATTR_ROOT);
#else
	r = attr_get (file, namelen, (char *) &size, &size_size, ATTR_ROOT);
#endif
	if (r == -1)
	    return ((mac_t) NULL);

	label = malloc ((size_t) (label_size = size));
	if (label == (mac_t) NULL)
	    return ((mac_t) NULL);
	
#if defined(SGI_CAP)
	r = cap_attr_get (file, name, (char *) label, &label_size, ATTR_ROOT);
#else
	r = attr_get (file, name, (char *) label, &label_size, ATTR_ROOT);
#endif
	if (r == -1 || mac_valid (label) <= 0)
	{
	    free ((void *) label);
	    return ((mac_t) NULL);
	}

	return (label);
    }
    errno = ENOSYS;
    return ((mac_t) NULL);
}

int
mac_user_cleared (const char *user, mac_t label)
{
    int r = 1;

    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    if (mac_enabled)
    {
	struct clearance *clp;

	clp = sgi_getclearancebyname (user);
	if (clp == (struct clearance *) NULL ||
	    mac_clearedlbl (clp, label) != MAC_CLEARED)
	{
	    r = 0;
	}
    }
    return (r);
}

mac_t
mac_swap (mac_t label)
{
    mac_t r;

    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    if (!mac_enabled)
	return ((mac_t) NULL);

    r = mac_get_proc ();
#if defined(SGI_CAP)
    if (cap_mac_set_proc (label) == -1)
#else
    if (mac_set_proc (label) == -1)
#endif
    {
	mac_free (r);
	r = NULL;
    }
    return (r);
}

void
mac_restore (mac_t label)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    if (!mac_enabled || label == (mac_t) NULL)
	return;

#if defined(SGI_CAP)
    (void) cap_mac_set_proc (label);
#else
    (void) mac_set_proc (label);
#endif
    mac_free (label);
}

int
q_dfopen(char *path, int flags, int m, int sff)
{
	int f;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	f = dfopen(path, flags, m, sff);
	if (TrustQ)
		mac_restore (oldlabel);
	return(f);
}

int
q_open(const char *path, int flags, mode_t m)
{
	int r;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	r = open(path, flags, m);
	if (TrustQ)
		mac_restore (oldlabel);
	return(r);
}

FILE *
q_fopen(const char *path, const char *flags)
{
	FILE *f;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	f = fopen(path, flags);
	if (TrustQ)
		mac_restore (oldlabel);
	return(f);
}

FILE *
q_freopen(const char *path, const char *flags, FILE *fp)
{
	FILE *f;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	f = freopen(path, flags, fp);
	if (TrustQ)
		mac_restore (oldlabel);
	return(f);
}

int
q_rename(const char *a, const char *b)
{
	int r;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	r = rename(a, b);
	if (TrustQ)
		mac_restore (oldlabel);
	return(r);
}

int
q_fstat(int fd, struct stat *stbuf)
{
	int r;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	r = fstat(fd, stbuf);
	if (TrustQ)
		mac_restore (oldlabel);
	return(r);
}

int
q_stat(const char *path, struct stat *stbuf)
{
	int r;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	r = stat(path, stbuf);
	if (TrustQ)
		mac_restore (oldlabel);
	return(r);
}

int
q_unlink(const char *path)
{
	int r;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	r = unlink(path);
	if (TrustQ)
		mac_restore (oldlabel);
	return(r);
}

int
q_chdir(const char *path)
{
	int r;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	r = chdir(path);
	if (TrustQ)
		mac_restore (oldlabel);
	return(r);
}

void *
q_opendir(const char *path)
{
	void *p;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	p = (void *) opendir(path);
	if (TrustQ)
		mac_restore (oldlabel);
	return(p);
}

mac_t
q_getmac(const char *path)
{
	mac_t oldlabel, m;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	m = mac_file_getattr(path, "SENDMAIL_MAC");
	if (TrustQ)
		mac_restore (oldlabel);
	return(m);
}

int
q_setmac(const char *path, mac_t m)
{
	int r;
	mac_t oldlabel;

	if (TrustQ)
		oldlabel = mac_swap (QueueLabel);
	r = mac_file_setattr(path, "SENDMAIL_MAC", m);
	if (TrustQ)
		mac_restore (oldlabel);
	return(r);
}

int
q_label(const char *path)
{
    if (mac_enabled == -1)
	mac_enabled = (sysconf (_SC_MAC) > 0);
    if (mac_enabled)
    {
	    mac_t AdminLabel;

	    /* get label of queue */
#if defined(SGI_CAP)
	    QueueLabel = cap_mac_get_file (path);
#else
	    QueueLabel = mac_get_file (path);
#endif
	    if (QueueLabel == NULL)
		    return (-1);

	    /* get administrative label */
	    AdminLabel = mac_from_text ("dbadmin");
	    if (AdminLabel == NULL)
	    {
		    mac_free(QueueLabel);
		    return(-1);
	    }
	    TrustQ = (mac_equal (QueueLabel, AdminLabel) > 0);
	    mac_free(AdminLabel);

	    return (0);
    }
    else
    {
	    QueueLabel = static_label;
	    TrustQ = 0;
	    return (0);
    }
}
#endif /* SGI_MAC */

#if defined(SGI_EXTENSIONS) && defined(SGI_MAP_NSD)
#define _DATUM_DEFINED
#include <ns_api.h>

ns_map_t *ns_aliases_map = 0;

char *
nsd_map_lookup(map, name, av, statp)
        MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char buf[MAXLINE];
	char *p;
	int buflen;
	char keybuf[MAXNAME + 1];
	
	if (tTd(38, 20))
		printf("nsd_map_lookup(%s, %s)\n",
			map->map_mname, name);

	buflen = strlen(name);
	if (buflen > sizeof keybuf - 1)
		buflen = sizeof keybuf - 1;
	bcopy(name, keybuf, buflen);
	keybuf[buflen] = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(keybuf);

	if (!ns_aliases_map) {
		ns_aliases_map = (ns_map_t *)calloc(1,sizeof(ns_map_t));
	}
		
	if (ns_lookup(ns_aliases_map, NULL, map->map_file, keybuf, NULL, 
		      buf, MAXLINE)) {
		return NULL;
	} else {
		/* 
		** Null out trailing \n
		*/
		if ((p = strchr(buf, '\n')) != NULL) {
			*p = '\0';
		}
		return map_rewrite(map,buf,strlen(buf),av);
	}
}
#endif
