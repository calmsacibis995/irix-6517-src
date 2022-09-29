#ident "$Revision: 1.12 $"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <fcntl.h>
#include <signal.h>
#include "defs.h"

#define VISUAL			"/usr/bin/vi"

char	cmd[MAX_STR_LEN];
int	modified = 0;
char	cfile[MAX_STR_LEN];
char	*scfile;
char	*tfn = NULL;
char	syscmd[MAX_LINE_LEN];
int	sendtype[SEND_TYPE_NO];
char	addresses[SEND_TYPE_NO][MAX_LINE_LEN];
char	addresses1[SEND_TYPE_NO][MAX_LINE_LEN];

typedef struct {
    char	*addrp[200];
} addrp_t;

addrp_t	addr[SEND_TYPE_NO], addr1[SEND_TYPE_NO];

void	setconfig(amconf_t flag, int state);
int	getstatusinterval(char *cfile);
void	setstatusinterval(char *cfile, int statusinterval);
void	getemailaddresses(char *cfile);
void	showaddresses();
void	editaddresses(char *cfile);
void	sortaddresses(addrp_t addr[]);
void	compare();

main(int argc, char *argv[])
{
    int		i, c, state, type;
    char	line[ MAX_LINE_LEN];
    FILE	*fp, *fp1;

    strcpy(cmd, basename(argv[0]));
    strcpy(cfile, amcs[amc_autoemaillist].filename);

    /* process online options */
    switch (argc) {
    case 1:
	printf("\tAvailMon Flag       State\n"
	       "\t=============       =====\n\n");
	for (i = 0; i < amc_statusinterval; i++) {
	    printf("\t%-20s", amcs[i].flagname);
	    if (chkconfig(i, 0))
		printf("on\n");
	    else
		printf("off\n");
	}

	printf("\t%-20s%d (days)\n", amcs[i].flagname,
	       getstatusinterval(amcs[amc_statusinterval].filename));
	getemailaddresses(amcs[amc_autoemaillist].filename);
	showaddresses();
	return(0);
    case 2:
	if (!strcmp(argv[1], "-k"))
	    goto check;
	i = getconfig(argv[1]);
	if (i < 0 || i >= amc_num) {
	    goto error;
	} else if (i < amc_statusinterval) {
	    return (!chkconfig(i, 0));
	} else if (i == amc_statusinterval) {
	    return (getstatusinterval(amcs[amc_statusinterval].filename));
	}
	break;
    case 3:
	i = getconfig(argv[1]);
	if (i < 0 || i >= amc_autoemaillist)
	    goto error;

	if (getuid() != 0) {
	    fprintf(stderr, "Error: %s: must be superuser to set configuration"
		    " flags.\n", cmd);
	    exit(1);
	}

	if (i < amc_statusinterval) {
	    if (!strcmp(argv[2], "-s")) {
		return (!chkconfig(i, 1));
	    } else if (!strcmp(argv[2], "on"))
		state = 1;
	    else if (!strcmp(argv[2], "off"))
		state = 0;
	    else
		goto error;
	    setconfig((amconf_t)i, state);
	} else {
	    if (!strcmp(argv[2], "-s")) {
		return (getstatusinterval(samcs[amc_statusinterval].filename));
	    } else {
		c = atol(argv[2]);
		setstatusinterval(amcs[amc_statusinterval].filename, c);
	    }
	}

	return 0;
    default:
      error:
	fprintf(stderr, "Usage: %s [{autoemail|shutdownreason|tickerd|"
		"hinvupdate|livenotification} [{on|off}]]\n", cmd);
	fprintf(stderr, "       %s autoemail.list\n", cmd);
	fprintf(stderr, "       %s statusinterval [<days>]\n", cmd);
	exit(1);
    }

    if (getuid() != 0) {
        fprintf(stderr, "Error: %s: must be superuser to change configuration"
		" file\n", cmd);
	exit(1);
    }
    if ((fp = fopen(cfile, "a")) == NULL) {
	fprintf(stderr, "Error: %s: cannot write configuration file %s\n",
		cmd, cfile);
	exit(1);
    }
    fclose(fp);
    getemailaddresses(cfile);
    showaddresses(addresses);
    printf("\nDo you want to modify the addresses? [y/n] (n) ");
    gets(line);
    while (line[0] == 'y' || line[0] == 'Y') {
	editaddresses(cfile);
	getemailaddresses(cfile);
	showaddresses();
	printf("\nDo you want to modify the addresses? [y/n] (n) ");
	gets(line);
    }

  check:
    scfile = samcs[amc_autoemaillist].filename;
    if (chkconfig(amc_autoemail, 0)) {
	getemailaddresses(cfile);
	sortaddresses(addr);
	getemailaddresses(scfile);
	sortaddresses(addr1);
	compare();
    }
    if ((fp = fopen(cfile, "r")) == NULL) {
	fprintf(stderr, "Error: %s: cannot open configuration file %s\n",
		cmd, cfile);
	exit(1);
    }
    if ((fp1 = fopen(scfile, "w")) == NULL) {
	fprintf(stderr, "Error: %s: cannot open saved configuration file %s\n",
		cmd, scfile);
	exit(1);
    }
    while (fgets(line, MAX_LINE_LEN, fp))
	fputs(line, fp1);
    fclose(fp);
    fclose(fp1);
}

int
getconfig(char *flag)
{
    int		i;

    for (i = 0; i < amc_num; i++)
	if (!strcmp(flag, amcs[i].flagname))
	    return(i);

    return(-1);
}

/*
 * check whether config flag is turned on
 */
int
chkconfig(amconf_t flag, int sflag)
{
    amc_t	*amc;
    char	line[MAX_LINE_LEN];
    FILE 	*fp;

    if (sflag)
	amc = samcs;
    else
	amc = amcs;
    if ((fp = fopen(amc[flag].filename, "r")) == NULL)
	return 0; /* no file == flag is off */
    /*
     * check "on" or "off".
     */
    if ((fgets(line, MAX_LINE_LEN, fp) == NULL) ||
	(strncasecmp(line, "on", 2) != 0)) {
	    fclose(fp);
	    return 0;
	}
    fclose(fp);
    return 1;
}

void
setconfig(amconf_t flag, int state)
{
    char	*cfile = amcs[flag].filename;
    char	*tfile = "/var/adm/avail/lasttick";
    FILE 	*fp;

    if (flag != amc_autoemail)
	if (chkconfig(flag, 0) == state)
	    return;

    if ((fp = fopen(cfile, "w")) == NULL) {
	fprintf(stderr, "Error: %s: cannot write configuration file %s\n",
		cmd, cfile);
	exit(1);
    }

/*-------------------------------------------------------------------*/
/* Changed by : sri@csd,                                             */
/* commented line :   fprintf(fp, "%s\n", state ? "on" : "off");     */
/*                    and added it in relevent 'if' conditions.      */
/*                                                                   */
/* symptom :If someone runs 'amconfig autoemail on' and if there     */
/*          is no machine readable serial number, amregister comes   */
/*          out with an error message.  However,  amconfig updates   */
/*          configuration file 'autoemail' with the value 'on'.      */
/*          consequences are, when the machine is rebooted, there    */
/*          is a possibility that reports could be mailed without    */
/*          a registration report.                                   */
/*          This behaviour is incorrect.                             */
/*-------------------------------------------------------------------*/
/*  fprintf(fp, "%s\n", state ? "on" : "off");                       */

    if (flag == amc_autoemail) {
	if (state == 1) {
	    if ( (system("/var/adm/avail/amregister -r")) == 0 ) 
	        fprintf(fp, "%s\n", "on");
	} else {
	    fprintf(fp, "%s\n", "off");
	    system("/var/adm/avail/amregister -d");
        }
    } else if (flag == amc_tickerd) {
	fprintf(fp, "%s\n", state ? "on" : "off");
	if (state == 1)
	    system("/var/adm/avail/amtickerd /var/adm/avail/lasttick 300");
	else
	    system("killall amtickerd ; rm -f /var/adm/avail/lasttick");
    } else
	fprintf(fp, "%s\n", state ? "on" : "off");

    fclose(fp);
}

int
getstatusinterval(char *cfile)
{
    FILE	*fp;
    int		statusinterval;

    if ((fp = fopen(cfile, "r")) == NULL) {
	fprintf(stderr, "Error: %s: cannot open configuration file %s\n",
		cmd, cfile);
	exit(1);
    }

    fscanf(fp, "%d", &statusinterval);
    fclose(fp);
    return statusinterval;
}

void
setstatusinterval(char *cfile, int statusinterval)
{
    FILE	*fp;
    int		oldstatusinterval;


    if ((fp = fopen(cfile, "r")) == NULL) {
	fprintf(stderr, "Error: %s: cannot open configuration file %s\n",
		cmd, cfile);
	exit(1);
    }

    fscanf(fp, "%d", &oldstatusinterval);
    fclose(fp);

    if ((fp = fopen(cfile, "w")) == NULL) {
	fprintf(stderr, "Error: %s: cannot open configuration file %s\n",
		cmd, cfile);
	exit(1);
    }

    fprintf(fp, "%d\n", statusinterval);
    fclose(fp);
    if (chkconfig(amc_tickerd, 0)) {
	if (oldstatusinterval != statusinterval) {
	    system("killall amtickerd ; rm -f /var/adm/avail/lasttick");
	    system("/usr/etc/amtickerd /var/adm/avail/lasttick 60");
	}
    } else {
	fprintf(stderr, "Warning: availmon status report will be sent only when"
		" tickerd is on.\nPlease use amconfig to turn tickerd on\n");
    }
}

void
getemailaddresses(char *cfile)
{
    FILE	*fp;
    char	line[ MAX_LINE_LEN], *ap, *dp;
    int		type, c, len[SEND_TYPE_NO], newaddr;

    if ((fp = fopen(cfile, "r")) == NULL) {
	fprintf(stderr, "Error: %s: cannot open configuration file %s\n",
		cmd, cfile);
	exit(1);
    }

    for (type = 0; type < SEND_TYPE_NO; type++) {
	addresses[type][0] = '\0';
	sendtype[type] = 0;
	len[type] = strlen(sendtypestr[type]);
    }

    while (fgets(line, MAX_LINE_LEN, fp)) {
	if (line[0] == '#')
	    continue;

	for (type = 0; type < SEND_TYPE_NO; type++)
	    if (strncmp(line, sendtypestr[type], len[type]) == 0)
		break;

	if (type == SEND_TYPE_NO)
	    continue;

	ap = line + len[type];
	dp = &addresses[type][sendtype[type]];
	newaddr = 1;
	while ((c = *ap++) != '\n') {
	    if ((c == '\0') || (c == '#'))
		break;
	    if ((c == '\t') || (c == ' ') || (c == ',')) {
		newaddr = 1;
		continue;
	    }
	    if (newaddr) {
		newaddr = 0;
		*dp++ = ' ';
		sendtype[type]++;
	    }
	    *dp++ = c;
	    sendtype[type]++;
	}
	*dp++ = ' ';
	*dp = '\0';
    }
    fclose(fp);
}

void
showaddresses()
{
    printf("\nThe availability report will be sent to:\n");
    printf("\tin compressed and encrypted form: %s\n", addresses[0]);
    printf("\tin compressed form: %s\n", addresses[1]);
    printf("\tin plain text form: %s\n", addresses[2]);

    printf("\nThe diagnosis report will be sent to:\n");
    printf("\tin compressed and encrypted form: %s\n", addresses[3]);
    printf("\tin compressed form: %s\n", addresses[4]);
    printf("\tin plain text form: %s\n", addresses[5]);
    printf("\nThe pager report will be sent to:\n");
    printf("\tin concise text form: %s\n", addresses[6]);
}

/*
 * Edit the email list configuration file
 */
void
editaddresses(char *cfile)
{
    int		pid, s, t;
    FILE	*fp, *fp1;
    char	*edit = VISUAL;
    void	(*sig)(), (*scont)(), signull();

    sig = sigset(SIGINT, SIG_IGN);

    /* fork an editor on the message */
    pid = fork();
    if (pid == 0) {
	/* CHILD - exec the editor. */
	if (sig != SIG_IGN)
	    signal(SIGINT, SIG_DFL);
	execl(edit, edit, cfile, 0);
	perror(edit);
	exit(1);
    }
    if (pid == -1) {
	/* ERROR - issue warning and get out. */
	perror("fork");
	unlink(cfile);
	goto out;
    }
    /* PARENT - wait for the child (editor) to complete. */
    while (wait(&s) != pid)
	;

    /* Check the results. */
    if ((s & 0377) != 0) {
	printf("Fatal error in \"%s\"\n", edit);
	unlink(cfile);
    }

  out:
    sigset(SIGINT, sig);
}

int
addresscompare(const void *address1, const void *address2)
{
    return(strcmp(*(char **)address1, *(char **)address2));
}

void
sortaddresses(addrp_t addr[])
{
    int		type, n, len, i;
    char	*ap, *dp, **addrps;

    for (type = 0; type < SEND_TYPE_NO; type++) {
	n = 0;
	addrps = addr[type].addrp;
	if (sendtype[type]) {
	    ap = &addresses[type][1];
	    while (dp = strchr(ap, ' ')) {
		*dp = '\0';
		len = strlen(ap);
		addrps[n] = malloc(len + 1);
		strcpy(addrps[n], ap);
		n++;
		ap = dp + 1;
	    }
	}
	addrps[n] = NULL;
	if (n)
	    qsort(addrps, n, sizeof(char *), addresscompare);
    }
}

void
compare()
{
    int		type, i, adds, deletes;
    char	*addrp, *addrp1, **ap, **ap1;
    FILE	*fp;

    adds = deletes = 0;
    for (type = 0; type < SEND_TYPE_NO - 1; type++) {
	addrp = addresses[type];
	addrp1 = addresses1[type];
	*addrp = '\0';
	*addrp1 = '\0';
	ap = addr[type].addrp;
	ap1 = addr1[type].addrp;
	while (*ap && *ap1) {
	    i = strcmp(*ap, *ap1);
	    if (i == 0) {
		ap++;
		ap1++;
	    } else if (i < 0) {
		strcat(addrp, " ");
		strcat(addrp, *ap);
		adds++;
		ap++;
	    } else {
		strcat(addrp1, " ");
		strcat(addrp1, *ap1);
		deletes++;
		ap1++;
	    }
	}
	while (*ap) {
	    strcat(addrp, " ");
	    strcat(addrp, *ap);
	    adds++;
	    ap++;
	}
	while (*ap1) {
	    strcat(addrp1, " ");
	    strcat(addrp1, *ap1);
	    deletes++;
	    ap1++;
	}
    }
    if (tfn == NULL) {
	if ((tfn = tempnam(NULL, "avail")) == NULL) {
	    fprintf(stderr, "Error: cannot create temp file name\n");
	    exit(1);
	}
	if ((fp = fopen(tfn, "w")) == NULL) {
	    fprintf(stderr, "Error: cannot create temp file\n");
	    exit(1);
	}
	fclose(fp);
    }
    if (adds) {
	fp = fopen(tfn, "w");
	for (type = 0; type < SEND_TYPE_NO - 1; type++) {
	    if (strlen(addresses[type]) > 980) {
		fprintf(stderr, "Error: %s: add too many addresses in one "
			"email  list.  Please use aliases to shorten the "
			"length.\n", cmd);
		fclose(fp);
		unlink(tfn);
		exit(-1);
	    }
	    fprintf(fp, "%s %s\n", sendtypestr[type], addresses[type]);
	}
	fclose(fp);
	sprintf(syscmd, "/var/adm/avail/amregister -r -c %s", tfn);
	system(syscmd);
    }
    if (deletes) {
	fp = fopen(tfn, "w");
	for (type = 0; type < SEND_TYPE_NO - 1; type++) {
	    if (strlen(addresses1[type]) > 980) {
		fprintf(stderr, "Error: %s: delete too many addresses in one "
			"email  list.  Please use aliases to shorten the "
			"length.\n", cmd);
		fclose(fp);
		unlink(tfn);
		exit(-1);
	    }
	    fprintf(fp, "%s %s\n", sendtypestr[type], addresses1[type]);
	}
	fclose(fp);
	sprintf(syscmd, "/var/adm/avail/amregister -d -c %s", tfn);
	system(syscmd);
    }
    if (tfn)
	unlink(tfn);
}
