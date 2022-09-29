#ident "$Revision: 1.7 $"

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <syslog.h>
#include "defs.h" 

char	cmd[MAX_STR_LEN];
char	*tfn = NULL;


void
errorexit(char *msg)
{
    if (tfn)
	unlink(tfn);
    if (isatty(fileno(stderr))) {
	fprintf(stderr, "%s: Error! %s\n", cmd, msg);
	exit(1);
    } else {
	openlog(cmd, LOG_PID, 0);
	syslog(LOG_WARNING, "%s\n", msg);
	exit(0);
    }
}

main(int argc, char *argv[])
{
    char 	filename[MAX_LINE_LEN], syscmd[MAX_LINE_LEN];
    char	line[MAX_LINE_LEN], *p, errmsg[MAX_LINE_LEN];
    char	flag = '\0';
    int		amrtype, usetmp, r;
    FILE	*fp;
    extern int	errno;

    strcpy(cmd, basename(argv[0]));
    if (argc == 1) {
	usetmp = 1;
    } else if (argc == 2) {
	usetmp = 1;
	if (!strcmp(argv[1], "-a"))
	    flag = 'A';
	else if (!strcmp(argv[1], "-d"))
	    flag = 'D';
	else if (!strcmp(argv[1], "-i"))
	    flag = 'I';
	else {
	    strcpy(filename, argv[1]);
	    usetmp = 0;
	}
    } else if (argc == 3) {
	if (!strcmp(argv[1], "-a"))
	    flag = 'A';
	else if (!strcmp(argv[1], "-d"))
	    flag = 'D';
	else if (!strcmp(argv[1], "-i"))
	    flag = 'I';
	else
	    goto error;
	strcpy(filename, argv[2]);
	usetmp = 0;
    } else {
      error:
	sprintf(errmsg, "incorrect arguments\n"
		"Usage: %s [-a|-d|-i] [ <input file> ]", cmd);
	errorexit(errmsg);
    }

    if (usetmp) {
	if ((tfn = tempnam(NULL, "avail")) == NULL) {
	    errorexit("cannot create temp file name");
	}
	if ((fp = fopen(tfn, "w")) == NULL) {
	    errorexit("cannot create temp file");
	}
	while (fgets(line, MAX_LINE_LEN, stdin))
	    fputs(line, fp);
	fclose(fp);
	strcpy(filename, tfn);
	fp = fopen(tfn, "r");
    } else {
	if ((fp = fopen(filename, "r")) == NULL) {
	    sprintf(errmsg, "cannot open email file %s", filename);
	    errorexit(errmsg);
	}
    }

    while (p = fgets(line, MAX_LINE_LEN, fp))
	if (strncmp("Subject:", line, 8) == 0)
	    break;

    if (p == NULL) {
	errorexit("not an Availmon report email");
    }

    if ((p = strstr(line, "AMR-")) == NULL) {
	errorexit("not an Availmon report email");
    }

    p += 4;
    if (flag) {
	/* filter out unwanted reports */
	if ((*p != flag) && ((*p != 'I') || (flag != 'D')))
	    goto out;
    }

    p += 2;
    if (*p == 'R')
	/* skip registration flag */
	p += 2;

    line[strlen(line) - 1] = '\0';
    if (!strcmp(p, "T")) {
	while (p = fgets(line, MAX_LINE_LEN, fp))
	    if (!strncmp("SYSID|", line, 6) || !strncmp("SERIALNUM|", line, 10))
		break;
	if (p) {
	    do {
		if (line[0] != '\n')
		    fputs(line, stdout);
	    } while (fgets(line, MAX_LINE_LEN, fp));
	}
	fclose(fp);
    } else if (!strcmp(p, "Z-X-U")) {
	fclose(fp);
	sprintf(syscmd, "/var/adm/avail/amuudecode %s | /usr/bin/crypt %s"
		"| /usr/bsd/uncompress", filename, AMR_CODE);
	system(syscmd);
    } else if (!strcmp(p, "Z-U")) {
	fclose(fp);
	sprintf(syscmd, "/var/adm/avail/amuudecode %s | /usr/bsd/uncompress",
		filename);
	system(syscmd);
    } else if (!strcmp(p, "X-U")) {
	fclose(fp);
	sprintf(syscmd, "/var/adm/avail/amuudecode %s | /usr/bin/crypt %s",
		filename, AMR_CODE);
	system(syscmd);
    } else {
	errorexit("not an Availmon report email");
    }

out:
    if (tfn)
	unlink(tfn);
}
