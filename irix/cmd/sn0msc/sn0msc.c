#define SN0		1

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/SN/router.h>
#include <sys/SN/SN0/sn0drv.h>

extern	int		errno;
extern	char	       *sys_errlist[];

#define STRERR		sys_errlist[errno]

int			opt_read;
int			opt_write;
int			opt_hex;
int			opt_cmd;
char		       *opt_dev;
char		       *opt_cmdstr;
int			opt_addr;
int			opt_count	= 1;
int			opt_value;

void usage(void)
{
    fprintf(stderr, "Usages:\n");
    fprintf(stderr, "  sn0msc -r [-x] elsc_nvram_dev addr [count]\n");
    fprintf(stderr, "  sn0msc -w elsc_nvram_dev addr value [count]\n");
    fprintf(stderr, "  sn0msc -c elsc_controller_dev \"command\"\n");

    exit(1);
}

main(int argc, char **argv)
{
    int			i, fd;
    char		buf[SN0DRV_CMD_BUF_LEN];
    sn0drv_nvop_t	nvop;

    while ((i = getopt(argc, argv, "rwcx")) >= 0)
	switch (i) {
	case 'r':
	    opt_read	= 1;
	    break;
	case 'w':
	    opt_write	= 1;
	    break;
	case 'c':
	    opt_cmd	= 1;
	    break;
	case 'x':
	    opt_hex	= 1;
	    break;
	}

    if (opt_read + opt_write + opt_cmd != 1 || optind + 2 > argc)
	usage();

    opt_dev = argv[optind++];

    if (opt_cmd)
	opt_cmdstr = argv[optind++];
    else
	opt_addr = strtol(argv[optind++], 0, 0);

    if (opt_write) {
	if (optind == argc)
	    usage();
	opt_value = strtol(argv[optind++], 0, 0);
    }

    if (opt_read || opt_write) {
	if (optind < argc)
	    opt_count = strtol(argv[optind++], 0, 0);
    }

    if (optind < argc)
	usage();

#if 0
    if (opt_read)
	printf("READ dev=%s addr=%d count=%d\n",
	       opt_dev, opt_addr, opt_count);

    if (opt_write)
	printf("WRITE dev=%s addr=%d value=%d count=%d\n",
	       opt_dev, opt_addr, opt_value, opt_count);

    if (opt_cmd)
	printf("CMD dev=%s cmd=%s\n",
	       opt_dev, opt_cmdstr);
#endif

    if ((fd = open(opt_dev, O_RDWR)) < 0) {
	fprintf(stderr,
		"sn0msc: could not open %s: %s\n",
		opt_dev, STRERR);
	exit(1);
    }

    if (opt_read) {
	while (opt_count--) {
	    nvop.flags	= SN0DRV_NVOP_READ;
	    nvop.addr	= opt_addr;

	    if (ioctl(fd, SN0DRV_ELSC_NVRAM, &nvop)) {
		fprintf(stderr,
			"sn0msc: NVRAM read to %s address %d failed: %s\n",
			opt_dev, opt_addr, STRERR);
		exit(1);
	    }

	    printf(opt_hex ? "%02x\n" : "%d\n", nvop.data);

	    opt_addr++;
	}
    } else if (opt_write) {
	while (opt_count--) {
	    nvop.flags	= SN0DRV_NVOP_WRITE;
	    nvop.addr	= opt_addr;
	    nvop.data	= opt_value;

	    if (ioctl(fd, SN0DRV_ELSC_NVRAM, &nvop)) {
		fprintf(stderr,
			"sn0msc: NVRAM write to %s address %d failed: %s\n",
			opt_dev, opt_addr, STRERR);
		exit(1);
	    }

	    opt_addr++;
	}
    } else if (opt_cmd) {
	strcpy(buf, opt_cmdstr);

	if (ioctl(fd, SN0DRV_ELSC_COMMAND, buf) < 0) {
	    fprintf(stderr,
		    "sn0msc: command to %s failed: %s\n",
		    opt_dev, STRERR);
	    exit(1);
	}

	printf("%s\n", buf);		/* Output response */
    }

    close(fd);

    exit(0);
}
