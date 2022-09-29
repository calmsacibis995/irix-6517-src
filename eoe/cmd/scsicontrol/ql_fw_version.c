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
#include <sys/ql.h>

struct scsi_ha_op	scsiioctl;
struct ql_fw_version 	version;

void
sbusage(char *prog)
{
	fprintf(stderr,
	        "usage: %s <scsi_host_adapter_device>\n",
	        prog);
	exit(1);
}

main(int argc, char **argv)
{
	int	 fd;
	char	*fn;
	int	 c;
	int	 errs;

	opterr = 0;	/* handle errors ourselves. */

	if (argc == 1)	/* need at least 1 arg  */
		sbusage(argv[0]);

	while (optind < argc)
	{
		fn = argv[optind++];
		if((fd = open(fn, O_RDONLY)) == -1)
		{
			/* if open fails, try /hw/scsi_ctlr/%d/bus */
			char buf[256];
			sprintf(buf, "/hw/scsi_ctlr/%d/bus",atoi(fn));
			fd = open(buf, O_RDONLY);
			if(fd == -1)
			{
				printf("%s:  ", fn);
				fflush(stdout);
				perror("cannot open");
				errs++;
				continue;
			}
		}

		/* try to order for reasonableness; reset first in case
		 * hung, then inquiry, etc. */


		scsiioctl.sb_opt = QL_FW_VERSION;
                scsiioctl.sb_addr = (uintptr_t)&version;
                scsiioctl.sb_arg = sizeof(ql_fw_version_t);

		if (ioctl(fd, SOP_GETDATA, &scsiioctl) != 0)
		{
			printf("%s:  cannot get firmware version: %s\n",
			       fn, strerror(errno));
			errs++;
			exit(1);
		}
		printf("Q-logic Firmware Version %d.%d\n",version.major,
			version.minor);

		close(fd);
	}
	return(errs);
}
