/*
 * $Id: linkstat.c,v 1.16 1999/10/22 12:54:43 steiner Exp $
 * 
 * Prints link statistics for Craylink interconnection on Origin system.
 * Note: kdebug=1 must be set for full stats reporting, otherwise -a will
 * only monitor utilization (when kdebug=0, e.g. production).
 */

/* #define DEBUG */
#define _PAGESZ	16384

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/syssgi.h>
#include <sys/hwperfmacros.h>
#include <sys/SN/SN0/arch.h>
#include <sys/SN/SN0/slotnum.h>
#include <sys/SN/router.h>
#include <sys/SN/SN0/hubstat.h>
#include <sys/SN/SN0/sn0drv.h>
#include <sys/errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/resource.h>

#define MAX_DEV_PATH	255
#define NO_ROUTERSTATS_MSG "Craylink statistics are not being gathered:  use systune -i to change\n\tgather_craylink_routerstats to turn on statistics.\n"

void usage(char *cmd);
int get_stats(char *vertex_name, int fds, int dev_type, void *buf,
	      int error_mode, int verbose_mode);
int find_devs(char **vertices, int *types, int index, int dev_type);
void *open_devs(char **vertices, int *fds, int num_devs);
void print_hubstats(hubstat_t *hsp, char *name, int error_mode,
		    int verbose_mode);
void print_routerstats(router_info_t *rip, char *name, int error_mode,
		       int verbose_mode);

void setup_histogram_type(int, int);

int hist_type = RPPARM_HISTSEL_UTIL;

int
main(int argc, char *argv[])
{
	int i, c;
	int show_hubs = 0;
	int show_routers = 0;
	int error = 0;
	int continuous = 0;
	int error_mode = 0;
	int verbose_mode = 0;
	char vertex_name[MAX_DEV_PATH];
	char *vertices[MAX_ROUTERS + MAX_NASIDS];
	int fds[MAX_ROUTERS + MAX_NASIDS];
	int types[MAX_ROUTERS + MAX_NASIDS];
        int routerstats_enabled;
	struct rlimit rl;

	
	int num_devs = 0;
	void *buf;
	hubstat_t *hsp;

	extern char *optarg;
	extern int optind;

	vertex_name[0] = '\0';

	while((c = getopt(argc, argv, "ahrcevGQU")) != -1) {
		switch(c) {
			case 'a':
				show_hubs = 1;
				show_routers = 1;
				break;
			case 'h':
				show_hubs = 1;
				break;
			case 'r':
				show_routers = 1;
				break;
			case 'c':
				continuous = 1;
				break;
			case 'e':
				error_mode = 1;
				break;
			case 'v':
				verbose_mode = 1;
				break;
			case 'G':
				hist_type = RPPARM_HISTSEL_AGE;
				break;
			case 'U':
				hist_type = RPPARM_HISTSEL_UTIL;
				break;
			case 'Q':
				hist_type = RPPARM_HISTSEL_DAMQ;
				break;

			case '?':
				usage(argv[0]);
		}
	}

	rl.rlim_cur = rl.rlim_max = MAX_ROUTERS + MAX_NASIDS + 10;
	setrlimit(RLIMIT_NOFILE, &rl);

	if (argc == optind + 1) {
		strcpy(vertex_name, argv[optind]);
		vertices[0] = vertex_name;
		vertices[1] = (char *)NULL;
		types[0] = SN0DRV_UKNOWN_DEVICE;
		num_devs = 1;
	} else {
		if (!show_hubs && !show_routers)
			usage(argv[0]);
	}

        if(show_routers) {
	    if(syssgi(SGI_ROUTERSTATS_ENABLED, &routerstats_enabled) >= 0) {
		if(!routerstats_enabled) {
		    printf("%s", NO_ROUTERSTATS_MSG);
		    if(!show_hubs) exit(0);
		    show_routers = 0;
		}
	    }
	}	

	if (show_routers) {
		num_devs = find_devs(vertices, types, num_devs,
				     SN0DRV_ROUTER_DEVICE);
	}

	if (show_hubs) {
		num_devs = find_devs(vertices, types, num_devs,
				     SN0DRV_HUB_DEVICE);
	}

	if (!num_devs) {
		fprintf(stderr, "linkstat: Can't find any devices.\n");
		exit(1);
	}

	buf = open_devs(vertices, fds, num_devs);

	for (i = 0; i < num_devs; i++) 
	    setup_histogram_type(fds[i], types[i]);

	
	do {
		for (i = 0; i < num_devs; i++)
			get_stats(vertices[i], fds[i], types[i], buf,
				  error_mode, verbose_mode);
		if (continuous)
			sleep(5);
	} while (continuous);

	exit(0);
}

int
find_devs(char **vertices, int *types, int index, int dev_type)
{
	char *cmd;
	char buf[MAX_DEV_PATH];
	FILE *ptr;
	int lines = index;

	if (dev_type == SN0DRV_ROUTER_DEVICE)
		cmd = "ls -1 /hw/module/*/slot/r*/*router/mon";
	else if (dev_type == SN0DRV_HUB_DEVICE)
		cmd = "ls -1 /hw/module/*/slot/*/node/hub/mon";
	else
		return index;
		
	if ((ptr = popen(cmd, "r")) != NULL) {
		while (fgets(buf, BUFSIZ, ptr) != NULL) {
			vertices[lines] = (char *)malloc(strlen(buf) + 1);
			types[lines] = dev_type;
			if (buf[strlen(buf) - 1] == '\n')
				buf[strlen(buf) - 1] = '\0';
			strcpy(vertices[lines], buf);
			lines++;
		}
		close(ptr);
	} else {
		printf("linkstat: Command failed: %s\n", cmd);
	}
	return lines;
}

void *
open_devs(char **vertices, int *fds, int num_devs)
{
	int i;
	int size;
	int maxsize = 0;

	for (i = 0; i < num_devs; i++) {
		fds[i] = open(vertices[i], O_RDONLY);

#ifdef DEBUG
		printf("Opened %s, fd == %d\n", vertices[i], fds[i]);
#endif

		if (fds[i] < 0) {
			printf("linkstat: Error opening %s\n", vertices[i]);
			perror("linkstat: open_devs");
			exit(1);
		}

		size = ioctl(fds[0], SN0DRV_GET_INFOSIZE);
	
		if (size > maxsize)
			maxsize = size;

		if (size < 0) {
			perror("linkstat: open_devs");
			exit(1);
		}
	}
	
	return (void *)malloc(maxsize);
}

__int64_t
to_permin(__int64_t count, __int64_t time, __int64_t scale)
{
	count *= scale;		/* Convert to microsecond units */
	return (count / time);
}

char *rp_bucket_name[RP_HIST_TYPES][RP_NUM_BUCKETS] = {
{"Age0", "Age1", "Age2", "Age3"},
{"bypass","receive","send", "clock"},
{"Damq0", "Damq1", "Damq2", "Damq3"}
};

char *rp_hist_name[] = {
    " Age",
    " Utilization",
    " Damq",
};


int
get_stats(char *vertex_name, int fd, int dev_type, void *buf, int error_mode,
	  int verbose_mode)
{
	extern int errno;
	int routerstats_enabled;

#ifdef DEBUG
	printf("Getting stats on %s, fd = %d, type = %d\n",
		vertex_name, fd, dev_type);
#endif

	if (dev_type != SN0DRV_HUB_DEVICE) {
		if (ioctl(fd, SN0DRV_GET_ROUTERINFO, buf) < 0) {
			if (dev_type != SN0DRV_UKNOWN_DEVICE) {
				perror("linkstat: get_stats(ROUTERINFO_GETINFO)");
				exit(1);
			}
		} else {
		       if(dev_type == SN0DRV_UKNOWN_DEVICE) {
			    if(syssgi(SGI_ROUTERSTATS_ENABLED, 
				      &routerstats_enabled) >= 0) {
				if(!routerstats_enabled) {
				    printf("%s", NO_ROUTERSTATS_MSG);
				    return 0;
				}	
			    }
			}
			print_routerstats((router_info_t *)buf,
					  vertex_name, error_mode,
					  verbose_mode);
			return 0;
		}
	}

	if (ioctl(fd, SN0DRV_GET_HUBINFO, buf) < 0) {
		perror("linkstat: get_stats(HUBINFO_GETINFO)");
		exit(1);
	} else {
		print_hubstats((hubstat_t *)buf, vertex_name, error_mode,
				verbose_mode);
		return 0;
	}

	/* NOTREACHED */
	return -1;
}


/* ARGSUSED */
void
print_hubstats(hubstat_t *hsp, char *vertex_name, int error_mode,
	       int verbose_mode)
{
	hubstat_t *hubstat;
	__int64_t delta;

	if (hsp->hs_version > HUBSTAT_VERSION) {
		fprintf(stderr, "This is an old linkstat binary.  Please upgrade\n");
		exit(1);
	} else if (HUBSTAT_VERSION > hsp->hs_version) {
		fprintf(stderr, "This is an old kernel.  Please upgrade and try again.\n");
		exit(1);
	}

	printf("Hub: %s\n", vertex_name);
	delta = hsp->hs_timestamp - hsp->hs_timebase;

	if (verbose_mode) {
		printf(" Statistics for %lld seconds (%lld minutes)\n",
			delta / (hsp->hs_per_minute / 60),
			delta / hsp->hs_per_minute);
	}
	
	if (verbose_mode) {
		printf("  nasid %d\n", hsp->hs_nasid);
		printf("  stat_rev_id 0x%llx\n",
			hsp->hs_ni_stat_rev_id);
		printf("  timebase %lld  timestamp %lld\n",
			hsp->hs_timebase, hsp->hs_timestamp);
	}

	printf(" NI: Retries %lld (%lld/Min), SN errs %lld (%lld/Min), "
		"CB errs %lld (%lld/Min) \n",
			hsp->hs_ni_retry_errors,
			to_permin(hsp->hs_ni_retry_errors, delta,
				  hsp->hs_per_minute),
			hsp->hs_ni_sn_errors,
			to_permin(hsp->hs_ni_sn_errors, delta,
				  hsp->hs_per_minute),
			hsp->hs_ni_cb_errors,
			to_permin(hsp->hs_ni_cb_errors, delta,
				  hsp->hs_per_minute));

	printf(" II: SN errs %lld (%lld/Min), CB errs %lld (%lld/Min) \n",
			hsp->hs_ii_sn_errors,
			to_permin(hsp->hs_ii_sn_errors, delta,
				  hsp->hs_per_minute),
			hsp->hs_ii_cb_errors,
			to_permin(hsp->hs_ii_cb_errors, delta,
				  hsp->hs_per_minute));
	printf("-----------------------------------------------------------"
		"----------\n");
	if (verbose_mode)
		printf("\n");
}

void
print_routerstats(router_info_t *rip, char *vertex_name, int error_mode,
		  int verbose_mode)
{
	router_port_info_t *rpp;
	int port;
	int bucket;
	__int64_t delta;
	int total = 0;

	if (rip->ri_version > ROUTER_INFO_VERSION) {
		fprintf(stderr, "This is an old linkstat binary.  Please upgrade\n");
		exit(1);
	} else if (ROUTER_INFO_VERSION > rip->ri_version) {
		fprintf(stderr, "This is an old kernel.  Please upgrade and try again.\n");
		exit(1);
	}

	printf("Router: %s\n", vertex_name);
	delta = rip->ri_timestamp - rip->ri_timebase;

	if (verbose_mode)
		printf(" Statistics for %lld seconds (%lld minutes)\n",
			delta / (rip->ri_per_minute / 60LL),
			delta / rip->ri_per_minute);

	if (verbose_mode)
		printf("  nasid %d, vector 0x%llx, portmask 0x%x, LEDs 0x%x\n",
			rip->ri_nasid, rip->ri_vector, rip->ri_portmask, rip->ri_leds);
		
	if (verbose_mode) {
		printf("  stat_rev_id 0x%llx, writeid %ld\n",
			rip->ri_stat_rev_id, rip->ri_writeid);
		printf("  timebase %lld  timestamp %lld\n",
			rip->ri_timebase, rip->ri_timestamp);
	}

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		if (!(rip->ri_portmask & (1 << (port - 1))))
			continue;
		rpp = &rip->ri_port[port - 1];
		printf(" Port %d:", port);
		if (verbose_mode) {
			printf("  histograms 0x%llx, port error 0x%llx\n",
				rpp->rp_histograms, rpp->rp_port_error);
		}
		if (!error_mode) {
			switch (rip->ri_hist_type) {
			case RPPARM_HISTSEL_AGE:
			case RPPARM_HISTSEL_DAMQ:
			    printf (" %s:", rp_hist_name[rip->ri_hist_type]);
			    total = 0;
			    for (bucket = 0; bucket < RP_NUM_BUCKETS;bucket++)
				total += rpp->rp_util[bucket];
			    for (bucket = 0; bucket < RP_NUM_BUCKETS;bucket++)
				printf(" %s (%d) %d%% ", rp_bucket_name[rip->ri_hist_type][bucket],
				       rpp->rp_util[bucket],
				       total ?  (100 * rpp->rp_util[bucket])/total : 0);
			    printf("\n");
			    break;
			case RPPARM_HISTSEL_UTIL:
			    printf (" %s:", rp_hist_name[rip->ri_hist_type]);
			    for (bucket = 0; bucket < RP_NUM_UTILS; bucket++) {
				printf(" %s %d%% ", rp_bucket_name[rip->ri_hist_type][bucket],
				       (100 * rpp->rp_util[bucket]) /
				       RR_UTIL_SCALE);
			    }
			    printf("\n");
			}
		}	
		printf("  Retries %ld (%lld/Min), SN errs %ld (%lld/Min), CB errs %ld (%lld/Min) \n",
			rpp->rp_retry_errors,
			to_permin(rpp->rp_retry_errors, delta,
				  rip->ri_per_minute),
			rpp->rp_sn_errors,
			to_permin(rpp->rp_sn_errors, delta,
				  rip->ri_per_minute),
			rpp->rp_cb_errors,
			to_permin(rpp->rp_cb_errors, delta,
				  rip->ri_per_minute));
	}

	printf("-----------------------------------------------------------"
		"----------\n");
	if(verbose_mode)
		printf("\n");
}

void
usage(char *cmd)
{
	fprintf(stderr, "Usage: %s [-c] [-e] [-v] -a | vertex_path\n", cmd);
	exit(1);
}


void
setup_histogram_type(int fd, int dev_type)
{
	
	if (dev_type == SN0DRV_ROUTER_DEVICE) {
		if (ioctl(fd, SN0DRV_SET_HISTOGRAM_TYPE, hist_type) < 0) {
			perror("Unable to set histogram type");
			printf("kdebug!=1?\n");
			exit(1);
		}
		
	}
	
}

