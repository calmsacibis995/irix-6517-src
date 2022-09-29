/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 * snoop.c	1.12	94/08/29 SMI"
 */

#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop.c,v 1.5 1998/04/24 00:47:16 jlan Exp $"

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <varargs.h>
#include <setjmp.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#ifndef sgi
#include <sys/pfmod.h>
#endif
#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netdb.h>

#ifdef sgi
void audioinit();
#endif

#include "snoop.h"

struct cache_struct xid_cache[XID_CACHE_SIZE];

int snaplen = 128 * 1024;
char *device = NULL;
jmp_buf jmp_env;

#define	MAXSUM 8
#define MAXSUMLINE 1024
char sumline  [MAXSUM] [MAXSUMLINE + MAXLINE + 1];
char detail_line [MAXLINE + 1];
int audio;
int maxcount;	/* maximum no of packets to capture */
int count;	/* count of packets captured */
int sumcount;
int x_offset = -1;
int x_length = 0x7fffffff;
FILE *namefile;
int Pflg;

#ifndef sgi
struct packetfilt pf;
#endif

void usage();
void pr_err();
void show_count();

main(argc, argv)
	int argc; char **argv;
{
	extern char *optarg;
	extern int optind;
	int c;
	int filter = 0;
	int flags = F_SUM;
#ifdef sgi
	char *fp = NULL;
#else
	struct packetfilt *fp = NULL;
#endif
	char *icapfile = NULL;
	char *ocapfile = NULL;
	int nflg = 0;
	int Nflg = 0;
	int Cflg = 0;
	int first = 1;
	int last  = 0x7fffffff;
	int ppa;
	int use_kern_pf;
	char *p, *p2;
	char names[256];
	char self[64];
	char *argstr = NULL;
	void (*proc)();
	extern void cap_write();
	extern void process_pkt();
	char *audiodev;

	if (setjmp(jmp_env)) {
		exit(1);
	}

	while ((c = getopt(argc, argv, "at:CPDSi:o:Nn:s:d:vVp:f:c:x:?"))
				!= EOF) {
		switch (c) {
		case 'a':
#ifdef sgi
			audioinit();
#else
			audiodev = getenv("AUDIODEV");
			if (audiodev == NULL)
				audiodev = "/dev/audio";
			audio = open(audiodev, 2);
			if (audio < 0) {
				pr_err("Audio device %s: %m",
					audiodev);
				exit(1);
			}
#endif
			break;
		case 't':
			flags |= F_TIME;
			switch (*optarg) {
			case 'r':	flags |= F_RTIME; break;
			case 'a':	flags |= F_ATIME; break;
			case 'd':	break;
			default:	usage();
			}
			break;
		case 'P':
			Pflg++;
			break;
		case 'D':
			flags |= F_DROPS;
			break;
		case 'S':
			flags |= F_LEN;
			break;
		case 'i':
			icapfile = optarg;
			break;
		case 'o':
			ocapfile = optarg;
			break;
		case 'N':
			Nflg++;
			break;
		case 'n':
			nflg++;
			(void) strcpy(names, optarg);
			break;
		case 's':
			snaplen = atoi(optarg);
			break;
		case 'd':
			device = optarg;
			break;
		case 'v':
			flags &= ~(F_SUM);
			flags |= F_DTAIL;
			break;
		case 'V':
			flags |= F_ALLSUM;
			break;
		case 'p':
			p = optarg;
			p2 = strpbrk(p, ",:-");
			if (p2 == NULL) {
				first = last = atoi(p);
			} else {
				*p2++ = '\0';
				first = atoi(p);
				last = atoi(p2);
			}
			break;
		case 'f':
			(void) gethostname(self, sizeof (self));
			p = strchr(optarg, ':');
			if (p) {
				*p = '\0';
				if (strcmp(optarg, self) == 0 ||
				    strcmp(p+1, self) == 0)
				(void) fprintf(stderr,
				"Warning: cannot capture packets from %s\n",
					self);
				*p = ' ';
			} else if (strcmp(optarg, self) == 0)
				(void) fprintf(stderr,
				"Warning: cannot capture packets from %s\n",
					self);
			argstr = optarg;
			break;
		case 'x':
			p = optarg;
			p2 = strpbrk(p, ",:-");
			if (p2 == NULL) {
				x_offset = atoi(p);
				x_length = -1;
			} else {
				*p2++ = '\0';
				x_offset = atoi(p);
				x_length = atoi(p2);
			}
			break;
		case 'c':
			maxcount = atoi(optarg);
			break;
		case 'C':
			Cflg++;
			break;
		case '?':
		default:
			usage();
		}
	}

	if (argc > optind)
		argstr = (char *) concat_args(&argv[optind], argc - optind);

	/*
	 * Need to know before we decide on filtering method some things
	 * about the interface.  So, go ahead and do part of the initialization
	 * now so we have that data.  Note that if no device is specified,
	 * check_device selects one and returns it.  In an ideal world,
	 * it might be nice if the "correct" interface for the filter
	 * requested was chosen, but that's too hard.
	 */
	if (!icapfile) {
		use_kern_pf = check_device(&device, &ppa);
	}

	if (argstr) {
#ifndef sgi
		if (!icapfile && use_kern_pf && pf_compile(argstr, Cflg)) {
			fp = &pf;
		} else
#endif
		{
			filter++;
			compile(argstr, Cflg);
		}

		if (Cflg)
			exit(0);
	}

	if (nflg) {
		if (access(names, F_OK) == 0) {
			load_names(names);
		} else {
			(void) fprintf(stderr, "%s not found\n", names);
			exit(1);
		}
	}

	if (flags & F_SUM)
		flags |= F_WHO;

	/*
	 * If the -o flag is set then capture packets
	 * directly to a file.  Don't attempt to
	 * interpret them on the fly (F_NOW).
	 * Note: capture to file is much less likely
	 * to drop packets since we don't spend cpu
	 * cycles running through the interpreters
	 * and possibly hanging in address-to-name
	 * mappings through the name service.
	 */
	if (ocapfile) {
		cap_open(ocapfile);
		proc = cap_write;
	} else {
		flags |= F_NOW;
		proc = process_pkt;
	}


	/*
	 * If the -i flag is set then get packets from
	 * the log file which has been previously captured
	 * with the -o option.
	 */
	if (icapfile) {
		names[0] = '\0';
		(void) strcpy(names, icapfile);
		(void) strcat(names, ".names");

		if (Nflg) {
			namefile = fopen(names, "w");
			if (namefile == NULL) {
				perror(names);
				exit(1);
			}
			flags = 0;
			(void) fprintf(stderr,
				"Creating name file %s\n", names);
		} else {
			if (access(names, F_OK) == 0)
				load_names(names);
		}

		if (flags & F_DTAIL)
			flags = F_DTAIL;
		else
			flags |= F_NUM | F_TIME;

		cap_read(first, last, icapfile, filter, proc, flags);

		if (Nflg)
			(void) fclose(namefile);

	} else {
		const int chunksize = 8 * 8192;
		struct timeval timeout;

		/*
		 * If listening to packets on audio
		 * then set the buffer timeout down
		 * to 1/10 sec.  A higher value
		 * makes the audio "bursty".
		 */
		if (audio) {
			timeout.tv_sec = 0;
			timeout.tv_usec = 100000;
		} else {
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
		}

		initdevice(device, snaplen, chunksize, &timeout
#ifndef sgi
			, fp, ppa
#endif
			);

		if (ocapfile)
			show_count();
		net_read(chunksize, filter, proc, flags);

		if (!(flags & F_NOW))
			printf("\n");
	}

	if (ocapfile)
		cap_close();
}

#ifdef sgi

#include <dlfcn.h>
#include <dmedia/audio.h>

ALconfig audioconfig;
ALport audioport;

void *h;

ALconfig (*ALnewconfigf)(void);
void (*ALsetsampfmtf)(ALconfig, long);
void (*ALsetqueuesizef)(ALconfig, long);
void (*ALsetchannelsf)(ALconfig, long);
void (*ALsetwidthf)(ALconfig, long);
ALport (*ALopenportf)(char*, char*, ALconfig);
int (*ALwritesampsf)(ALport, void*, long);

/*
 * Initialize audio port using the Audio Library.
 * Use dlopen()/dlsym() because the dmedia weeners
 * no longer support the static libaudio.a, only the dynamic libaudio.so.1 .
 */
void
audioinit()
{
        char libaudio[50];
#if (_MIPS_SIM == _ABIO32)
	strcpy(libaudio, "/usr/lib/libaudio.so");
#else 
#if (_MIPS_SIM == _ABIN32)
	strcpy(libaudio, "/usr/lib32/libaudio.so");
#else
	strcpy(libaudio, "/usr/lib64/libaudio.so");
#endif
#endif
        if ((h = dlopen(libaudio, RTLD_LAZY)) == NULL) {
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}

	ALnewconfigf = (ALconfig (*)(void)) dlsym(h, "ALnewconfig");
	ALsetsampfmtf = (void (*)(ALconfig, long)) dlsym(h, "ALsetsampfmt");
	ALsetqueuesizef = (void (*)(ALconfig, long)) dlsym(h, "ALsetqueuesize");
	ALsetchannelsf = (void (*)(ALconfig, long)) dlsym(h, "ALsetchannels");
	ALsetwidthf = (void (*)(ALconfig, long)) dlsym(h, "ALsetwidth");
	ALopenportf = (ALport (*)(char*, char*, ALconfig)) dlsym(h, "ALopenport");
	ALwritesampsf = (int (*)(ALport, void*, long)) dlsym(h, "ALwritesamps");

	audioconfig = (*ALnewconfigf)();

	(*ALsetsampfmtf)(audioconfig, AL_SAMPFMT_TWOSCOMP);
	(*ALsetqueuesizef)(audioconfig, 44100);
	(*ALsetchannelsf)(audioconfig, AL_MONO);
	(*ALsetwidthf)(audioconfig, AL_SAMPLE_8);

	if ((audioport = (*ALopenportf)("snoop", "w", audioconfig)) == 0) {
		switch (oserror()) {
		case AL_BAD_NO_PORTS:
			fprintf(stderr, "ALopenport: out of audio ports\n");
			break;

		case AL_BAD_DEVICE_ACCESS:
			fprintf(stderr, "ALopenport: no audio device driver\n");
			break;

		case AL_BAD_OUT_OF_MEM:
			fprintf(stderr, "ALopenport: out of memory\n");
			break;

		default:
			fprintf(stderr, "ALopenport: error\n");
			break;
		}
		exit(1);
	}

	audio = 1;
}
#endif	/* sgi */

int tone[] = {
0x034057, 0x026074, 0x136710, 0x126660, 0x147551, 0x034460,
0x026775, 0x141727, 0x127670, 0x156532, 0x036064, 0x030721,
0x134703, 0x126705, 0x046071, 0x030073, 0x036667, 0x140666,
0x126137, 0x064463, 0x031064, 0x072677, 0x135652, 0x141734,
0x036463, 0x027472, 0x137333, 0x127257, 0x152534, 0x033065,
0x027723, 0x136313, 0x127735, 0x053473, 0x035470, 0x052666,
0x167260, 0x140535, 0x045471, 0x034474, 0x132711, 0x132266,
0x047127, 0x027077, 0x043705, 0x141676, 0x134110, 0x063400,
};

/*
 * Make a sound on /dev/audio according
 * to the length of the packet.  The tone
 * data above is a piece of waveform from
 * a Pink Floyd track. The amount of waveform
 * used is a function of packet length e.g.
 * a series of small packets is heard as
 * clicks, whereas a series of NFS packets
 * in an 8k read sounds like a "WHAAAARP".
 *
 * Note: add 4 constant bytes to sound segments
 * to avoid an artifact of DBRI/MMCODEC that
 * results in a screech due to underrun (bug 114552).
 */
void
click(len)
	int len;
{
	len /= 8;
	len = len ? len : 4;

	if (audio) {
#ifdef sgi
		(*ALwritesampsf)(audioport, (void*)tone, len);
#else
		write(audio, tone, len);
		write(audio, "\377\377\377\377", 4);
#endif
	}
}

/* Display a count of packets */
void
show_count()
{
	static int prev = -1;

	if (count == prev)
		return;

	prev = count;
	(void) fprintf(stderr, "\r%d ", count);
}

/*
 * Display data that's external to the packet.
 * This constitutes the first half of the summary
 * line display.
 */
void
show_pktinfo(flags, num, src, dst, ptvp, tvp, drops, len)
	int flags, num, drops, len;
	char *src, *dst;
	struct timeval *ptvp, *tvp;
{
	struct tm *tm;
	struct tm *localtime();
	static struct timeval tvp0;
	int sec, usec;
	char line[MAXLINE], *lp = line;
	int i, start;

	if (flags & F_NUM) {
		sprintf(lp, "%3d ", num);
		lp += strlen(lp);
	}
	tm = localtime(&tvp->tv_sec);

	if (flags & F_TIME) {
		if (flags & F_ATIME) {
			sprintf(lp, "%d:%02d:%d.%06d ",
				tm->tm_hour, tm->tm_min, tm->tm_sec,
				tvp->tv_usec);
			lp += strlen(lp);
		} else {
			if (flags & F_RTIME) {
				if (tvp0.tv_sec == 0) {
					tvp0.tv_sec = tvp->tv_sec;
					tvp0.tv_usec = tvp->tv_usec;
				}
				ptvp = &tvp0;
			}
			sec  = tvp->tv_sec  - ptvp->tv_sec;
			usec = tvp->tv_usec - ptvp->tv_usec;
			if (usec < 0) {
				usec += 1000000;
				sec  -= 1;
			}
			sprintf(lp, "%3d.%06d ", sec, usec);
			lp += strlen(lp);
		}
	}

	if (flags & F_WHO) {
		sprintf(lp, "%12s -> %-12s ", src, dst);
		lp += strlen(lp);
	}

	if (flags & F_DROPS) {
		sprintf(lp, "drops: %d ", drops);
		lp += strlen(lp);
	}

	if (flags & F_LEN) {
		sprintf(lp, "length: %4d  ", len);
		lp += strlen(lp);
	}

	if (flags & F_SUM) {
		if (flags & F_ALLSUM)
			printf("________________________________\n");

		start = flags & F_ALLSUM ? 0 : sumcount - 1;
		printf("%s%s\n", line, sumline[start]);

		for (i = start + 1; i < sumcount; i++)
			printf("%s%s\n", line, sumline[i]);

		sumcount = 0;
	}

	if (flags & F_DTAIL) {
		printf("%s\n\n", detail_line);
		detail_line[0] = '\0';
	}
}

/*
 * The following two routines are called back
 * from the interpreters to display their stuff.
 * The theory is that when snoop becomes a window
 * based tool we can just supply a new version of
 * get_sum_line and get_detail_line and not have
 * to touch the interpreters at all.
 */
char *
get_sum_line()
{
	sumline[sumcount][0] = '\0';
	return (sumline[sumcount++]);
}

char *
get_detail_line(off, len)
	int off, len;
{
	if (detail_line[0]) {
		printf("%s\n", detail_line);
		detail_line[0] = '\0';
	}
	return (detail_line);
}

/*
 * Print an error.
 * Works like printf (fmt string and variable args)
 * except that it will subsititute an error message
 * for a "%m" string (like syslog) and it calls
 * long_jump - it doesn't return to where it was
 * called from - it goes to the last setjmp().
 */
void
pr_err(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list ap;
	char buf[BUFSIZ], *p2;
	char *p1;
	extern int errno;

	strcpy(buf, "snoop: ");
	p2 = buf + strlen(buf);

	for (p1 = fmt; *p1; p1++) {
		if (*p1 == '%' && *(p1+1) == 'm') {
			char *errstr;

			if ((errstr = strerror(errno)) != (char *) NULL) {
				(void) strcpy(p2, errstr);
				p2 += strlen(p2);
			}
			p1++;
		} else {
			*p2++ = *p1;
		}
	}
	if (p2 > buf && *(p2-1) != '\n')
		*p2++ = '\n';
	*p2 = '\0';

	va_start(ap);
	(void) vfprintf(stderr, buf, ap);
	va_end(ap);
	longjmp(jmp_env, 1);
}

/*
 * Ye olde usage proc
 * PLEASE keep this up to date!
 * Naive users *love* this stuff.
 */
void
usage()
{
	(void) fprintf(stderr, "\nUsage:  snoop\n");
	(void) fprintf(stderr,
	"\t[ -a ]			# Listen to packets on audio\n");
	(void) fprintf(stderr,
	"\t[ -d device ]		# settable to le?, ie?, bf?, tr?\n");
	(void) fprintf(stderr,
	"\t[ -s snaplen ]		# Truncate packets\n");
	(void) fprintf(stderr,
	"\t[ -c count ]		# Quit after count packets\n");
	(void) fprintf(stderr,
	"\t[ -P ]			# Turn OFF promiscuous mode\n");
	(void) fprintf(stderr,
	"\t[ -D ]			# Report dropped packets\n");
	(void) fprintf(stderr,
	"\t[ -S ]			# Report packet size\n");
	(void) fprintf(stderr,
	"\t[ -i file ]		# Read previously captured packets\n");
	(void) fprintf(stderr,
	"\t[ -o file ]		# Capture packets in file\n");
	(void) fprintf(stderr,
	"\t[ -n file ]		# Load addr-to-name table from file\n");
	(void) fprintf(stderr,
	"\t[ -N ]			# Create addr-to-name table\n");
	(void) fprintf(stderr,
	"\t[ -t  r|a|d ]		# Time: Relative, Absolute or Delta\n");
	(void) fprintf(stderr,
	"\t[ -v ]			# Verbose packet display\n");
	(void) fprintf(stderr,
	"\t[ -V ]			# Show all summary lines\n");
	(void) fprintf(stderr,
	"\t[ -p first[,last] ]	# Select packet(s) to display\n");
	(void) fprintf(stderr,
	"\t[ -x offset[,length] ]	# Hex dump from offset for length\n");
	(void) fprintf(stderr,
	"\t[ -C ]			# Print packet filter code\n");
	(void) fprintf(stderr,
	"\n\t[ filter expression ]\n");
	(void) fprintf(stderr, "\nExample:\n");
	(void) fprintf(stderr, "\tsnoop -o saved  host fred\n\n");
	(void) fprintf(stderr, "\tsnoop -i saved -tr -v -p19\n");
	exit(1);
}
