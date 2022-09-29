#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/scsi.h>

struct scsi_ha_op	scsiioctl;


void
sbusage(char *prog)
{
	fprintf(stderr,
	        "usage: %s [-r (reset adapter and/or SCSI bus)] [-p (probe)]\n\t"
		"[-t (scsi parms)] "
		"[-l (Fibrechannel: init loop)]\n\t"
		"[-L <target> (Fibrechannel: reset target and init loop)]\n\t"
		"<scsi bus number | full name of the scsi bus vertex>\n",
	        prog);
	exit(1);
}

int
itemnumber(char *textnumber)
{
	int	  number;

	if (strcmp(textnumber, "0") == 0)
		return 0;
	number = (int) strtol(textnumber, NULL, 0);
	if (number == 0)
		return -1;
	return number;
}

main(int argc, char **argv)
{
	int	 doreset = 0, doprobe = 0, dodebug = 0, doparms = 0,
		 dobypass = 0, doenable = 0, doenableall = 0,
		 dolip = 0, doliprst = 0;
	int	 debuglevel, portno;
	int	 fd;
	char	*fn;
	int	 c;
	int	 errs;

	opterr = 0;	/* handle errors ourselves. */
	while ((c = getopt(argc, argv, "D:L:b:e:Elprt")) != -1)
	{
		switch (c)
		{
		case 'r':
			doreset = 1;	/* do a scsi bus reset */
			break;
		case 'p':
			doprobe = 1;
			break;
		case 't':
			doparms = 1;
			break;
		case 'D':
			dodebug = 1;
			debuglevel = itemnumber(optarg);
			if (debuglevel < 0 || debuglevel > 99)
				sbusage(argv[0]);
			break;
		case 'b':
			dobypass = 1;
			portno = itemnumber(optarg);
			if (portno < 0 || portno > 125)
				sbusage(argv[0]);
			break;
		case 'e':
			doenable = 1;
			portno = itemnumber(optarg);
			if (portno < 0 || portno > 125)
				sbusage(argv[0]);
			break;
		case 'E':
			doenableall = 1;
			break;
		case 'l':
			dolip = 1;
			break;
		case 'L':
			doliprst = 1;
			if (strcasecmp(optarg, "all") == 0)
				portno = 255;
			else
			{
				portno = itemnumber(optarg);
				if (portno < 0 || portno > 125)
					sbusage(argv[0]);
			}
			break;

		default:
			sbusage(argv[0]);
		}
	}

	if (optind >= argc || optind == 1)	/* need at 1 arg and one option */
		sbusage(argv[0]);

	while (optind < argc)
	{
	        char buf[256], *p;
		int  busnum;

		fn = argv[optind++];

	        /*
		 * Check if the argument is completely numerical
		 */
		busnum = (int)strtoul(fn, &p, 0);
		if (*p == '\0') /* Argument is completely numerical */
		{
		  sprintf(buf, "/hw/scsi_ctlr/%d/bus", busnum);
		  if ((fd = open(buf, O_RDONLY)) == -1)
		  {
			printf("%s:  ", buf);
			fflush(stdout);
			perror("cannot open");
			errs++;
			continue;
		  }
		}
		else		/* Argument is NOT completely numerical */
		{
		  if ((fd = open(fn, O_RDONLY)) == -1)
		  {
			printf("%s:  ", fn);
			fflush(stdout);
			perror("cannot open");
			errs++;
			continue;
		  }
		}

		/* try to order for reasonableness; reset first in case
		 * hung, then LIP, LIPRST, probe, etc. */

		if (doreset)
		{
			if (ioctl(fd, SOP_RESET, &scsiioctl) != 0)
			{
				printf("%s:  ", fn);
				printf("reset failed: %s\n", strerror(errno));
				errs++;
			}
		}

		if (dolip)
		{
			if (ioctl(fd, SOP_LIP, &scsiioctl) != 0)
			{
				printf("%s:  loop initialization failed: %s\n", fn,
				       strerror(errno));
				errs++;
			}
		}

		if (doliprst)
		{
			scsiioctl.sb_arg = portno;
			if (ioctl(fd, SOP_LIPRST, &scsiioctl) != 0)
			{
				printf("%s:  target reset and loop initialization"
				       " failed: %s\n", fn, strerror(errno));
				errs++;
			}
		}

		if (doprobe)
		{
			if (ioctl(fd, SOP_SCAN, &scsiioctl) != 0)
			{
				printf("%s:  ", fn);
				printf("probe failed: %s\n", strerror(errno));
				errs++;
			}
		}

		if (doparms)
		{
		        struct scsi_parms sp;
			uint              targ;
			uint              first = 1;
			char		 *bustype;

			scsiioctl.sb_addr = (uintptr_t)&sp;
			if (ioctl(fd, SOP_GET_SCSI_PARMS, &scsiioctl) != 0)
			{
				printf("%s:  ", fn);
				printf("scsi parms failed: %s\n", strerror(errno));
				errs++;
			}
			else {
			  switch(sp.sp_is_diff){
			  	case 2:
					bustype = "LVD";
				break;
			  	case 1:
					bustype = "DIFF";
				break;
				case 0: 
					bustype = "SE";
				break;
				default:
					bustype = "UNKN";
			   }

			  printf("BUS PARMS: Selection Timeout: %d uS, "
				 "Host SCSI ID: %d, "
				 "%s\n", 
				 sp.sp_selection_timeout,
				 sp.sp_scsi_host_id,
				 bustype);

			  for (targ = 0; targ < 16; ++targ) {
			    if (sp.sp_target_parms[targ].stp_is_present) {
			      if (first)
				printf("TARGET PARMS: [%d] ", targ);
			      else
				printf("            : [%d] ", targ);
			      first = 0;
			      if (sp.sp_target_parms[targ].stp_is_sync) 
				printf("sync. period: %d nS, sync. offset: %d, ",
				       sp.sp_target_parms[targ].stp_sync_period,
				       sp.sp_target_parms[targ].stp_sync_offset);
			      else
				  printf("async. transfers, ");
			      printf("%s\n", (sp.sp_target_parms[targ].stp_is_wide ? "WIDE" : "NARROW"));
			    }
			  }
			  if (first)
			    printf("TARGET PARMS: No attached devices found.\n");
			}
		}

		if (dodebug)
		{
			scsiioctl.sb_arg = debuglevel;
			if (ioctl(fd, SOP_DEBUGLEVEL, &scsiioctl) != 0)
			{
				printf("%s:  cannot set debug level: %s\n",
				       fn, strerror(errno));
				errs++;
			}
		}

		if (dobypass)
		{
			scsiioctl.sb_arg = portno;
			if (ioctl(fd, SOP_ENABLE_PBC, &scsiioctl) != 0)
			{
				printf("%s:  cannot send bypass primitive: %s\n",
				       fn, strerror(errno));
				errs++;
			}
		}
		if (doenableall)
		{
			if (ioctl(fd, SOP_LPEALL, &scsiioctl) != 0)
			{
				printf("%s:  cannot send enable primitive: %s\n",
				       fn, strerror(errno));
				errs++;
			}
		}
		else if (doenable)
		{
			scsiioctl.sb_arg = portno;
			if (ioctl(fd, SOP_LPE, &scsiioctl) != 0)
			{
				printf("%s:  cannot send enable primitive: %s\n",
				       fn, strerror(errno));
				errs++;
			}
		}

		close(fd);
	}
	return(errs);
}
