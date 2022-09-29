#ident "$Revision: 1.18 $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * bstream - program to copy from standard input to standard output
 *	at extremely high rates using double buffering.  This version is
 *	fully System V compatabile.  9/25/87  J. M. Barton
 *
 *  -b size	buffer size, in Kbytes
 *  -n cnt	number of buffers to use
 *  -l		attempt to lock segments in core
 *  -t		attempt to lock process as well
 *  -d		turn on debugging
 *  -v		turn on buffer use reporting
 *  -s bufs	statistics every "bufs" buffers
 *  -r		summary of passthrough
 *  -i file	file to stream from
 *  -o file	file to stream to
 *
 * If files not specified, use standard input and output appropriately.
 *
 * $Log: bstream.c,v $
 * Revision 1.18  1998/07/21 19:00:38  rwu
 * Change buffer size type to allow more than 2GB.
 *
 * Revision 1.17  1992/12/11 15:41:17  wtk
 * Correction to allow build
 *
 * Revision 1.16  1992/11/17  10:02:49  ism
 * Fix compile errors
 *
 * Revision 1.15  91/12/03  00:16:07  olson
 * Use waitpid instead of wait when waiting for child; my memory is fuzzy now,
 * but it fixed an obscure bug, possibly relating to pipelines.
 * 
 * Revision 1.14  91/06/21  14:30:41  jojo
 * Handles new tape driver definition of end of tape.
 * 
 * Revision 1.13  91/02/05  23:19:44  olson
 * make usage message more useful
 * 
 * Revision 1.12  91/02/05  21:37:44  olson
 * changed a number of error messages to use perror; use HZ instead of
 * hardcoding 100 or 64.  linebuffer after initial error checking so that
 * messages from input and output sides do not get intermixed.
 * 
 * Revision 1.11  90/10/26  17:57:19  bowen
 * Changed appropriate instances of strtol to strtoul.
 * 
 * Revision 1.10  90/10/24  15:37:36  bowen
 * Fixed prototype warnings.
 * 
 * Revision 1.9  89/10/27  10:10:19  bh
 * (textually a change to 1.7)
 * Backed out -B mods as they caused the following bug:
 * 	if amount to write mod 16k >512 and <16k and not a mult of 512
 * 	then last partial (16k) block is not written (only when output is tape)
 * (-B was undocumented)
 * an fprintf in copyout was missing a %s arg causing seg faults in the
 * child process causing the parent to hang (waiting forever for a message)
 * 
 * Revision 1.7  88/02/23  10:00:05  eva
 * added signal handlers for SIGINT, SIGQUIT and SIGTERM that clean up the
 * msg queue, and added a copyright statement to the top of the file.
 * 
 * Revision 1.7  88/02/23  09:58:53  eva
 * Added signal handlers for SIGINT, SIGQUIT and SIGTERM that cleans up the
 * msg queue, and put a copyright statement at the top of the file.
 * 
 * Revision 1.6.1.1  88/02/23  09:53:26  eva
 * auto-integrate
 * 
 * Revision 1.6  87/11/27  11:36:55  jmb
 * Added non-block-aligned write zero fill feature, cleaned up handling of
 * small files, added parent wait for child to finish writing tape, cleaned
 * up handling of end-of-file conditions on tape.
 * 
 * Revision 1.5.1.1  87/11/27  11:32:10  jmb
 * auto-integrate
 * 
 * Revision 1.5  87/11/05  12:03:29  jmb
 * Added comments and the fullbuf hysterisis code.
 * 
 * Revision 1.4  87/09/30  18:12:13  jmb
 * Working version of bstream.  Checking in to modify again!
 * 
 * Revision 1.3  87/08/29  09:57:49  jmb
 * First tested version, handles multiple tapes.
 * Streaming performance is about 77Kb/sec.
 * 
 * Revision 1.2  87/08/26  15:20:47  jmb
 * Comments?  Comments?  We don't need no stinkin' comments!
 * 
 */

# include	<sys/types.h>
# include	<sys/param.h>
# include	<sys/ipc.h>
# include	<sys/shm.h>
# include	<sys/msg.h>
# include	<sys/lock.h>
# include	<sys/times.h>
# include	<sys/mtio.h>

# include	<signal.h>
# include 	<stdio.h>
# include	<errno.h>
# include	<fcntl.h>

# define	MAXNBUF		6	/* depends on SHMSEG kernel constant */

char		*pname;			/* program name */
char		*iname = 0;		/* input file name */
char		*oname = 0;		/* output file name */
long		bsize = 16 * 1024;	/* buffer size */
int		nbuf = 4;		/* number of buffers */
int		shm[MAXNBUF];		/* buffer descriptors */
char		*buf[MAXNBUF];		/* buffer locations */
long		*len[MAXNBUF];		/* length of a buffer */
char		legendbuf[20];		/* for printing nicely */
int		msg;			/* message queue descriptor */

char		lock = 0;		/* lock buffers in core */
char		lprog = 0;		/* lock program in core */
char		report = 0;		/* statistics */
char		istape = 0;		/* if a tape device */
int		blocksize = 512;	/* tape block size */
int		blockmask = 0x1ff;	/* block size mask */
long		stats = 0;		/* buffer statistics */
FILE		*log = 0;		/* debugging log */
int		proc;			/* child process ID */
off_t		nbytes;			/* buffer size */
char		summary = 0;		/* summary report */
struct tms	tb;			/* start time */
struct tms	ta;			/* end time */
long		ustart;			/* start tick value */
long		uend;			/* end tick value */
int		inout;			/* my I/O direction */

extern int	errno;			/* system call error */
void copyin(void);
void copyout(void);
void goodbye();		/* handler to clean up msg q */


main(argc, argv)
   int		argc;
   char		*argv[];
{
   extern int	optind;
   extern char	*optarg;
   int		c;
   int		err = 0;
   int		i;

	pname = argv[0];
	while ((c = getopt(argc, argv, "i:o:rs:vn:b:ltd")) != EOF) {
		switch (c) {
		case 'r':
			summary++;
			break;
		case 'b':
			bsize = strtoul(optarg, (char **) 0, 0) * 1024;
			break;
		case 's':
			stats = strtoul(optarg, (char **) 0, 0);
			if (stats <= 1) {
				fprintf(stderr,
					"%s: stat size must > 1\n",
					pname);
				exit(1);
			}
			break;
		case 'i':
			iname = optarg;
			break;
		case 'o':
			oname = optarg;
			break;
		case 'l':
			lock++;
			break;
		case 't':
			lprog++;
			break;
		case 'd':
			log = stderr;
			break;
		case 'v':
			report++;
			break;
		case 'n':
			if ((nbuf = strtoul(optarg, (char **) 0, 0)) < 2 ||
			     nbuf > MAXNBUF) {
				fprintf(stderr,
				"%s: nbuf must be > 2 and < %d\n", pname, nbuf);
				exit(1);
			}
			break;
		case '?':
			err++;
			break;
		}
	}
	if (err) {
		fprintf(stderr, "usage: %s [-n bufs] [-b size] [-ltrvd] [-s statcnt] [-i file] [-o file]\n", pname);
     
		exit(1);
	}
	for (i = optind; i < argc; i++) {
		fprintf(stderr, "%s: extra argument %s ignored\n", pname,
			argv[i]);
	}
	setbuf(stderr, NULL);
	nbytes = 0;
	setupbuf();
	if (log)
		fprintf(log, "using %d byte buffers\n", bsize);
	if (lprog)
		if (nice(-10) == -1 && log)
			fprintf(log, "can't increase process priority\n");
	if ((proc = fork()) == 0) {
		if (log)
			fprintf(log, "child waiting for parent\n");
		if (lprog)
			plock(PROCLOCK);
		inout = 1;
		if (summary) {
			ustart = times(&tb);
			strcpy(legendbuf, "output");
		}
		copyout();
		if (summary)
			printrate(0, legendbuf);
		msgctl(msg, IPC_RMID, 0);
		exit(0);
	}
	else if (proc == -1) {
		fprintf(stderr, "%s: can't fork\n", pname);
		msgctl(msg, IPC_RMID, 0);
		exit(1);
	}
	/* CATCH signal so can remove msg_q */
	signal(SIGINT, goodbye);	 /* ignore */
	signal(SIGQUIT, goodbye); /* ignore */
	signal(SIGTERM, goodbye); /* cleanup and quit */
	if (lprog)
		if (plock(PROCLOCK) == -1) {
			fprintf(stderr, "%s: can't lock process in core\n",
				pname);
		}
	inout = 0;
	if (summary) {
		ustart = times(&tb);
		strcpy(legendbuf, "input");
	}
	copyin();
	if (summary)
		printrate(0, legendbuf);
	if(waitpid(proc, NULL, 0) == -1)
		perror("Child bstream process could not be waited for");
		
	exit(0);
}

/*
 * Copyin and copyout are really quite simple.  copyin() is run by the parent,
 * and it's task is to copy the input to the buffers.  As each buffer is
 * filled, copyin() sends a message to the child informing it of the full
 * buffer.  copyout() is run by the child.  It's job is to copy out
 * of the buffers to the output.  As each buffer is emptied, it sends a
 * message to the parent informing it that a buffer is available for input.
 */
void
copyin(void)
{
   register long	cnt;
   register long	nb;
   register long	bleft;
   register int		i;
   register int		done = 0;

	if (log)
		fprintf(log, "parent starting\n");
	if (iname) {
		close(0);
		if (open(iname, O_RDONLY) == -1) {
			fprintf(stderr, "%s: can't open input ", pname);
			perror(iname);
			goodbye();
		}
	}
	istape = isatape();

	while (1) {
		i = getfreebuf();
		cnt = 0;
		while ((cnt < bsize) && !done) {
			bleft = bsize - cnt;
			nb = read(0, buf[i]+cnt, bleft);
			/* check for eom */
			if ((nb == -1) || (istape && (nb == 0))) {

				if ((nb == -1) && !((errno == ENXIO) && istape)) {
					fprintf(stderr, "%s: read ", pname);
					perror("error");
					goodbye();
				}
				nb = chkinout(buf[i]+cnt, bleft);
				/* End of the tape? */
				if (nb == -1) {
					done = 1;
					break;
				}
				if (nb == 0) {
					fprintf(stderr,
						"%s: read recovery failed\n",
						pname);
					goodbye();
				}
				else {
					cnt += nb;
					continue;
				}
			}
			if (log)
				fprintf(log, "parent reads %d bytes to %#x\n",
					nb, buf[i]+cnt);
			if (nb == 0)
				break;
			cnt += nb;
		}
		nbytes += cnt;
		*(len[i]) = cnt;
		fullbuf(i);
		if (report)
			fprintf(stderr, "--> %d #%d\n", cnt, i);
		if (cnt == 0)
			return;
	}
}

void
copyout(void)
{
   register long	out;
   register long	cnt;
   register long	nb;
   register int		i;

	if (log)
		fprintf(log, "child starting\n");
	if (oname) {
		close(1);
		if (open(oname, O_WRONLY) == -1) {
			fprintf(stderr, "%s: can't open output ", pname);
			perror(oname);
			goodbye();
		}
	}
	istape = isatape();

	while (1) {
		i = getfullbuf();
		if ((out = *(len[i])) == 0) {
			if (log)
				fprintf(log, "no more data to be passed\n");
			return;
		}
		cnt = 0;
		while (out > 0) {
			if (istape && (out & blockmask) != 0) {
			   register int		k;
			   register int		l;

				k = cnt + out;
				l = (k + blocksize) & ~blockmask;
				for (; k < l; k++)
					*(buf[i]+k) = 0;
				out = (out + (blocksize - 1)) & ~blockmask;
				fprintf(stderr,
		"%s: warning: last tape write rounded up to tape block size\n",
					pname);
			}
			if ((nb = write(1, buf[i]+cnt, out)) == -1) {
				/*
				 * This code is dependent on the brain-damaged
				 * end-of-medium handling in the tape
				 * driver.  Check this area if problems when
				 * porting.
				 */
				if (!(istape && ((errno == ENXIO) || (errno == ENOSPC)))) {
					fprintf(stderr, "%s ", pname);
					perror("write error on output");
					goodbye();
				}
				nb = chkinout(buf[i]+cnt, out);
				if (nb == 0) {
					fprintf(stderr, "%s: write recovery failed\n",
						 pname);
					goodbye();
				}
			}
			if (log)
				fprintf(log, "child wrote %d bytes from %#x\n",
					nb, buf[i]+cnt);
			cnt += nb;
			out -= nb;
		}
		nbytes += cnt;
		if (report)
			fprintf(stderr, "<-- %d #%d\n", cnt, i);
		freebuf(i);
	}
}

/*
 * This routine goes through the rather gross setup needed to create the
 * buffers and message queue.  Since we can get the OS to automatically
 * dispose of the buffers for us on exit, we do so.  Too bad messages
 * don't have the same property.
 */
setupbuf()
{
   int 		i;

	bsize += sizeof(long);
	for (i = 0; i < nbuf; i++) {
		if ((shm[i] = shmget(IPC_PRIVATE, bsize, IPC_CREAT|0600))
			== -1) {
		   register int	j;

			fprintf(stderr,
				"%s: can't allocate %d byte segment %d\n",
				pname, bsize, i);
			perror(pname);
			for (j = 0; j < i; j++)
				shmctl(shm[j], IPC_RMID, 0);
			exit(1);
		}
	}
	bsize -= sizeof(long);

	for (i = 0; i < nbuf; i++) {
		if ((buf[i] = (char *) shmat(shm[i], 0, 0))
			== (char *) -1) {
			fprintf(stderr,
				"%s: can't attach to shared segment %d\n",
				pname, i);
			perror(pname);
			for (i = 0; i < nbuf; i++)
				shmctl(shm[i], IPC_RMID, 0);
			exit(1);
		}
		len[i] = (long *) (buf[i] + bsize);
		if (log)
			fprintf(log, "segment %d attached at %#x\n", i, buf[i]);
	}

#ifdef mips
	if (lock) {
		for (i = 0; i < nbuf; i++)
			if (shmctl(shm[i], SHM_LOCK, 0) == -1)
				fprintf(stderr,
				"%s: warning: can't lock shared segments\n",
					pname);
	}
#endif
	for (i = 0; i < nbuf; i++)
		shmctl(shm[i], IPC_RMID, 0);
	if ((msg = msgget(IPC_PRIVATE, IPC_CREAT|0660)) == -1) {
		fprintf(stderr, "%s: can't allocate message queue\n", pname);
		perror(pname);
		exit(1);
	}
	for (i = 0; i < nbuf; i++)
		freebuf(i);
}

void
goodbye()
{
	msgctl(msg, IPC_RMID, 0);
	if(summary && nbytes>0)
	/* print summary even on errors, if anything was done.  This
	 * is useful when copying to devices like disks that hit
	 * the end of the disk and return ENOSPC, etc. */
		printrate(0, legendbuf);
	if (proc)
		kill(proc, SIGTERM);
	else
		kill(getppid(), SIGTERM);
	exit(1);
}

/*
 * The following routines deal with the message queue, and implement the
 * buffer management protocol.  At initialization, messages for the number
 * of available buffers are placed on the queue as empty buffers.  The
 * reader simply blocks waiting for one of these messages.  When one arrives,
 * he fills the buffer and then sends a buffer full message.  The writer
 * blocks on the queue waiting for buffer full messages.  As he gets each
 * one, he writes the buffer and then enters it back on the queue as empty.
 * Thus, once started, the management is self-sustaining.  A criticial note:
 * this code relies on the message queue staying ordered.
 */

# define	MSGSZ		(sizeof(pbuf)-sizeof(long))
# define	DOBUF		1
# define	FREEBUF		2

typedef struct {
	long	m_type;
	int	m_buf;
} bufmsg_t;

fullbuf(sp)
   int		sp;
{
   bufmsg_t		pbuf;
   static long		lstat = 0;
   static struct tms	tbefore;
   static long		tstart;
   struct tms		tafter;
   long			tend;
   long			bufs;

	if (log)
		fprintf(log, "buf %d full\n", sp);
	pbuf.m_type = DOBUF;
	pbuf.m_buf = sp;
	bufs = *(len[sp]);
	if (msgsnd(msg, (const struct msgbuf *)&pbuf, MSGSZ, 0) == -1) {
		fprintf(stderr, "%s: message send %d ", pname, sp);
		perror("failed");
		goodbye();
	}
	if (stats) {
		if (lstat == 0)
			tstart = times(&tbefore);
		lstat++;
		if (lstat > stats) {
			tend = times(&tafter);
			lstat *= bsize;
			fprintf(stderr, "IN: %d Kbytes/sec\n", 
				((lstat / (tend - tstart)) * HZ) / 1024);
			lstat = 0;
		}
	}
}

freebuf(sp)
   int		sp;
{
   bufmsg_t	pbuf;
   static long		lstat = 0;
   static struct tms	tbefore;
   static long		tstart;
   struct tms		tafter;
   long			tend;
   long			bufs;

	if (log)
		fprintf(log, "buf %d freed\n", sp);
	pbuf.m_type = FREEBUF;
	pbuf.m_buf = sp;
	bufs = *(len[sp]);
	if (msgsnd(msg, (const struct msgbuf *)&pbuf, MSGSZ, 0) == -1) {
		fprintf(stderr, "%s: message send %d ", pname, sp);
		perror("failed");
		goodbye();
	}
	if (stats) {
		if (lstat == 0)
			tstart = times(&tbefore);
		lstat++;
		if (lstat > stats) {
			tend = times(&tafter);
			lstat *= bsize;
			fprintf(stderr, "OUT: %d Kbytes/sec\n", 
				((lstat / (tend - tstart)) * HZ) / 1024);
			lstat = 0;
		}
	}
}

getfullbuf()
{
   bufmsg_t	pbuf;
   static int	bufcache[MAXNBUF];
   static int	ncache = 0;
   int		half = nbuf / 2;

	/*
	 * This routine attempts to add some hysterisis to the output,
	 * to help insure maximum overlap between input and output.
	 * This is done by not starting to write buffers until at least
	 * half are full, which helps if the reader is slower than the
	 * writer, making the output flow stop less often.  Otherwise,
	 * the writer could just chase the reader around the buffer
	 * list, having to wait on each new buffer.
	 */
	if (ncache > 0)
		return(bufcache[--ncache]);
	ncache = half;
	while (ncache > 0) {
		if (msgrcv(msg,(struct msgbuf *)&pbuf,MSGSZ,DOBUF,0) == -1) {
			fprintf(stderr, "%s: message receive ", pname);
			perror("failed");
			goodbye();
		}
		bufcache[--ncache] = pbuf.m_buf;
		if (*(len[pbuf.m_buf]) == 0)
			break;
	}
	ncache = half;
	return(bufcache[--ncache]);
}

getfreebuf()
{
   bufmsg_t	pbuf;

	if (log)
		fprintf(log, "waiting for free buffer ...\n");
	if (msgrcv(msg, (struct msgbuf *)&pbuf, MSGSZ, FREEBUF, 0) == -1) {
		fprintf(stderr, "%s: message receive ", pname);
		perror("failed");
		goodbye();
	}
	if (log)
		fprintf(log, "got empty buffer %d\n", pbuf.m_buf);
	return(pbuf.m_buf);
}

/*
 * This code implements the multi-volume tape handling strategy.
 * If the driver gets smarter and tells us more, this routine can
 * shrink and become more intelligent.  For instance, it would be REALLY
 * nice if re-setting the driver really worked, so we didn't have to
 * explicitly know the tape special file name.
 */
chkinout(where, amount)
   char		*where;
   int		amount;
{
   FILE		*id;
   char		buf[BUFSIZ];
   char		*s;
   int		nb;
   static int	ntapes = 1;
   int		nmt;
   char		*td;

	if (log)
		fprintf(stderr, "checking for tape input/output\n");

	if (!isatty(2))
		return(0);
	if ((td = (inout?oname:iname)) == 0)
		return(0);

	if ((id = fopen("/dev/tty", "r")) == NULL)
		return(0);
	if (summary) {
		sprintf(legendbuf, "tape %d", ntapes++);
		printrate(1, legendbuf);
		sprintf(legendbuf, "tape %d", ntapes);
	}

	close(inout);
    gettape:
	fprintf(stderr, "<%s> Change tape and type RETURN or \"quit\":", td);
	fflush(stderr);
	if (fgets(buf, BUFSIZ, id) == NULL) {
		fprintf(stderr, "%s: end of input on tty\n", pname);
		goodbye();
	}
	for (s = buf; *s != '\0' && (*s == ' ' || *s == '\t'); s++);
	if (strncmp(s, "quit", 4) == 0 || strncmp(s, "q", 1) == 0) {
		fclose(id);
		return(-1);
	}
	else if (*s != '\n')
		fprintf(stderr, "%s: extra arguments ignored; re-trying\n",
			pname);
	fclose(id);
	if (open(td, (inout?O_WRONLY:O_RDONLY)) == -1) {
		fprintf(stderr, "%s: can't open drive %s\n", pname, td);
		goto gettape;
	}

    retry:
	if (inout) {
		if ((nb = write(1, where, amount)) < 0) {
			fprintf(stderr, "%s: write ", pname);
			perror("error");
			goodbye();
		}
	}
	else {
		if ((nb = read(0, where, amount)) < 0) {
			fprintf(stderr, "%s: read ", pname);
			perror("error");
			goodbye();
		}
	}
	return(nb);
}

printrate(upd, legend)
   int		upd;
   char		*legend;
{
   register double	utime;
   register double	stime;
   register double	ticks;

	uend = times(&ta);
	ticks = uend - ustart;
	utime = (ta.tms_utime - tb.tms_utime);
	stime = (ta.tms_stime - tb.tms_stime);
	/* called late, so no worry about error messages without
	 * newlines; if we don't do this, sometimes the in and out
	 * messages get intermixed. */
	setlinebuf(stderr);
	fprintf(stderr, "%s: %s: %lld Kb %lld b/s, %%%2.2f u, %%%2.2f s\n",
		pname, legend, (nbytes/1024), (off_t)((nbytes / ticks) * HZ),
		((utime / ticks) * HZ),
		((stime / ticks) * HZ));
	nbytes = 0;
	if (upd)
		ustart = times(&tb);
}

isatape()
{
   struct mtop	mtb;

	mtb.mt_op = MTNOP;
	mtb.mt_count = 1;
	if (ioctl(inout, MTIOCTOP, &mtb) == -1)
		return(0);
	return(1);
}
