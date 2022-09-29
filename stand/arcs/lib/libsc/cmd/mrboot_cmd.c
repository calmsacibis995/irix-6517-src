#ident "lib/libc/cmd/mrboot_cmd.c: $Revision: 1.86 $"

/*
 * Mini-root Boot
 *
 */


#include <setjmp.h>
#include <sys/types.h>
#include <sys/dvh.h>
#include <saioctl.h>
#include <sys/dkio.h>
#include <parser.h>
#include <pause.h>
#include <arcs/io.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <stdarg.h>
#include <gfxgui.h>
#include <style.h>
#include <libsc_internal.h>
#include <ctype.h>
#include <sys/param.h>
#include <arcs/time.h>

#define MAXFNLEN 128
#define UNIX_DIR_LOCATION "miniroot/unix."
#define CDROM_LOCATION "dist/"

static char *gettapehelp = "Please enter the name of the server on which the tape will be used.";

static jmp_buf jb;
static jmp_buf cancelljb;

extern char *make_bootfile(int);
extern char *inv_findcpu(void);

void mr_boot(char *, char *, int, char **);
static void mr_copy(char *, char *);
static void mr_getsource(char *);
static int mr_getdest(char *, int);
static void mr_giveup(char *, ...);
static void mr_msg_progress(int, int, int);
static void mr_msg_start(void);
static void mr_msg_done(void);
static int arcs_to_dsk(char *arcs, char *dsk);
static int mr_bootable(char *source);
int mr_interrupted_inst(int ismrb, int iscustom);

/* can be called from sash's main also */
void mr_setinst_state(int which);
int mr_iscustom(int ac, char** av);

#define BOOTABLE_NOT    0
#define BOOTABLE_KERN   1

/*
 * Copy the mini-root from local or remote tape to the swap area and 
 * boot it.
 *
 */

/*ARGSUSED*/
int
mrboot(int ac, char **av, char **bunk1, struct cmd_table *bunk2)
{
	char source[MAXFNLEN], dest[MAXFNLEN];

	if (setjmp(jb)) {
		p_textmode();
		return 0;
	}

	if (!doGui())
		p_panelmode();

	/* 
	 * If mr_getdest returns 0 the normal happens ... we copy 
	 * the "mr" to the swap partition.  Otherwise the swap and root 
	 * are on same partiton, so probably an interrupted install, and the 
	 * user may choose (see mr_getdest()) to continue the boot onto 
	 * the existing miniroot.  If the user decides to use the existing
	 * miniroot, there is no need to reload the "mr" on to the swap
	 * partition again ... thus just boot the kernel and off we go.
	 *
	 */
	if (Debug) printf("mrboot\n");

	if (mr_getdest(dest, mr_iscustom(ac,av)) == 0) {
		/* Now get source location */
		/* source = /usr/local/boot/sa(mr) */
		mr_getsource(source);

		/* copy source to destination */
		if (Debug) printf("source : %s\ndest : %s\n", source, dest);

		/* dest = swap parition on disk */
		mr_copy(source,dest);
	}

	/* boot from destination */
	mr_boot(source, dest, ac, av);

	return 0;
}
/*
 * copy the mini-root to swap
 *
 */
#define MR_CP_BUFSIZ 32768

static void
mr_copy(char *from, char *to)
{
	ULONG src_fd, dst_fd, bcnt, wcnt;
	char *buf, *mbuf;
	int tries = 0;
	int lp,lt;
	long rc;

	buf = 0;
	src_fd = dst_fd = (ULONG) -1;

	/* "Copying installation program to disk" */
	mr_msg_start();

	if ( setjmp(cancelljb) ) {
		if ( src_fd != (ULONG) -1 )
			Close(src_fd);
		if ( dst_fd != (ULONG) -1 )
			Close(dst_fd);
		if ( buf )
			dmabuf_free(buf);
		longjmp(jb,1);
		return;
	}

	/* If we get an EBUSY it means that the tape is still rewinding.
	 * The older code always wait 20 seconds first, but this is silly
	 * if loading over the net or from CDROM.  So now print message only
	 * on 2nd and later sleeps.  The bootp check is because some tftpd's
	 * might not pass back the EBUSY, so we always try twice on failures
	 * from bootp.  Thus bootp will still get a long delay if there
	 * was a typo...  Before it waited at most 95 seconds; it turns
	 * out that in some cases a tape at the 'wrong' position can
	 * take longer than this.. */
	while ((rc = Open((CHAR *)from, OpenReadOnly, &src_fd)) != ESUCCESS
		&& tries++ < 9 
		&& (rc==EBUSY || (strstr(from,"bootp")!=0 && tries==1))) {
		if ( tries > 1 )
			printf("Waiting for the tape to become available...  ");
		if (pause(20, "", "\033") == P_INTERRUPT)
			mr_giveup("Procedure canceled\n");
		}
	
	if ( tries > 1 && !isGuiMode())
		printf("\n");

	if (rc != ESUCCESS)
		mr_giveup("Unable to open source %s.\n"
			  "%s.\n", from, arcs_strerror(rc));

	if ( (rc = Open((CHAR *)to, OpenWriteOnly, &dst_fd)) != ESUCCESS ) {
		Close(src_fd);
		mr_giveup("Unable to open destination %s.\n"
			  "%s.\n", to, arcs_strerror(rc));
		/*NOTREACHED*/
	}

	lp = lt = 0;

	/*  Round up to BLKSIZE so copies to disks do not have to buffer */
	mbuf = dmabuf_malloc(MR_CP_BUFSIZ + BLKSIZE);
	buf = (char *)(((__psunsigned_t)mbuf+BLKSIZE-1)&~(BLKSIZE-1));

	for(;;)	{
		FILEINFORMATION fi;

		fi.EndingAddress.lo = 0;

		if ( (GetFileInformation(src_fd, &fi) == ESUCCESS) &&
		     fi.EndingAddress.lo != 0 ) {
			int tenxper, per, tenth;

			/* still get right proportion on deltas over 1000
			 * (less than 1/10 percent) on miniroot, but avoid
			 * overflow of 32 bit number.
			 */
			tenxper = (int)(fi.CurrentAddress.lo /
				(fi.EndingAddress.lo/1000));
			per = tenxper / 10;
			tenth = tenxper % 10;

			if (lp != per || lt != tenth)
				mr_msg_progress(per,tenth,lp);

			lp = per;
			lt = tenth;
		}
		else
			mr_msg_progress(1,0,0);

		if ( (rc=Read(src_fd, buf, MR_CP_BUFSIZ, &bcnt)) != ESUCCESS) {
			Close(src_fd);
			Close(dst_fd);
			dmabuf_free(mbuf);
			mr_giveup("\nRead error %d: %s\n",rc,arcs_strerror(rc));
			/*NOTREACHED*/
		}
		else if ( bcnt == 0 ) {
			if ( (GetFileInformation(src_fd, &fi) == ESUCCESS) &&
			      (fi.EndingAddress.lo != 0) &&
			      (fi.CurrentAddress.lo != fi.EndingAddress.lo) ) {
				Close(src_fd);
				Close(dst_fd);
				dmabuf_free(mbuf);
				mr_giveup("\nShort copy: expected %d bytes, "
					  "recieved %d bytes\n",
					  fi.EndingAddress.lo,
					  fi.CurrentAddress.lo);
			}
			break;
		}

		if (((rc = Write(dst_fd, buf, bcnt, &wcnt)) != ESUCCESS) ||
		    (bcnt != wcnt)) {
			Close(src_fd);
			Close(dst_fd);
			dmabuf_free(mbuf);
			mr_giveup("\nWrite error %d: %s\n",rc,
				  arcs_strerror(rc));
			/*NOTREACHED*/
		}


	}

	mr_msg_done();

	Close (src_fd);
	Close (dst_fd);
	dmabuf_free(mbuf);

	return;
}

/*
 * Boot the mini root
 *
 */
void
mr_boot(char *source, char *dest, int ac, char **av)
{
	char unixname[MAXFNLEN];
	char device[MAXFNLEN];
	char rootbuf[32];
	char *argv[16];
	int argc = 0;
	int bootable;
	int i;

	/* set device array ... it may change in mr_bootable. */

	strcpy(device,"tapedevice=");
	strcat(device,source);
	
	/* Is this image bootable? 
	 *
	 * Choices :
	 * 	BOOTABLE_KERN - Use the kernel in a dir (ie. miniroot/unix.IP*)
	 *		source = network(0)bootp()machine:/usr/local/boot/miniroot/unix.IP*
	 * 	or if off a local CDROM ...
	 *		source = scsi(0)cdrom(6)partition(8)(mr)
	 *		There are two different partitions for CD-ROM :
	 *			partition(8) (vol header) only has io4prom, 
	 *				and sash* (sash64, sashARCS, sashIP17)
	 *			partition(7) (rest of disk) only has sa, 
	 *				miniroot, and eoe*, etc ... images 
	 */

	bootable = mr_bootable(source);

	switch (bootable) {
	case BOOTABLE_KERN:
		/* use source from mr_bootable ... */ 
		strcpy(unixname,source);
		break;
	case BOOTABLE_NOT:	/* this now happens only when rebooting an
		* existing miniroot, so don't tack on the inv_findcpu(),
		* like we used to do; it's only in miniroot as /unix, if at all */
		sprintf(unixname, "%s/unix", dest);
		break;
	}

	argv[argc++] = "boot";
	argv[argc++] = "-f";
	argv[argc++] = unixname;
	argv[argc++] = device;

	/*  Include root specification so install works automatically when
	 * system disk is not dksc(0,1)
	 */
	strcpy(rootbuf,"root=");
	if (arcs_to_dsk(dest,rootbuf+5))
		argv[argc++] = rootbuf;

	for (i = 0; i < ac && argc < 15; i++ )
		argv[argc++] = av[i];

	argv[argc] = 0;

#ifdef DEBUG
	p_printf("%s %s %s %s\n",argv[0], argv[1], argv[2], argv[3]);
#endif

	mr_setinst_state(0);

	boot(argc,argv,argv,0);

	/* Shouldn't get here */
	mr_giveup("Failed to boot\n");
	/*NOTREACHED*/
}

/*
 * Find the installation tape
 *
 */
static void
mr_getsource(char *source)
{
	char *tape;
	extern char *inv_findtape(void);

	/*
	 * The user was prompted in the proms for the
	 * source of the installation.  The proms should have
	 * passed it to us as the environment variable "tapedevice".
	 */
	if ( tape = getenv("tapedevice") )
		strcpy(source,tape);
	else
	/* If the machine has a tape device, assume it will be used. */
	if ( !getenv("notape") && (tape = inv_findtape()) )
		strcpy(source,tape);
	else
	/* If no local tape, then assume network installation. */
	for(;;) {
		char response[LINESIZE];
		p_curson();
		printf("Enter the name of the server with the tape drive: ");
		gets(response);
		/*sa_get_response( response );*/
		if ( response[0] == '?' ) {
			printf("%s\n", gettapehelp);
			continue;
			}
		if ( ! response[0] || response[0] == 4 /* Ctrl-d */ ) {
			p_cursoff();
			mr_giveup("Procedure Canceled\n");
			/*NOTREACHED*/
			}
		if ( ! index(response,'(') )
			strcpy(source,"bootp()");
		strcat(source,response);

		/* If the user gives a pathname after the machine name,
		   use it.  Otherwise assume /dev/tape. */

		if ( ! index(response,':') ) {
			strcat(source,":/dev/tape");
			if (isgraphic(1)) p_printf(
		"Insert the installation media, then press \001 Enter \006 ");
			else p_printf(
			"Insert the installation media, then press <enter>: ");
			gets(response);

			/* If the user types anything, abort */
			if (response[0])
				mr_giveup("Procedure canceled\n");
			}
		setenv("tapedevice",source);
		break;
		}

	/* Mini-root is machine independent */
	strcat(source,"(mr)");
}

/*
 * Find the swap area of the boot disk
 *
 */
static int
mr_getdest(char *dest, int iscustom)
{
	char *oboot = make_bootfile(1);
	char *osopts;
	char *dp = dest;
	char *cp;
	struct volume_header vh;
	ULONG fd;
	long rc;

	if ( ! oboot ) {
		/* XXX look for local disk controller */
		p_printf("bootfile not set, using default.\n");
		oboot = "scsi()disk(1)rdisk()partition(8)/sash";
		}

	/* trim off filename portion of bootfile to get device */
	/* dest is used as a temporary buffer */
	strcpy(dest,oboot);
	if ( !(cp = rindex(dest, ')')) )
		mr_giveup("Bad device format in $SystemPartition\n");

	*(cp+1) = '\0';

	if ( (rc = Open((CHAR *)dest, OpenReadOnly, &fd)) != ESUCCESS )
		mr_giveup("Unable to open \"%s\": %s\n",dest,
			  arcs_strerror(rc));

	if ( ioctl(fd, DIOCGETVH, (long)&vh) == -1 ) {
		Close(fd);
		mr_giveup("Unable to read volume header of \"%s\"\n",dest);
		/*NOTREACHED*/
	}

	Close(fd);

	if (vh.vh_swappt < 0 || vh.vh_swappt >= NPARTAB )
		mr_giveup("swap partition (%d) is not a valid partition.\n",
				vh.vh_swappt);

	
	if (vh.vh_pt[vh.vh_swappt].pt_type != PTYPE_RAW )
		mr_giveup("swap partition (%d) is not a valid swap area.\n",
				vh.vh_swappt);

	/* get device name */
	if (!strncmp(oboot, "dksc(", 4)) {
		/* Parse old style name */
		dp = NULL;
		cp = oboot;
		while (cp = index(cp, ',')) {
			dp = cp;
			cp++;
		}
		if (!dp)
			goto badname;
		dp++;
		bcopy(oboot, dest, (int)((long)dp - (long)oboot));
		dp = dest + ((long)dp - (long)oboot);
	} else { 

		/* Parse new arcs-style name */	
		cp = oboot;
		while (cp = index(cp, 'p')) {
			if (!strncmp(cp, "partition", 9))
				break;
			cp++;
		}
		if (!cp)
			goto badname;

		/* got xxxx()partition
		 */
		if (!(cp = index(cp, '(')))
			goto badname;
		++cp;

		bcopy(oboot, dest, (int)((long)cp - (long)oboot));
		dp = dest + ((long)cp - (long)oboot);

		/* just test for closing paren
		 */
		if (!index(cp, ')'))
			goto badname;
	}
	*dp++ = (char)(vh.vh_swappt + '0');
	*dp++ = ')';
	*dp++ = '\0';

	/* were we already in the middle of an install, that failed somehow? */
	osopts = getenv("OSLoadOptions");
	if(osopts && !strncmp("INST", osopts, 4) && mr_interrupted_inst(1, iscustom)==1)
		return 1;	/* they chose to abort the install */	

	return 0;

badname:
	mr_giveup("Invalid characters in disk name \"%s\".\n", dest);
	/*NOTREACHED*/
	return 0;
}

/*
 * Called when some kind of fatal error occurs
 *
 */
static void
mr_giveup(char *fmt, ...)
{
	static int buttons[] = {DIALOGCONTINUE,DIALOGPREVDEF,-1};
	va_list ap;
	char buff[LINESIZE];

	cleanGfxGui();
	setGuiMode(0,GUI_NOLOGO);

	/*CONSTCOND*/
	va_start(ap,fmt);
	vsprintf(buff, fmt, ap);
	va_end(ap);

	if (isGuiMode()) {
		setGuiMode(1,GUI_NOLOGO);		/* clear screen */
		popupDialog(buff,buttons,DIALOGWARNING,DIALOGCENTER);
	}
	else {
		p_curson();
		p_printf("%s\nPress <Enter> to return to the menu: ", buff);
		gets(buff);
	}
	longjmp(jb,1);
}

static int
cleanup(char* buf, ULONG fd, int retval)
{
    if (fd != (ULONG)-1)
	Close(fd);
    if (buf)
	dmabuf_free(buf);
    return retval;
}

static int
auto_mr_63_format(const char* vh_buf, struct string_list* newargv)
{
    const char* SA       = "\nSA=";
    const char* SASH     = "\nSASH=";

    char bootfile[256];
    const char *ptr, *tptr;
    int  len1,len2;

    /* this garbage below is for compatibility with 6.3/6.4 sash 
       so autominiroot on a 6.3/6.4 system works with a sash >= 6.5 */
    if ((ptr=strstr(vh_buf,SA)) != NULL &&
	(tptr=strstr(ptr+1,"\n")) != NULL &&
	(len1=tptr-ptr-strlen(SA)) < sizeof(bootfile)) {
	strncpy(bootfile,ptr+strlen(SA),len1); 
	*(bootfile+len1) = '\0';
	setenv("tapedevice",bootfile);
	if ((ptr=strstr(vh_buf,SASH)) != NULL &&
	     (tptr=strstr(ptr+1,"\n")) != NULL &&
	    len1+(len2=tptr-ptr-strlen(SASH)) < sizeof(bootfile)) {
	    strncpy(bootfile+len1,ptr+strlen(SASH),len2);
	    *(bootfile+len1+len2) = '\0';
	    new_str1(bootfile,newargv);
	    new_str1("--m",newargv);

	    return 1;
	}
    }
    return 0;
}


/*  Determine if we should do a custom boot.  Currently,
    we do a custom boot if the file "auto_mr" is present in the
    volume header *and* the first byte is '0'.  After first encountering
    this file, the first byte of auto_mr is changed to '1' and the file 
    is written out again to prevent the sash from continually trying to 
    do a custom boot when starting up the system.  Inside the 
    auto_mr file, we store the bootfile and the boot arguments.
    We also store any environment variable settings such as tapedevice
    for a miniroot boot.                                                    */

#define VH_READ_BUFSIZE  (8 * BLKSIZE)

int
do_custom_boot(int oldargc, char** oldargv)
{
    LARGE off;
    LONG rval;
    ULONG fd, byte_count, bytes_total, bytes;
    struct string_list newargv;
    char  vh_name[256], *argv[30];
    char *vh_buf, *vh_cpy, *buf, *cpy, *ptr, *tptr, *envptr;
    char* vh_path = getenv("SystemPartition");
    int argc = 0, i, retry_count = 0;
    const char* BOOTFILE = "\nBOOTFILE=";
    const char* BOOTARGS = "\nBOOTARGS=";
    const char* ENV      = "\nENV:";

    /* nothing we can do */
    if (vh_path == 0)
	return 0;

    strcpy(vh_name,vh_path); strcat(vh_name,"auto_mr");
tryopen:
    if ((rval = Open((CHAR *)vh_name, OpenReadWrite, &fd)) != ESUCCESS) {
	/* this is for roboinst and CD-ROM... some CD-ROM drives are */
	/* taking a while to become ready after a scsi bus reset and */
	/* consequently boots are failing.  delay and retry. (547463) */
    	if (rval == ENODEV && retry_count++ < 3) {
		pause(5, "", "\033");
		goto tryopen; 
	}
	return 0;
    }

    if ((buf=dmabuf_malloc(VH_READ_BUFSIZE + BLKSIZE)) == 0) 
	return cleanup(0,fd,0);

    vh_buf = (char *)(((__psunsigned_t)buf+BLKSIZE-1)&~(BLKSIZE-1));

    /* read at most VH_READ_BUFSIZE bytes  */

    bytes = VH_READ_BUFSIZE; bytes_total = 0;
    while (bytes > 0 && Read(fd, (void *)(vh_buf+bytes_total), 
			     bytes, &byte_count) == ESUCCESS && byte_count) {
	   
	bytes       -= byte_count;
	bytes_total += byte_count;
    }

    /* if we read full VH_READ_BUFSIZE bytes, we need to truncate a byte */

    vh_buf[bytes > 0 ? bytes_total : bytes_total-1] = '\0';

    if (bytes_total == 0 || *vh_buf != '0') 
	return cleanup(buf,fd,0);

    /* seek back to start of the auto_mr file and reset magic first byte  */

    off.lo = 0;  off.hi = 0;
    if (Seek(fd, &off, SeekAbsolute) != ESUCCESS) 
	return cleanup(buf,fd,0);

    /* write out just first block, probably entire buffer */
    /* use a copy since buffer might get byteswapped on some platforms */
    *vh_buf = '1';
    if ((cpy=dmabuf_malloc(VH_READ_BUFSIZE + BLKSIZE)) == 0)
        return cleanup(buf,fd,0);
    vh_cpy = (char *)(((__psunsigned_t)cpy+BLKSIZE-1)&~(BLKSIZE-1));
    memcpy(vh_cpy, vh_buf, BLKSIZE);
    if (Write(fd, vh_cpy, BLKSIZE, &byte_count) != ESUCCESS)
	return dmabuf_free(cpy), cleanup(buf,fd,0);
    dmabuf_free(cpy);
    Close(fd);

    /* look for the boot file;  if not found, just abort */
    init_str(&newargv);
    if ((ptr=strstr(vh_buf,BOOTFILE)) != NULL &&
	(tptr=strstr(ptr+1,"\n")) != NULL) {
	*tptr = '\0';
	new_str1(ptr+strlen(BOOTFILE),&newargv);
	*tptr = '\n';
    }
    else if (!auto_mr_63_format(vh_buf, &newargv))
	return cleanup(buf,-1,-1);

    /* parse the boot arguments into the newargv array */
    if ((ptr=strstr(vh_buf,BOOTARGS)) != NULL &&
	(tptr=strstr(ptr+1,"\n")) != NULL) {
	char* bootargs = ptr + strlen(BOOTARGS);
	*tptr = '\0';
	while (1) {
	    if ((ptr = index(bootargs,' ')) != NULL)
		*ptr = '\0';

	    if (*bootargs != '\0')
		new_str1(bootargs,&newargv);

	    if (ptr) {
		*ptr = ' ';
		bootargs = ptr+1;
	    }
	    else
		break;
	}
	*tptr = '\n';
    }

    /* set any desired environment variables */
    envptr = vh_buf;
    while ((ptr=strstr(envptr,ENV)) != NULL &&
	   (tptr=strstr(ptr+1,"\n")) != NULL) {
	*tptr = '\0';
	if ((envptr = index(ptr,'=')) != NULL) {
	    *envptr = '\0';
	    setenv(ptr+strlen(ENV),envptr+1);
	}
	*tptr='\n';
	envptr = tptr;
    }

    /* free the malloc'd memory */
    (void)cleanup(buf,-1,0);

    /* create the new argv array from the provided arguments */
    argv[argc++] = "boot";
    argv[argc++] = "-f";
    for (i=0; i<newargv.strcnt && argc<sizeof(argv)/sizeof(char*); i++)
	argv[argc++] = newargv.strptrs[i];

    /* append ARCS compliant arguments */
    for (i=0; i<oldargc && argc<sizeof(argv)/sizeof(char*); i++)
	argv[argc++] = oldargv[i];


    /* Some CDROM devices do not become ready fast enough, causing 
       the sash boot to fail.  So, keep trying to open boot file, 
       pausing progressively longer after each iteration. (589308) */

    if (argc > 2 && !strstr(argv[2],"bootp")) {
	retry_count = 0;
	while (((rval=Open((CHAR *)argv[2],OpenReadOnly,&fd)) != ESUCCESS) && 
	       (++retry_count <= 4))
	    pause(retry_count*2, "", "\033"); /* pause progressively longer */

	if (rval == ESUCCESS)
	    Close(fd);
    }

    boot(argc,argv,argv,0);

    return -1;
}


/*
 * User interface routines
 */

static long discnt;
static struct ProgressBox *prog;

/*ARGSUSED*/
static void
cbhandler(struct Button *b, __scunsigned_t value)
{

	setGuiMode(0,0);
	longjmp(cancelljb,1);
}

static void
mr_msg_start(void)
{
	static char *title = "Copying installation tools to disk";
	struct Button *cancel;
	struct Dialog *d;
#define PGHEIGHT	20

	if ( !doGui() ) {
		p_printf("Copying installation program to disk.\n");
		discnt = 0;
		return;
	}

	/*
	 * Hack for testing
	 */
	SetEnvironmentVariable("VERBOSE", "0");

	setGuiMode(1,GUI_NOLOGO);

	d = createDialog(title,DIALOGPROGRESS,DIALOGBIGFONT);
	cancel = addDialogButton(d,DIALOGCANCEL);
	addButtonCallBack(cancel,cbhandler,1);
	resizeDialog(d,0,DIALOGBUTMARGIN+PGHEIGHT);

	prog = createProgressBox(d->gui.x1+DIALOGBDW+DIALOGMARGIN,
			 cancel->gui.y2+DIALOGBUTMARGIN,
			 d->gui.x2-d->gui.x1-2*(DIALOGBDW+DIALOGMARGIN));

	guiRefresh();
	
}

static void
mr_msg_progress(int percent, int tenth, int oldpercent)
{
	if (isGuiMode()) {
		changeProgressBox(prog,percent,tenth);
		return;
	}

	if (percent == oldpercent)
		return;

	if ( percent % 10 == 0 ) {
		printf(" %d%% ", percent );
		discnt += 4;
	} else
		printf (".");

	if (++discnt >= 70) {
		printf("\n");
		discnt = 0;
	}
}

static void
mr_msg_done(void)
{
	if (doGui())
		setGuiMode(0,GUI_NOLOGO);
	else
		printf("\nCopy complete\n");
}

/* converts arcs name into dksXdXsX suitable for setting root/swap.
 */
static int
arcs_to_dsk(char *arcs, char *dsk)
{
	char *tmp, *p = arcs;
	int scsi, disk, part;
	__psunsigned_t len;

	scsi = part = 0;
	disk = -1;

	while (*p) {
		if (!(tmp=index(p,'(')) || !*(++tmp))
			return(0);

		len = (__psunsigned_t)tmp - (__psunsigned_t)p;

		if (len < 4)
			/*skip*/;
		else if (!strncmp("scsi",p,4))
			scsi = atoi(tmp);
		else if (!strncmp("disk",p,4))
			disk = atoi(tmp);
		else if (!strncmp("part",p,4))
			part = atoi(tmp);

		if (!(p=index(tmp,')')))
			return(0);

		p++;		/* skip ')' */
	}

	/* disk(X) must be specified */
	if (disk == -1)
		return(0);

	sprintf(dsk,"dks%dd%ds%d",scsi,disk,part);
	return(1);
}



/*
 * If tapedevice specifies CD-ROM and it contains a bootable disk
 * or if tapedevice is a bootable tape image over the net then fill in
 * source and dest (both are the same for a bootable image).
 * as of 6.2 for all but IP17 (and 6.3 for all), kernels are no longer in
 * the miniroot images, so tape boots are out; the kernel is found at
 * the same place as "sa", but in the subdir miniroot/unix.IPxx
 * Returns BOOTABLE_NOT, or BOOTABLE_KERN.
 */

static int
mr_bootable(char *source)
{
	char *td, bootable[MAXFNLEN];
	char *cp;
	ULONG fd;

	td = getenv("tapedevice");

	if (Debug) {
		printf("tapedevice=%s\n", td);
		printf("source=%s\n", source);
	}

	/*
	 * if no tapedevice it's kinda hard to check what's bootable...
	 */
	if (!td)
		return BOOTABLE_NOT;

	/* Is tapedevice a bootable remote directory? 
	 * bootable if bootp()$host:$path/miniroot/unix.IPXX exists 
	 * "tapedevice" used in the non-bootable case: don't trash it
	 */
	strcpy(bootable, td);

	/* chop off the "...sa" if it exists */
	cp = bootable + strlen(bootable) - 2;
	if (*cp == 's' && *(cp + 1) == 'a')
		*cp = '\0';

	/* if a cdrom ... chop off the partition number */
	if (strstr(bootable, "cdrom") != NULL) {
		cp = bootable + strlen(bootable) - 3;

		/* Replace partition value ... should be 7 
		 * and is a single digit number.
		 */
		if (*cp == '(' && *(cp + 2) == ')')
			*(cp + 1) = '7';
		else 
			return BOOTABLE_NOT;

		/* need "dist/" since it is from CDROM */
		strcat(bootable, CDROM_LOCATION);
	}

	/* Check the directory for minroot unix's */
	strcat(bootable, UNIX_DIR_LOCATION);
	strcat(bootable, inv_findcpu());

	if (Debug) printf("bootable=%s\n", bootable);

	if (Open(bootable, OpenReadOnly, &fd) == ESUCCESS) {
		Close(fd);
		strcpy(source, bootable);
		if (Debug) 
			printf("Booting installation tools from: %s\n", bootable);
		return BOOTABLE_KERN;
	}

	if (Debug)
		printf("%s: not found (tapedevice=%s), not bootable\n",
				bootable, td);

	/* tapedevice is a tape perhaps? */
	return BOOTABLE_NOT;
}

/* set (or reset) miniroot install state.  Used to check if there was
 * crash/power failure/bad error while we were in the miniroot, so we
 * can tell the user we seem to have had an interrupted install, and
 * and see if the want to retry it, reload, or just cleanup.
 * Up through 6.2, this was done in miniroot bootup scripts, and involved
 * also changing the root part, but -b option to dvhtool was removed as part of
 * cleanup, and it's cleaner to not much with the volhdr, anyway.
 * Called from within this file, and also from main() in sash.
*/
void
mr_setinst_state(int which)
{
	char *action = which ? "restore" : "set";
	char *osl, nosl[13];

	osl = getenv("OSLoadOptions");
	/* do both, setenv and SetEnv so it's correct for subsequent getenv
	 * and loaded programs also */
	if(which) { /* restore */
		if(osl && (!strncmp(osl, "inst", 4) || !strncmp(osl, "INST", 4))) {
			/* set to empty, since a value of auto in the nvram
			   will prevent sash from autobooting */
			SetEnvironmentVariable("OSLoadOptions", "");
			setenv("OSLoadOptions", "");
		}
	}
	else { /* save old, and set root same as swap */
		if(osl) {
			if(!strncmp(osl, "inst", 4))
				osl += 4;	/* in case we didn't reset already */
			if(strlen(osl) <= 8 && strncmp(osl, "INST", 4))
			        sprintf(nosl, "INST%.8s", osl);
			else
				sprintf(nosl, osl);
		}
		else
			sprintf(nosl, "INST");

		SetEnvironmentVariable("OSLoadOptions", nosl);
		setenv("OSLoadOptions", nosl);
	}
}


mr_interrupted_inst(int ismrb, int iscustom)
{
	char ansbuf[LINESIZE];

	p_printf("It appears that a miniroot install failed.  Either the system is\n");
	p_printf("misconfigured or a previous installation failed.\n");
	if (iscustom) {
		mr_setinst_state(1);
		p_printf("miniroot install state automatically reset to normal\n");
		return -1;	/* continue loading miniroot, or whatever */
	}
	if(ismrb) {
		p_printf("If you think the miniroot is still valid, you may continue booting\n");
		p_printf("using the current miniroot image.  If you are unsure about the\n");
		p_printf("current state of the miniroot, you can reload a new miniroot image.\n");
	}
	else
		p_printf("You may continue booting without fixing the state\n");
	
	p_printf("You may abort the installation and return to the menu, or you can\n");
	p_printf("fix (reset to normal) the miniroot install state\n");
	p_printf("See the 'Software Installation Guide' chapter on Troubleshooting for\n");
	p_printf("more information\n\n");
	p_printf("Enter 'c' to continue%s with no state fixup.\n",
		ismrb ? " booting the old miniroot" : "");
	p_printf("Enter 'f' to fix miniroot install state, and try again\n");
	if(ismrb) {
		p_printf("Enter 'r' to reload the miniroot.\n");
		p_printf("Enter 'a' to abort (cancel) the installation.\n");
	}
	else
		p_printf("Enter 'a' to abort and return to menu.\n");
	p_printf("Enter your selection and press ENTER (c, f,%s or a) ",
		ismrb?" r,":"");

	*ansbuf = '\0';
	gets(ansbuf);
	if(*ansbuf == 'c' || *ansbuf == 'C')
		return 1;
	if(*ansbuf == 'f' || *ansbuf == 'F') {
		mr_setinst_state(1);
		p_printf("miniroot install state reset to normal\n");
		return -1;	/* continue loading miniroot, or whatever */
	}
	if(ismrb) {
		if(*ansbuf == 'r' || *ansbuf == 'R')
			return 0;
		mr_giveup("Procedure canceled\n");
	}
	return 0;
}

/*
 * Derive the flag that determines a custom miniroot boot
 */
int mr_iscustom(int ac, char **av)
{
	int i;

	for (i=0; i<ac ; ++i)
	    if (av[i] && (!strcmp(av[i], "mrmode=custom") ||
	            !strcmp(av[i], "mrmode=customdebug")))
		return 1;
        return 0;
}
