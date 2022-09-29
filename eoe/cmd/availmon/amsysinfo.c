#ident "$Revision: 1.8 $"

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/syssgi.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>


main(int argc, char *argv[])
{
    int		c, i, errflg = 0;
    int		sflag = 0, nflag = 0;
    char	cmd[128];
    struct hostent *hent;
    char	systemid[MAXSYSIDSIZE + 1], systemid1[MAXSYSIDSIZE + 1];
    char	hostname[MAXHOSTNAMELEN + 1];
    struct utsname name;
    char	**p;
    int		modnum;
    module_info_t modinfo;
    extern int	errno;

    strcpy(cmd, basename(argv[0]));
    strcpy(systemid, "");

    /* process online options */
    while ((c = getopt(argc, argv, "sn")) != EOF)
	switch (c) {
	case 's':
	    sflag = 1;
	    if (nflag)
		errflg++;
	    if ((optind != argc) && (argv[optind][0] != '-')) {
		strcpy(systemid, argv[optind]);
		optind++;
	    }
	    break;
	case 'n':
	    nflag = 1;
	    if (sflag)
		errflg++;
	    break;
	case '?':
	default:
	    errflg++;
	}

    if (errflg) {
	fprintf(stderr, "Usage: %s { -s | -n }\n", cmd);
	exit(-1);
    } else if (!sflag && !nflag) {
	/* no option was given, default to -s */
	sflag = 1;
    }

    if (sflag) {
	/*
	 * get system's serial number
	 */
	if (uname(&name) == -1) {
	    fprintf(stderr, "Error %d: %s: cannot call uname\n", errno, cmd);
	    exit(-1);
	}
	/*
	 * to allow this command to be used on old IRIX, I did not use
	 * get_module_info for all platforms execpt IP20, which is the
	 * only platform supported in 6.5 that does not have machine-
	 * readable serial number.
	 */
	if (strcmp(systemid, "") == 0) {
	    if (strcmp(name.machine, "IP19") == 0 ||
		strcmp(name.machine, "IP21") == 0 ||
		strcmp(name.machine, "IP25") == 0) {
		if (syssgi(SGI_SYSID, systemid) == -1) {
		    fprintf(stderr, "Error %d: %s: cannot call syssgi\n", errno,
			    cmd);
		    printf("unknown\n");
		    exit(-1);
		}
	    } else if (strcmp(name.machine, "IP22") == 0 ||
		       strcmp(name.machine, "IP26") == 0 ||
		       strcmp(name.machine, "IP28") == 0 ||
		       strcmp(name.machine, "IP30") == 0 ||
		       strcmp(name.machine, "IP32") == 0) {
		if (syssgi(SGI_SYSID, systemid1) == -1) {
		    fprintf(stderr, "Error %d: %s: cannot call syssgi\n", errno,
			    cmd);
		    printf("unknown\n");
		    exit(-1);
		} else {
		    strcpy(systemid, "0800");
		    for (i = 0; i < 4; i++)
			sprintf(&systemid[i*2 + 4], "%2.2X", systemid1[i] & 0xff);
		    systemid[12] = '\0';
		}
	    } else if (strcmp(name.machine, "IP27") == 0) {
		get_module_info(0, &modinfo, sizeof(module_info_t));
		strcpy(systemid, modinfo.serial_str);
	    } else {
		printf("unknown\n");
		exit(1);
	    }
	}
	/*
	 * check the format of the system's serial number
	 */
	if (strcmp(name.machine, "IP19") == 0 ||
	    strcmp(name.machine, "IP21") == 0 ||
	    strcmp(name.machine, "IP25") == 0) {
	    if (strlen(systemid) != 6)
		goto invalid;
	    if (systemid[0] != 'S')
		goto invalid;
	    for (i = 1; i < 6; i++)
		if (! isdigit(systemid[i]))
		    goto invalid;
	} else if (strcmp(name.machine, "IP22") == 0 ||
		   strcmp(name.machine, "IP26") == 0 ||
		   strcmp(name.machine, "IP28") == 0 ||
		   strcmp(name.machine, "IP30") == 0 ||
		   strcmp(name.machine, "IP32") == 0) {
	    if (strlen(systemid) == 8) {
		if (strncmp(systemid, "69", 2))
		    goto invalid;
		for (i = 2; i < 8; i++)
		    if (! isxdigit(systemid[i]))
			goto invalid;
	    } else if (strlen(systemid) == 12) {
		if (strncmp(systemid, "080069", 6))
		    goto invalid;
		for (i = 6; i < 12; i++)
		    if (! isxdigit(systemid[i]))
			goto invalid;
	    } else
		goto invalid;
	} else if (strcmp(name.machine, "IP27") == 0) {
	    if (strlen(systemid) != 8)
		goto invalid;
	    if (strncmp(systemid, "K", 1) && strncmp(systemid, "690", 3))
		goto invalid;
	    for (i = 1; i < 8; i++)
		if (! isxdigit(systemid[i]))
		    goto invalid;
	} else if (strcmp(name.machine, "IP17") == 0) {
	    if (strlen(systemid) == 6) {
		if (systemid[0] != 'S')
		    goto invalid;
		for (i = 1; i < 6; i++)
		    if (! isdigit(systemid[i]))
			goto invalid;
	    } else if (strlen(systemid) == 8) {
		for (i = 0; i < 8; i++)
		    if (! isdigit(systemid[i]))
			goto invalid;
	    } else
		goto invalid;
	} else if (strcmp(name.machine, "IP20") == 0) {
	    if (strlen(systemid) != 8)
		goto invalid;
	    if (strncmp(systemid, "35", 2) && strncmp(systemid, "37", 2))
		goto invalid;
	    for (i = 2; i < 8; i++)
		if (! isxdigit(systemid[i]))
		    goto invalid;
	} else if (strcmp(name.machine, "IP6") == 0) {
	    if (strlen(systemid) == 9) {
		if (systemid[0] != 'E')
		    goto invalid;
		for (i = 1; i < 9; i++)
		    if (! isdigit(systemid[i]))
			goto invalid;
	    } else if (strlen(systemid) == 8) {
		for (i = 0; i < 8; i++)
		    if (! isdigit(systemid[i]))
			goto invalid;
	    } else
		goto invalid;
	}
	puts(systemid);
	return(0);

      invalid:
	printf("unknown\n");
	exit(1);
    } else {
	/*
	 * get system's hostname, including domainname
	 */
	if (gethostname (hostname, MAXHOSTNAMELEN) != 0) {
	    fprintf(stderr, "Error: %s: cannot invoke gethostname.\n", cmd);
	    printf("unknown\n");
	    exit(-1);
	}
	i = 0;
	while (1) { 
	    hent = gethostbyname(hostname);
	    if (hent != NULL)
		break;
	    if (h_errno == TRY_AGAIN) {	
		i++;
		if (i > 3) {
		    fprintf(stderr, "Error: %s: cannot get hostname."
			    "  Failed after 3 tries", cmd);
		    printf("unknown\n");
		    exit(-1);
		}
		continue;
	    } else { /* some irrecorverable error */
		fprintf(stderr, "Error: %s: cannot get hostname.\n", cmd);
		printf("unknown\n");
		exit(-1);
	    }
	}
	puts(hent->h_name);
    }
}
