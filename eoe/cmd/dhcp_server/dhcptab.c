#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <strings.h>
#include <libgen.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "dhcp_common.h"
#include "dhcptab.h"

#ifndef uint32
# define uint32 unsigned int
#endif

extern EtherAddr *ether_aton(char *);
char *ether_ntoa(EtherAddr *e);

extern FILE *log_fopen(char *, char *);
extern uint32 hash_buf(u_char *str, int len);


typedef struct dtab_ent {
    struct dhcpmacs m;
    struct dtab_ent *link;
} *dtab_list;

static FILE *dtabfp = 0;
static long	dtab_modtime;	/* last modification time of dhcpmtab */
static int ndhcpmacs = 0;

#define DHCPMTAB "/var/dhcp/config/dhcpmtab"

/* linked list to store regexes */
static dtab_list regex_dtab_list = 0;

/* hash structure to store other entries */
struct dtab_key {
    u_char htype;
    u_char haddr[16];
};

#define DTAB_BUCKETS 128

static dtab_list dtabm[DTAB_BUCKETS];

extern int debug;

/* This function cleans up the hash table consisting of the MAC address
 * entries
 */
void
cleanup_dtabm(void)
{
    struct dtab_ent *p, *tmp;
    int i;

    for (i = 0; i < DTAB_BUCKETS; i++) {
	for (p = dtabm[i]; p ; p = tmp) {
	    tmp = p->link;
	    free(p);
	}
	dtabm[i] = 0;
    }
}

/* This function cleans up the regular expression entries
 */
void
cleanup_dtabr(void)
{
    struct dtab_ent *p, *tmp;

    for (p = regex_dtab_list; p ; p = tmp) {
	tmp = p->link;
	if (p->m.regex_mac)
	    free(p->m.regex_mac);
	free(p);
    }
}

/* Insert an entry into the list
 */
int
insertdtab(dtab_list *p_dtab_list, struct dhcpmacs *m)
{
    struct dtab_ent *p = (struct dtab_ent*) malloc(sizeof(struct dtab_ent));
    if (p == (struct dtab_ent*)0)
	return -1;
    bcopy(m, &p->m, sizeof(struct dhcpmacs));
    p->link = *p_dtab_list;
    *p_dtab_list = p;
    return 0;
}
/* Insert an entry into the hash table
 */
int 
insert_dtab(struct dhcpmacs *m)
{
    struct dtab_key a_dtab_key;
    int ret = 0;

    a_dtab_key.htype = m->htype;
    bcopy(m->haddr, a_dtab_key.haddr, sizeof(a_dtab_key.haddr));

    if (insertdtab(&dtabm[hash_buf((u_char*)&a_dtab_key, 
				   sizeof(struct dtab_key)) % DTAB_BUCKETS], m)
	!= 0) {
	ret = -1;
    }
    return ret;

}

/*
 * The source file will consist of space or tab separated fields consisting
 * of length (zero if any length), type, macaddress 
 * (in the format that is accepted by ether_aton), action, and command 
 * to execute. The action field consists of one or more of K|L|E
 */
void
read_dtab(void)
{
    struct stat st;
    struct dhcpmacs dm;
    struct dhcpmacs *dmp;
    int		linenum;
    char	line[256];	/* line buffer for reading bootptab */
    char	*linep;		/* pointer to 'line' */
    int		i;
    EtherAddr *ethp;
    char * action;
    int tmp;

    if (dtabfp == 0) {
	if ((dtabfp = log_fopen(DHCPMTAB, "r")) == NULL) {
	    syslog(LOG_ERR, "can't open %s (%m)", DHCPMTAB);
	    exit(1);
	}
    }
    fstat(fileno(dtabfp), &st);
    if (st.st_mtime == dtab_modtime && st.st_nlink) 
	return;	/* hasnt been modified or deleted yet */
    fclose(dtabfp);
    if ((dtabfp = log_fopen(DHCPMTAB, "r")) == NULL) {
	syslog(LOG_ERR, "can't reopen %s (%m)", DHCPMTAB);
	exit(1);
    }
    fstat(fileno(dtabfp), &st);
    if (debug)
	syslog(LOG_DEBUG, "(re)reading %s", DHCPMTAB);
    dtab_modtime = st.st_mtime;
    dmp = &dm;
    ndhcpmacs = 0;
    cleanup_dtabm();		/* plain */
    cleanup_dtabr();		/* regex */
    linenum = 0;

    /*
     * read and parse each line in the file.
     */
    for (;;) {
	if (fgets(line, sizeof line, dtabfp) == NULL)
	    break;	/* done */
	if ( (i = strlen(line)) && (line[i-1] == '\n') )
	    line[i-1] = 0;	/* remove trailing newline */
	linep = line;
	linenum++;
	if (line[0] == '#' || line[0] == 0 || line[0] == ' ')
	    continue;	/* skip comment lines */
	bzero(dmp, sizeof(struct dhcpmacs));
	dmp->line = linenum;
	linep = strtok(line, " \t");
	if ((!linep) || (sscanf(linep, "%i", &tmp) != 1)) {
	    syslog(LOG_ERR, "bad length (%s) on line %d", linep, linenum);
	    continue;
	}
	dmp->hlen = (u_char)tmp;
	linep = strtok(0, " \t");
	if ((!linep) || (sscanf(linep, "%i", &tmp) != 1)) {
	    syslog(LOG_ERR, "bad type (%s) on line %d", linep, linenum);
	    continue;
	}
	dmp->htype = (u_char)tmp;
	linep = strtok(0, " \t");
	if (!linep) {
	    syslog(LOG_ERR, "bad address (%s) on line %d", linep, linenum);
	    continue;
	}
	switch (dmp->htype) {
	case (TYPE_ETHER):
	case (TYPE_FDDI):
	    if (dmp->hlen == sizeof(EtherAddr)) {
		ethp = ether_aton(linep);
		if (ethp) {
		    bcopy(ethp, dmp->haddr, sizeof(EtherAddr));
		    dmp->regex_mac = 0;
		}
		else {
		    syslog(LOG_ERR, "Error in mac address (%s) on line %d",
			   linep, linenum);
		    continue;
		}
	    }
	    else if ((dmp->hlen == 0) || (dmp->hlen > sizeof(dmp->haddr))) {
				/* this is a regex */
		dmp->regex_mac = regcmp(linep, (char*)0);
		if (dmp->regex_mac == NULL) {
		    syslog(LOG_ERR, "Error in mac address regex (%s) on line %d",
			   linep, linenum);
		    continue;
		}
	    }
	    else {
		syslog(LOG_ERR, "Error in mac address (%s) on line %d",
		       linep, linenum);
		continue;
	    }
	    break;
	default:
	    syslog(LOG_ERR, "This type is not supported use ETHER=1, FDDI=2");
	    break;
	}
	linep = strtok(0, " \t");
	if (!linep) {
	    syslog(LOG_ERR, "bad action (%s) on line %d", linep, linenum);
	    continue;
	}
	action = linep;
	linep = strtok(0, "\n");
	/* 
	if (linep) {
	    strcpy(dmp->exec_cmd, linep);
	    tmp = strlen(linep);
	    if (dmp->exec_cmd[tmp-1] == '&') {
		dmp->exec_cmd[tmp-1] = '\0';
		dmp->background = 1;
	    }
	    else
		dmp->background = 0;
	}
	*/
	dmp->flags = 0;
	linep = action;
	action = strtok(linep, "|");
	if (*action) {
	    do {
		*action = toupper(*action);
		if (*action == 'K')
		    dmp->flags |= DROP_REQ;
		else if (*action == 'L')
		    dmp->flags |= LOG_REQ;
		/*
		else if (*action == 'E')
		    dmp->flags |= EXEC_REQ;
		*/
		else {
		    syslog(LOG_ERR, "Unknow action type %s on line %d",
			   action, linenum);
		}
	    } while (action = strtok(0, "|"));
	}
	/* if this is a regex store this on a link list  */
	ndhcpmacs++;
	if (dmp->regex_mac) {
	    if (insertdtab(&regex_dtab_list, dmp) != 0) {
		syslog(LOG_ERR, "Error inserting entry on line %d", linenum);
		exit(1);
	    }
	}
	else {
	    if (insert_dtab(dmp) != 0) {
		syslog(LOG_ERR, "Error inserting entry on line %d", linenum);
		exit(1);
	    }
	}
    }
}

/* Lookup into the hash table using the mac type and address as the key
 * the length should also match
 */
struct dhcpmacs*
lookupdtabm(dtab_list *p_dtab_list, struct dtab_key* k, int len)
{
    struct dtab_ent *p = *p_dtab_list;

    if (p == (struct dtab_ent*)0)
	return (struct dhcpmacs*)0;
    for (;p;p = p->link) {
	if ((k->htype == p->m.htype) && (p->m.hlen == len) &&
	    (bcmp(k->haddr, p->m.haddr, len) == 0))
	    return &p->m;
    }
    return (struct dhcpmacs*)0;
}

/* Check for a matching reg expression
 */
struct dhcpmacs*
lookupdtabregex(u_char len, u_char type, u_char *chaddr)
{
    struct dtab_ent *p = regex_dtab_list;

    if (p == (struct dtab_ent*)0)
	return (struct dhcpmacs*)0;
    for (;p;p = p->link) {
	if ((type == p->m.htype) && 
	    (regex(p->m.regex_mac, ether_ntoa((EtherAddr*)chaddr)) != (char*)0)) {
	    if ((p->m.hlen != 0) && (p->m.hlen <= sizeof(p->m.haddr))) {
		if (len == p->m.hlen)
		    return &p->m;
	    }
	    else
		return &p->m;
	}
    }
    return (struct dhcpmacs*)0;
}


/* 
 * returns non-zero to continue with the request
 * returns 0 to drop the request
 */
int
lookup_dtab(u_char len, u_char type, u_char *chaddr)
{
    /* First lookup in the hash table for a match return 
     * then if a match is not found look up in the regexp link list
     * Take the action on the match. Return 0 to proceed with this 
     * request and 1 to drop the request 
     */
    struct dtab_key a_dtab_key;
    struct dhcpmacs * dhcpmacp;
    int ret = 0;

    a_dtab_key.htype = type;
    bzero(a_dtab_key.haddr, sizeof(a_dtab_key.haddr));
    bcopy(chaddr, a_dtab_key.haddr, len);
    dhcpmacp = lookupdtabm(&dtabm[hash_buf((u_char*)&a_dtab_key,
					   sizeof(struct dtab_key)) % 
				 DTAB_BUCKETS],
			   &a_dtab_key, len);
    if (dhcpmacp == (struct dhcpmacs*)0) {
	dhcpmacp = lookupdtabregex(len, type, chaddr);
    }
    if (dhcpmacp) {
	/* take the necessary actions */
	/*
	cmdbuf[0] = '\0';
	if (dhcpmacp->flags & EXEC_REQ) {
	    sprintf(cmdbuf, "%s -M %s", dhcpmacp->exec_cmd, 
		    ether_ntoa((EtherAddr*)chaddr));
	    if (dhcpmacp->background)
		strcat(cmdbuf, " &");
	    system(cmdbuf);
	}
	*/
	if (dhcpmacp->flags & DROP_REQ) {
	    ret = 1;
	}
	if (dhcpmacp->flags & LOG_REQ) {
	    syslog(LOG_INFO, "DHCP packet from MAC=%s, htype=%d, drop=%d",
		   ether_ntoa((EtherAddr*)chaddr), (int)type, 
		   (int)(dhcpmacp->flags & DROP_REQ) );
	}
    }
    return ret;
}
