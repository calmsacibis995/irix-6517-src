
/******************************************************************
 *
 *  SpiderX25 - Configuration utility
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  LLTUNE.C
 *
 *  X25 Control Interface : configuration of LAPB/LLC2 module
 *
 ******************************************************************/

/*
 *	 /usr/projects.old/tcp/PBRAIN/SCCS/pbrainF/rel/src/clib/x25util/0/s.lltune.c
 *	@(#)lltune.c	5.8
 *
 *	Last delta created	10:07:01 5/12/92
 *	This file extracted	15:14:15 5/14/92
 *
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>

#include <sys/snet/uint.h>
#include <sys/snet/ll_proto.h>
#include <sys/snet/ll_control.h>
#include <sys/snet/x25_proto.h>
#include <sys/snet/x25_control.h>

#include <stdio.h>
#include <fcntl.h>
#include <streamio.h>

#define	LAPB_DEV "/dev/lapb"
#define	LLC2_DEV "/dev/llc2"

#define	ACCREAD	04

extern void open_conf(), close_conf();
extern ushort 	get_conf_ushort();

extern char 	*optarg;
extern int 	errno, optind;

struct protos {
	char   *name;
	char   *device;
	int	blocklen;
	int	count;
	uint8	type;
} protos[] = {
	{ "lapb", LAPB_DEV,
		  sizeof(struct lapb_tnioc),
		  (sizeof(lapbtune_t)/sizeof(uint16)),
		  LI_LAPBTUNE },
	{ "llc2", LLC2_DEV,
		  sizeof(struct llc2_tnioc),
		  sizeof(llc2tune_t)/sizeof(uint16),
		  LI_LLC2TUNE },
	{ 0 } };


#define TIMEOUT	 60
#define OPTIONS  "PGd:s:p:"	/* Valid command line options	 */
#define	USAGE1	 "usage: %s -s subnet_id -p protocol  -P  [-d device] [filename]\n"
#define	USAGE2	 "       %s -s subnet_id -p protocol [-G] [-d device]\n"
					 
extern ushort    get_conf_ushort();
extern char *	 get_conf_line();

extern char 	*optarg;
extern int 	 errno, optind;

char            *prog_name;
extern char     *basename();
char		*subnetid, *device, *protocol;

main (argc, argv)
int	argc;
char	**argv;
{
	struct  protos   *prp;
	lliun_t           tnioc;
	struct	strioctl  strio;
	uint16           *tunep;
	int		  c, count;
	int		  dd;	/* Device descriptor */
	int		  lp;

	/* Booleans to show whether devices, snids etc are present */
	int get_reqd = 0, put_reqd = 0;

	prog_name = basename(argv[0]);

	/* Get any command line options */
	while ((c = getopt (argc, argv, OPTIONS)) != EOF)
	{
		switch (c)
		{
		case 'd' :	/* device */
				device = optarg;
				break;

		case 's' :	/* subnetwork ID */
				subnetid = optarg;
				break;

		case 'p' :	/* protocol */
				protocol = optarg;
				break;
				
		case 'P' :	/* put ioctl option */
				put_reqd = 1;
				break;

		case 'G' :	/* get ioctl option */
				get_reqd = 1;
				break;

		case '?' :	/* ERROR */
				fprintf(stderr, USAGE1, prog_name);
				fprintf(stderr, USAGE2, prog_name);
				exit(1);
		}
	}

	if (put_reqd == 0 && get_reqd == 0)
		get_reqd = 1;			/*
						 *  Set default to put if
						 *  no option specifed
						 */
						 
	if (put_reqd == get_reqd)      /* Put and get have same values */
	{
		fprintf(stderr, USAGE1, argv[0]);
		fprintf(stderr, USAGE2, argv[0]);
		exerr("only one of -P or -G may be specifed");
	}

	if (!subnetid)
	{
		fprintf(stderr, USAGE1, argv[0]);
		fprintf(stderr, USAGE2, argv[0]);
		exerr("no subnetwork identifier given");
	}

	if (!protocol)
	{
		fprintf(stderr, USAGE1, argv[0]);
		fprintf(stderr, USAGE2, argv[0]);
		exerr("no protocol specified");
	}

	/* Find protocol and check */
	for (prp = protos; prp->name; prp++)
		if (strcmp(protocol, prp->name) == 0) break;
	if (!prp->name) exerr("%s is an invalid protocol", protocol);

	/* If no explicit device, take protocol's default */
	if (!device) device = prp->device;
	if ((dd = S_OPEN(device, O_RDWR))<0)
		experr("opening %s", device);

	if (put_reqd)
	{
		/* Read values from configuration file */
		open_conf(argv[optind]);
		tunep = (uint16 *)&tnioc.lapb_tn.lapb_tune;
		count = prp->count; 
/*
 * Since the last 5 mask fields have been added to def.lapb we can not
 * use the old method of reading in the data. For the last five fields
 * we have to read in 5 lines and set up the relevant bit mask.
 * Hence if we decrement the count ( i.e. don't read in last field )
 * and read in five lines, we have the complete file.
 */
		if (prp->type == LI_LAPBTUNE)
			count--;
		while (count--)
			*tunep++ = get_conf_ushort();
		for ( lp = 1 ; prp->type == LI_LAPBTUNE && lp < (1<<5); lp<<=1 )
		{
			char *line = get_conf_line();

			if (!strchr("YyNn", line[0]) || line[1] != '\0')
				conf_exerr("Invalid field, not 'Y|y' or 'N|n'");
			if (strchr("Yy", line[0]))
				*tunep |= lp;
		}
		close_conf();

		tnioc.ll_hd.lli_type = prp->type;
		tnioc.ll_hd.lli_snid = snidtox25(subnetid);
		tnioc.ll_hd.lli_spare[0] = 0;
		tnioc.ll_hd.lli_spare[1] = 0;
		tnioc.ll_hd.lli_spare[2] = 0;

		strio.ic_cmd    = L_SETTUNE;
		strio.ic_timout = TIMEOUT;
		strio.ic_len    = prp->blocklen;
		strio.ic_dp     = (char *)&tnioc;	

		if (S_IOCTL(dd, I_STR, (char *)&strio) < 0)
			experr("L_SETTUNE ioctl failed on %s", device);

		return 0;
	}
	else			/* Get */
	{
		tnioc.ll_hd.lli_type = prp->type;
		tnioc.ll_hd.lli_snid = snidtox25(subnetid);
		tnioc.ll_hd.lli_spare[0] = 0;
		tnioc.ll_hd.lli_spare[1] = 0;
		tnioc.ll_hd.lli_spare[2] = 0;

		strio.ic_cmd    = L_GETTUNE;
		strio.ic_timout = TIMEOUT;
		strio.ic_len    = prp->blocklen;
		strio.ic_dp     = (char *)&tnioc;	

		if (S_IOCTL(dd, I_STR, (char *)&strio) < 0)
			experr("L_GETTUNE ioctl failed on %s", device);

		tunep = (uint16 *)&tnioc.lapb_tn.lapb_tune;
		count = prp->count; 

/*
 * Same applies as above.
 */
		if (prp->type == LI_LAPBTUNE)
			count--;
		while (count--)
			printf("%d\n", *tunep++);
		for ( lp = 1 ; prp->type == LI_LAPBTUNE && lp < (1<<5); lp<<=1 )
			printf("%c\n",*tunep & lp ? 'Y' : 'N');

		return 0;
	}
}
