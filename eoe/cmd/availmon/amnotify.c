#ident "$Revision: 1.6 $"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <fcntl.h>
#include "defs.h" 

char	cmd[MAX_STR_LEN];
int	cflag = 0, aflag = 0, dflag = 0, iflag = 0, pflag = 0, rflag = 0;
char	cfile[MAX_STR_LEN] = "/var/adm/avail/config/autoemail.list";
char	afile[MAX_STR_LEN], dfile[MAX_STR_LEN], pfile[MAX_STR_LEN];
char	*repfile[SEND_TYPE_NO];
char	*fileflag[SEND_TYPE_NO];
int	sendtype[SEND_TYPE_NO];
char	addresses[SEND_TYPE_NO][MAX_LINE_LEN];
char	syscmdseed[4][MAX_LINE_LEN];
char	*syscmd[SEND_TYPE_NO];

void	getemailaddresses(char *cfile);


main(int argc, char *argv[])
{
    int		c, errflg = 0, type;
    char	syscmdline[MAX_LINE_LEN];
    FILE	*fp;

    strcpy(cmd, basename(argv[0]));

    /* process online options */
    while ((c = getopt(argc, argv, "c:a:d:i:p:r")) != EOF)
	switch (c) {
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
	case 'a':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else if (aflag)
		errflg++;
	    else {
		aflag = 1;
		strcpy(afile, optarg);
	    }
	    break;
	case 'd':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else if (dflag || iflag)
		errflg++;
	    else {
		dflag = 1;
		strcpy(dfile, optarg);
	    }
	    break;
	case 'i':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else if (iflag || dflag)
		errflg++;
	    else {
		iflag = 1;
		strcpy(dfile, optarg);
	    }
	    break;
	case 'p':
	    if ((optind - 1 == argc) || (optarg[0] == '-')) {
		/* no file was specified */
		errflg++;
	    } else if (pflag)
		errflg++;
	    else {
		pflag = 1;
		strcpy(pfile, optarg);
	    }
	    break;
	case 'r':
	    if (rflag)
		errflg++;
	    else
		rflag = 1;
	    break;
	case '?':
	    errflg++;
	}

    if (errflg) {
	fprintf(stderr, "Usage: %s [-c <config file>] [-a <avail file>]"
			" [-i <icrash file>] [-r]\n", cmd);
	exit(1);
    }

    if (aflag) {
	if ((fp = fopen(afile, "r")) == NULL) {
	    fprintf(stderr, "Error: %s: cannot open avail file %s\n", cmd,
		    afile);
	    exit(1);
	}
	fclose(fp);
	repfile[0] = repfile[1] = repfile[2] = afile;
	sendtype[0] = sendtype[1] = sendtype[2] = 0;
	if (rflag)
	    fileflag[0] = fileflag[1] = fileflag[2] = "A-R";
	else
	    fileflag[0] = fileflag[1] = fileflag[2] = "A";
    } else
	sendtype[0] = sendtype[1] = sendtype[2] = -1;

    if (dflag || iflag) {
	if ((fp = fopen(dfile, "r")) == NULL) {
	    fprintf(stderr, "Error: %s: cannot open icrash file %s\n", cmd,
		    dfile);
	    exit(1);
	}
	fclose(fp);
	repfile[3] = repfile[4] = repfile[5] = dfile;
	sendtype[3] = sendtype[4] = sendtype[5] = 0;
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
    } else
	sendtype[3] = sendtype[4] = sendtype[5] = -1;

    if (pflag) {
	if ((fp = fopen(pfile, "r")) == NULL) {
	    fprintf(stderr, "Error: %s: cannot open pager file %s\n", cmd,
		    pfile);
	    exit(1);
	}
	fclose(fp);
	repfile[6] = pfile;
	sendtype[6] = 0;
	fileflag[6] = "P";
    } else
	sendtype[6] = -1;

    sprintf(syscmdseed[0], "/usr/bsd/compress -f -c %s | /usr/bin/crypt %s |"
	    " /usr/bsd/uuencode %s | /usr/sbin/Mail -s AMR-%s-Z-X-U %s\n",
	    "%s", AMR_CODE, AMR_EN_FILE, "%s", "%s");
    sprintf(syscmdseed[1], "/usr/bsd/compress -f -c %s | /usr/bsd/uuencode %s "
	    "| /usr/sbin/Mail -s AMR-%s-Z-U %s\n", "%s", AMR_EN_FILE,
	    "%s", "%s");
    strcpy(syscmdseed[2], "/usr/bin/cat %s | /usr/sbin/Mail -s AMR-%s-T %s\n");
    strcpy(syscmdseed[3], "/usr/bin/cat %s | /usr/sbin/Mail %s\n");

    syscmd[0] = syscmd[3] = syscmdseed[0];
    syscmd[1] = syscmd[4] = syscmdseed[1];
    syscmd[2] = syscmd[5] = syscmdseed[2];
    syscmd[6] = syscmdseed[3];

    getemailaddresses(cfile);

    for (type = 0; type < SEND_TYPE_NO - 1; type++)
	if (sendtype[type] > 0) {
	    sprintf(syscmdline, syscmd[type], repfile[type], fileflag[type],
		    addresses[type]);
	    system(syscmdline);
	}
    type = SEND_TYPE_NO - 1;
    if (sendtype[type] > 0) {
	sprintf(syscmdline, syscmd[type], repfile[type], addresses[type]);
	system(syscmdline);
    }
}

void
getemailaddresses(char *cfile)
{
    FILE	*fp;
    char	line[ MAX_LINE_LEN], *ap, *dp;
    int		type, c, len[SEND_TYPE_NO], newaddr;

    if ((fp = fopen(cfile, "r")) == NULL) {
	fprintf(stderr,"Error: %s: cannot open configuration file %s\n",
		cmd, cfile);
	exit(1);
    }

    for (type = 0; type < SEND_TYPE_NO; type++) {
	addresses[type][0] = '\0';
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

	if (sendtype[type] < 0)
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
	*dp = '\0';
    }
    fclose(fp);
}	
