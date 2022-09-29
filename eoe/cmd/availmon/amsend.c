#ident "$Revision: 1.5 $"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <fcntl.h>
#include "defs.h" 

char	cmd[MAX_STR_LEN];
int	cflag = 0, aflag = 0, dflag = 0, iflag = 0, rflag = 0;
char	cfile[MAX_STR_LEN] = "/var/adm/avail/config/autoemail.list";
char	afile[MAX_STR_LEN] = "/var/adm/crash/availreport";
char	dfile[MAX_STR_LEN] = "/var/adm/crash/diagreport";
char	*repfile[SEND_TYPE_NO];
char	*fileflag[SEND_TYPE_NO];
int	sendtype[SEND_TYPE_NO];
char	addresses[SEND_TYPE_NO][MAX_LINE_LEN];
char	syscmdseed[3][MAX_LINE_LEN];
char	*syscmd[SEND_TYPE_NO];
int	usefile = 0, usetmp = 0, compress = 0, doencrypt = 0;
char 	sendaddresses[MAX_LINE_LEN];

void	getemailaddresses(char *cfile);


main(int argc, char *argv[])
{
    char 	filename[MAX_LINE_LEN], fflag[MAX_STR_LEN];
    char	line[MAX_LINE_LEN], syscmdline[MAX_LINE_LEN], *tfn = NULL;
    int		c, errflg = 0, type;
    FILE 	*fp;

    strcpy(cmd, basename(argv[0]));

    /* process online options */
    while ((c = getopt(argc, argv, "adirzxc:f:")) != EOF)
	switch (c) {
	case 'a':
	    if (aflag || dflag || iflag)
		errflg++;
	    else
		aflag = 1;
	    if (!usefile)
		strcpy(filename, afile);
	    break;
	case 'd':
	    if (aflag || dflag || iflag)
		errflg++;
	    else
		dflag = 1;
	    if (!usefile)
		strcpy(filename, dfile);
	    break;
	case 'i':
	    if (aflag || dflag || iflag)
		errflg++;
	    else
		iflag = 1;
	    if (!usefile)
		strcpy(filename, dfile);
	    break;
	case 'r':
	    if (rflag)
		errflg++;
	    else
		rflag = 1;
	    break;
	case 'z':
	    compress = 1;
	    break;
	case 'x':
	    doencrypt = 1;
	    break;
	case 'c':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else if (cflag)
		errflg++;
	    else {
		cflag = 1;
		strcpy(cfile, optarg);
	    }
	    break;
	case 'f':
	    if (optind - 1 == argc)
		/* no file name was specified */
		errflg++;
	    else if (usefile)
		errflg++;
	    else if (optarg[0] == '-')
		usetmp = 1;
	    else
		strcpy(filename, optarg);
	    usefile = 1;
	    break;
	case '?':
	    errflg++;
	}

    if (errflg) {
      error:
	fprintf(stderr, "Usage: %s [-a|-d|-i] [-f <file>] [-z] [-x] "
		"[-c <config file>] <address> ...\n", cmd);
	exit(-1);
    }

    sendaddresses[0] = '\0';
    for (; optind < argc; optind++) {
	strcat(sendaddresses, " ");
	strcat(sendaddresses, argv[optind]);
    }

    if (aflag || dflag || iflag) {
	usefile = 1;
    }

    if (usefile) {
	if (aflag)
	    strcpy(fflag, "A");
	else if (dflag)
	    strcpy(fflag, "D");
	else if (iflag)
	    strcpy(fflag, "I");
	else
	    goto error;

	if (rflag)
	    strcat(fflag, "-R");

	if (usetmp) {
	    if ((tfn = tempnam(NULL, "avail")) == NULL) {
		fprintf(stderr, "Error: cannot create temp file name\n");
		exit(-1);
	    }
	    if ((fp = fopen(tfn, "w")) == NULL) {
		fprintf(stderr, "Error: cannot create temp file\n");
		exit(-1);
	    }
	    while (fgets(line, MAX_LINE_LEN, stdin))
		fputs(line, fp);
	    fclose(fp);
	    strcpy(filename, tfn);
	} else {
	    if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Error: cannot open report file %s\n", filename);
		exit(-1);
	    }
	    fclose(fp);
	}

	if (compress && doencrypt)
	    sprintf(syscmdline, "/usr/bsd/compress -f -c %s |"
		    "/usr/bin/crypt %s | /usr/bsd/uuencode %s |"
		    "/usr/sbin/Mail -s AMR-%s-Z-X-U %s\n",
		    filename, AMR_CODE, AMR_EN_FILE, fflag, sendaddresses);
	else if (!compress && !doencrypt)
	    sprintf(syscmdline, "/usr/bin/cat %s | /usr/sbin/Mail "
		    "-s AMR-%s-T %s\n", filename, fflag, sendaddresses);
	else if (!doencrypt)
	    sprintf(syscmdline, "/usr/bsd/compress -f -c %s |"
		    "/usr/bsd/uuencode %s | /usr/sbin/Mail -s AMR-%s-Z-U %s\n",
		    filename, AMR_EN_FILE, fflag, sendaddresses);
	else
	    sprintf(syscmdline, "/usr/bin/cat %s | /usr/bin/crypt %s |"
		    "/usr/bsd/uuencode %s | /usr/sbin/Mail -s AMR-%s-X-U %s\n",
		    filename, AMR_CODE, AMR_EN_FILE, fflag, sendaddresses);

	system(syscmdline);
	if (tfn)
	unlink(tfn);
    } else {
	if ((fp = fopen(afile, "r")) == NULL) {
	    fprintf(stderr, "Error: %s: cannot open avail file %s\n", cmd,
		    afile);
	    exit(-1);
	}
	fclose(fp);
	repfile[0] = repfile[1] = repfile[2] = afile;
	if (rflag)
	    fileflag[0] = fileflag[1] = fileflag[2] = "A-R";
	else
	    fileflag[0] = fileflag[1] = fileflag[2] = "A";

	if ((fp = fopen(dfile, "r")) == NULL) {
	    fprintf(stderr, "Error: %s: cannot open icrash file %s\n", cmd,
		    dfile);
	    exit(1);
	}
	fclose(fp);
	repfile[3] = repfile[4] = repfile[5] = dfile;
	if (rflag) {
	    if (dflag)
		fileflag[3] = fileflag[4] = fileflag[5] = "D-R";
	    else
		fileflag[3] = fileflag[4] = fileflag[5] = "I-R";
	} else {
	    if (dflag)
		fileflag[3] = fileflag[4] = fileflag[5] = "D";
	    else
		fileflag[3] = fileflag[4] = fileflag[5] = "I";
	}

	sprintf(syscmdseed[0], "/usr/bsd/compress -f -c %%s |"
		"/usr/bin/crypt %s | /usr/bsd/uuencode %s |"
		"/usr/sbin/Mail -s AMR-%%s-Z-X-U %%s\n",
		AMR_CODE, AMR_EN_FILE);
	sprintf(syscmdseed[1], "/usr/bsd/compress -f -c %%s |"
		"/usr/bsd/uuencode %s | /usr/sbin/Mail -s AMR-%%s-Z-U %%s\n",
		AMR_EN_FILE);
	sprintf(syscmdseed[2], "/usr/bin/cat %%s | /usr/sbin/Mail "
	       "-s AMR-%%s-T %%s\n");

	syscmd[0] = syscmd[3] = syscmdseed[0];
	syscmd[1] = syscmd[4] = syscmdseed[1];
	syscmd[2] = syscmd[5] = syscmdseed[2];

	getemailaddresses(cfile);

	errflg = 1;
	/* ignore pager reports */
	for (type = 0; type < SEND_TYPE_NO - 1; type++)
	    if (sendtype[type] > 0) {
		sprintf(syscmdline, syscmd[type], repfile[type], fileflag[type],
			addresses[type]);
		system(syscmdline);
		errflg = 0;
	    }
	if (errflg)
	    fprintf(stderr, "Error: no email sent.\n");
    }
}

void
getemailaddresses(char *cfile)
{
    FILE	*fp;
    char	line[MAX_LINE_LEN], *ap, *dp, *ep;
    int		type, c, len[SEND_TYPE_NO], newaddr, n;

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

    for (type = 0; type < SEND_TYPE_NO; type++) {
	line[0] = '\0';
	if (sendtype[type]) {
	    ap = &addresses[type][1];
	    while (dp = strchr(ap, ' ')) {
		*dp = '\0';
		if (ep = strstr(sendaddresses, ap)) {
		    n = strlen(ap);
		    if (ep[n] == ' ' || ep[n] == '\0') {
			strcat(line, " ");
			strcat(line, ap);
		    }
		}
		ap = dp + 1;
	    }
	    sendtype[type] = strlen(line);
	    strcpy(addresses[type], line);
	}
    }
}
