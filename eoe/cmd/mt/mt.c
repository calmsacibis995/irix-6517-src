/*
** 	mt.c	- Copyright (C) Silicon Graphics - Mountain View, CA 94043
**		- Author: chase bailey
**		- Date: April 1984
**		- Any use, copy or alteration is strictly prohibited
**		- and gosh darn awfully -- morally inexcusable
**		- unless authorized by SGI.
**
*/

#ident "mt/mt.c: $Revision: 1.59 $"

#include <sys/param.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <sys/invent.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "rmt.h"

#ifndef btod
# define	btod(BB)	(((BB) + NBPSCTR - 1) >> SCTRSHFT)
#endif

#define	DEFTAPE	"/dev/nrtape"

#define	equal(s1,s2)	(strcmp(s1, s2) == 0)

struct commands {
	char *c_name;
	char *c_help;
	int c_ioctl;
	int c_code;
	int c_openopt;
	int c_argsreq;
} com[] = {
 { "weof", "Write [count] end-of-file marks", MTIOCTOP, MTWEOF, O_WRONLY, 0 },
 { "wsetmk", "Write [count] set marks", MTIOCTOP, MTWSM, O_WRONLY, 0 },
 { "fsf", "Space forward [count] file marks", MTIOCTOP, MTFSF, O_RDONLY, 0 },
 { "fsr", "Space forward [count] records", MTIOCTOP, MTFSR, O_RDONLY, 0 },
 { "bsf", "Space backward [count] file marks", MTIOCTOP, MTBSF, O_RDONLY, 0 },
 { "bsr", "Space backward [count] records", MTIOCTOP, MTBSR, O_RDONLY, 0 },
 { "spsetmk", "Space [count] set marks", MTIOCTOP, MTSKSM, O_RDONLY, 0 },
 { "rewind", "Rewind tape device", MTIOCTOP, MTREW, O_RDONLY, 0 },
 { "sppart", "Space to partition 'count'", MTIOCTOP, MTSETPART, O_RDONLY, 3 },
 { "mkpart", "Create two partition tape; first is 'count' Mbytes long", MTIOCTOP,
	MTMKPART, O_WRONLY, 3 },
 { "retension", "Retension tape", MTIOCTOP, MTRET, O_RDONLY, 0 },
 { "feom", "Space to end of data", MTIOCTOP, MTEOM, O_RDONLY, 0 },
 { "offline", "Take tape offline", MTIOCTOP, MTOFFL, O_RDONLY, 0 },
 { "unload", "Unload tape from drive", MTIOCTOP, MTUNLOAD, O_RDONLY, 0 },
 { "erase", "Erase from current position to EOT", MTIOCTOP, MTERASE,
	O_RDONLY, 0 },
 { "exist", "Exit status 0 if tape drive exists", MTIOCTOP, MTNOP, O_RDONLY, 0 },
 { "status", "Read tape status", MTIOCGET, MTNOP, O_RDONLY, 0 },
 { "blksize", "Return default tape block size", MTIOCTOP, MTNOP, O_RDONLY, 0 },
 { "setblksz", "Set block size for some scsi tape drives", MTSPECOP,
 	MTSCSI_SETFIXED, O_RDONLY, 3 },
 { "recerron", "Enable recoverable error reporting",
 	MTSPECOP, MTSCSI_CIPHER_SEC_ON, O_RDONLY, 0 },
 { "recerroff", "Disable recoverable error reporting",
 	MTSPECOP, MTSCSI_CIPHER_SEC_OFF, O_RDONLY, 0 },
 { "sili", "Suppress illegal length indicator", MTSPECOP, MTSCSI_SILI,
	O_RDONLY, 0 },
 { "eili", "Enable illegal length indicator", MTSPECOP, MTSCSI_EILI,
	O_RDONLY, 0 },
 { "logdisable", "Disable sense data logging", MTSPECOP, MTSCSI_LOG_OFF,
	O_RDONLY, 0 },
 { "logenable", "Enable sense data logging", MTSPECOP, MTSCSI_LOG_ON,
	O_RDONLY, 0 },
 { "reset", "Obsolete -- use 'scsiha -r' to reset SCSI bus now",
 	MTIOCTOP, MTNOP, O_RDONLY, 0 },
 { "audio", "Set audio (1) or data (0) mode for writing", MTIOCTOP, MTAUD,
	O_RDONLY, 3 },
 { "seek", "Seek to given block (program # in audio mode)", MTIOCTOP,
	MTSEEK, O_RDONLY, 3 },
 { "help", "Help printout of this message", MTIOCTOP, MTNOP, O_RDONLY },
 { 0 }
};


int mtfd;
struct mtop mt_com;
struct mtget mt_status;
char *tape;
void status(struct mtget *bp);
void usage(void);

extern int errno;

/* seperate routine because with buffering turned on for stderr, we
 * want to make sure everything flushed before doing the perror.
 */
void
error(const char * msg, int xitval)
{
	fflush(stdout);	/* just in case */
	fflush(stderr);
	perror(msg);
	exit(xitval);
}

void
doit(register struct commands *comp)
{
	mt_com.mt_op = comp->c_code;
	if (ioctl(mtfd, comp->c_ioctl, &mt_com) < 0) {
		fprintf(stderr, "%s %s %d ", tape, comp->c_name,
			mt_com.mt_count);
		error("failed", 2);
	}
}

int
main( int argc, char **argv)
{
	register char *cp;
	register struct commands *comp = com;
	char *getenv();

	setvbuf(stderr, NULL, _IOLBF, BUFSIZ);	/* mainly for usage message */
	if (argc < 2) {
		usage();
		exit(2);
	}
	if (argc > 2 && (equal(argv[1], "-t") || equal(argv[1], "-f"))) {
		if (argc < 4) {
			usage();
			exit(2);
		}
		argc -= 2;
		tape = argv[2];
		argv += 2;
	} else
		if ((tape = getenv("TAPE")) == NULL) {
			tape = DEFTAPE;
		}
	cp = argv[1];
	for (comp = com; comp->c_name != NULL; comp++)
		if (strncmp(cp, comp->c_name, strlen(cp)) == 0)
			break;
	if (comp->c_name == NULL) {
		usage();
		exit(2);
	}
	if (strcmp(comp->c_name, "help") == 0) {
		usage();
		exit(0);
	}
	/*
	 * This command simply returns an error code if the
	 * tape device exists. If the tape can be opened and
	 * accepts the NOP ioctl, or * is busy, return true.
	 * Otherwise, return false. NOP ioctl is so we don't return
	 * successful on regular files or non-tape devices.
	 */
	if (strcmp(comp->c_name, "exist") == 0) {
		if((mtfd=open(tape, comp->c_openopt)) < 0) {
			if (errno == EBUSY)
				exit(0);
			else
				exit(1);
		}
		else {
			mt_com.mt_op = comp->c_code;
			exit(ioctl(mtfd, comp->c_ioctl, &mt_com) == 0 ? 0 : 1);
		}
	}
	if ((mtfd = open(tape, comp->c_openopt)) < 0) {
		switch(errno) {
		case ENXIO:
			fprintf(stderr, "%s: %s (tape driver not loaded?)\n",
				tape, strerror(errno));
			break;
		case ENOEXEC:
			fprintf(stderr, "%s: %s (tape driver no longer loadable?)\n",
				tape, strerror(errno));
			break;
		case ENODEV:
			fprintf(stderr, "%s: %s (device file present, but no tape drive)\n",
				tape, strerror(errno));
			break;
		case ENOENT:
			fprintf(stderr, "%s: %s in filesystem\n",
				tape, strerror(errno));
			break;
		default:
			error(tape, 1);
		}
		return 1;
	}
	if (strcmp(comp->c_name, "blksize") == 0) {
		int blocksize;
		struct mtblkinfo blkinfo;

		if(ioctl( mtfd, MTIOCGETBLKINFO, &blkinfo) == 0) {
			/* only on SCSI tape */
			fprintf(stderr, "\n Recommended tape I/O size: %d bytes",
				blkinfo.recblksz);
			if((btod(blkinfo.recblksz) << SCTRSHFT) == blkinfo.recblksz) 
				fprintf(stderr, " (%d 512-byte blocks)",
					blkinfo.recblksz >> SCTRSHFT);
			fprintf(stderr,
				"\n Minimum block size: %d byte(s)\n Maximum block size: %d bytes",
				blkinfo.minblksz, blkinfo.maxblksz);
			if (blkinfo.curblksz > 0)
				fprintf(stderr,
					"\n Current block size: %d byte(s)\n",
					blkinfo.curblksz);
			else
				fprintf(stderr,
					"\n Current block size: Variable\n");
		}
		else if (ioctl(mtfd, MTIOCGETBLKSIZE, &blocksize) < 0)
			error("mt: can't get blksize", 2);
		else
			fprintf(stderr, "\n Default Tape Block Size %d\n", blocksize);
		/* else don't complain; only supported on SCSI drives */
		exit(0);
	} else if (strcmp(comp->c_name, "status") == 0) {
	 	mt_status.mt_type = comp->c_code;
		if (ioctl(mtfd, comp->c_ioctl, (char *)&mt_status) < 0)
			error("mt: can't get status", 2);
		status(&mt_status);
	}
	else if (strcmp(comp->c_name, "reset") == 0) {
		fprintf(stderr, "The SCSI bus reset operation is no longer "
			"supported by mt.  Please use 'scsiha'.\n");
	}
	else  {
		if(comp->c_argsreq > argc) {
			fprintf(stderr, "You must give an argument with %s\n", comp->c_name);
			exit(1);
		}
		if(argc == 2)
			mt_com.mt_count = 1;
		else {
			/* use strtol primarily so we can seek to 0xbbb and 0xeee
			 * for audio mode tapes; also allows better checking for
			 * to see if they actually gave us a number, rather than
			 * using 0, as with atoi() */
			char *s;
			mt_com.mt_count = strtol(argv[2], &s, 0);
			if(s == argv[2]) {
				fprintf(stderr, "mt: %s is not a valid number\n", s);
				exit(1);
			}
		}
		doit(comp);
	}
	return 0;
}

void
usage(void)
{
	struct commands *comp = com;

	fprintf(stderr, "\n mt [-f /dev/tapename] command [count]\n");
	fprintf(stderr, "\t\t\t*** commands ***\n");
	for (; comp->c_name != NULL; comp++)
		fprintf(stderr, "\t%-11s - %s\n", comp->c_name, comp->c_help);
}

/*
 * As of 3/96, only SCSI drives through tpsc are supported.  Previously,
 * we distinguished jagtape, VME-QIC, and VME-Pertec.
 */
struct tape_desc {
	short	t_type;		/* type of magtape device */
	char	*c_name;	/* controller name */
} tapes[] = {
	{ MT_ISSCSI,	"Western Digital SCSI Controller"},
	{ 0 }
};

/*
** Interpret the status buffer returned
*/
void
status(struct mtget *bp)
{
	struct tape_desc *mt;
	unsigned long status = 0;	/* scsi uses 32 bits */
	struct mt_capablity mtcap;
	char *msg;

	for (mt = tapes; mt->t_type; mt++)
		if (mt->t_type == bp->mt_type)
			break;
	if (mt->t_type == 0) {
		fprintf(stderr, "unknown driver type (%d)\n", bp->mt_type);
		return;
	}

	if (mt->t_type == MT_ISSCSI) {
		ct_g0inq_data_t	info;
		char	vidbuf[ MAX_INQ_VID + 1 ];
		char	pidbuf[ MAX_INQ_PID + 1 ];
		char	revbuf[ MAX_INQ_PRL + 1 ];

		status = bp->mt_dsreg | ((ulong)bp->mt_erreg<<16);

		fprintf(stderr, "\tController: SCSI\n");
		/* if can't execute this ioctl then won't have
		** tape drive name
		*/
		if ( ioctl( mtfd, MTSCSIINQ, &info ) >= 0 ) {
			if ( info.id_ailen == 0 )
			{
				strncpy( vidbuf, "CIPHER ", MAX_INQ_VID );
				strncpy( pidbuf, "CIPHER 540S", MAX_INQ_PID );
				*revbuf = '\0';
			}
			else
			{
				int i;
				strncpy( vidbuf, info.id_vid, MAX_INQ_VID );
				strncpy( pidbuf, info.id_pid, MAX_INQ_PID );
				/* show prom rev also, since we have it, and it
				 * can make a difference to some problem
				 * reports */
				strncpy( revbuf, info.id_prl, MAX_INQ_PRL );
				/* remove trailing spaces from vendor-id, so the
				we don't get ' : ' for short names */
				for(i=MAX_INQ_VID-1; i>=0; i--)
					if(vidbuf[i] != ' ')
						break;	/* stop at first non-blank */
				/* ensure null termination */
				vidbuf[i+1] = 0;
				pidbuf[ sizeof(pidbuf)-1 ] = 0;
				revbuf[ sizeof(revbuf)-1 ] = 0;
			}
			/* no space before prl; drives like archive then
			 * show  ARCHIVE: VIPER 150  21247-603 */
			fprintf(stderr, "\tDevice: %s: %s%s\n", vidbuf, pidbuf,
				revbuf);
		}
		fprintf(stderr, "\tStatus: 0x%x\n", status);
	}

	if(ioctl(mtfd, MTCAPABILITY, &mtcap) == 0) {
		/* not all support at SGI yet, no non-SGI, so don't complain
		 * on failures.  This should match hinv. */
		char *dtype = NULL;
		char newtype[40];
		switch(mtcap.mtsubtype) {
		case TPQIC24:
			dtype = "QIC 24";
			break;
		case TPQIC150:
			dtype = "QIC 150";
			break;
		case TPQIC1000:
			dtype = "QIC 1000";
			break;
		case TPQIC1350:
			dtype = "QIC 1350";
			break;
		case TPDAT:
			dtype = "DAT";
			break;
		case TP9TRACK:
			dtype = "9 track";
			break;
		case TP8MM_8200:
			dtype = "8mm(8200) cartridge";
			break;
		case TP8MM_8500:
			dtype = "8mm(8500) cartridge";
			break;
		case TP3480:
			dtype = "3480 cartridge";
			break;
		case TPDLT:
		case TPDLTSTACKER:
			dtype = "DLT";
			break;
		case TPD2:
			dtype = "D2 cartridge";
			break;
		case TPNTP:
		case TPNTPSTACKER:
			dtype = "IBM Magstar 3590";
			break;
		case TPSTK9490:
			dtype = "STK 9490 (Timberline)";
			break;
		case TPSTKSD3:
			dtype = "STK SD3 (Redwood)";
			break;
		case TPSTK9840:
			dtype = "STK 9840";
			break;
		case TPUNKNOWN:
			dtype = "unknown";
			break;
		case TPGY10:
			dtype = "SONY GY-10";
			break;
		case TPGY2120:
			dtype = "SONY GY-2120";
			break;
		case TP8MM_8900:
			dtype = "8mm(8900) cartridge";
			break;
		case TP8MM_AIT:
			dtype = "8mm (AIT) cartridge";
			break;
               	case TPMGSTRMP:
                case TPMGSTRMPSTCKR:
                        dtype = "IBM Magstar MP 3570";
                        break;
		case TPSTK4791:
			dtype = "STK 4791";
			break;
		case TPSTK4781:
			dtype = "STK 4781";
			break;
                case TPFUJDIANA1:
                        dtype = "FUJITSU  M1016/M1017 3480";
                        break;
                case TPFUJDIANA2:
                        dtype = "FUJITSU  M2483 3480/3490";
                        break;
                case TPFUJDIANA3:
                        dtype = "FUJITSU  M2488 3480/3490";
                        break;
                case TPTD3600:
                        dtype = "Philips TD3600";
                        break;
                case TPNCTP:
                        dtype = "Philips NCTP";
                        break;
		case TPOVL490E:
			dtype = "Overland Data L490E";
			break;
		default:
			dtype = newtype;
			sprintf(dtype, "unknown type %d", mtcap.mtsubtype);
		}
		fprintf(stderr, "\tDrive type: %s\n", dtype);
	}

	if (mt->t_type == MT_ISSCSI) {
		if(bp->mt_dposn & MT_ONL) {
			fprintf(stderr, "\tMedia : READY");
			fprintf(stderr, ", %s, ",
				bp->mt_dposn & MT_WPROT ? "write protected"
				: "writable");
			if (bp->mt_dposn & MT_BOT)
				msg = "at BOT";
			else if (bp->mt_dposn & MT_EOT)
				msg = "at EOT";
			else if (bp->mt_dposn & MT_EOD)
				msg = "at EOD";
			else if (!(bp->mt_dposn & MT_FMK))
				/* don't print not at BOT if at FM */
				msg = "not at BOT";
			else
				msg = NULL;
			if(msg)
			    fprintf(stderr, "%s",msg);
			/* seperate from above, because we want to print at EOD, at FMK,
			 * if that is the case */
			if (bp->mt_dposn & MT_FMK)
				fprintf(stderr, "%sat FMK",msg?", ":"");
			if(mt->t_type == MT_ISSCSI) {
				if(status & CT_SMK)
					fprintf(stderr, ", setmark");
				if(status & CT_MULTPART)
					fprintf(stderr, ", partition %d", bp->mt_resid);
				if(status & CT_AUD_MED) {
					fprintf(stderr, ", audio media");
					if(mtcap.mtsubtype == TPDAT && (bp->mt_dposn & MT_BOT))
						fprintf(stderr, " or blank tape");
				}
				if(status & CT_AUDIO)
					fprintf(stderr, ", audio mode");
				/* don't print anything for 'old' tape and
				 * no density bit set, or QIC150 drive and
				 * 'standard' QIC150 cartridge.
				 * Unfortunately, the density code (and hence
				 * the bit in dsreg) isn't set until after a
				 * successful read or space cmd.  Still
				 * better than nothing...  The density
				 * code is the FORMAT the tape was last
				 * written in, not the cartrige type.  */
				if(bp->mt_dsreg & (MT_QIC24|MT_QIC120))
					fprintf(stderr, ", QIC%d format",
						(bp->mt_dsreg & MT_QIC120)
						? 120 : 24);
				/* this will rarely if ever be true, since the open
				 * will normally block until an immediate mode
				 * seek or rewind completes, but this may
				 * change in the future. */
				if(status & CT_SEEKING)
					fprintf(stderr, ", seeking");
			}
			if(bp->mt_blkno) {	/* many drives/drivers don't support
				this, so the value will be zero */
				if(status & (CT_AUD_MED|CT_AUDIO)) {
					switch(bp->mt_blkno&0xfff) {
					/* for these 3, the driver makes the top nibble
					 * the same as the bottom 2, so they don't match
					 * any valid program number */
					case 0xAAA:	/* program # not valid */
						break;
					case 0xBBB:
						fprintf(stderr, ", at leadin");
						break;
					case 0xEEE:
						fprintf(stderr, ", at leadout");
						break;
					default:
						fprintf(stderr, ", program %d", bp->mt_blkno);
						break;
					}
				}
				else {
					/*
					 * Upper 10 bits of STK is for
					 * internal use; only the
					 * lower 22 bits reflect the
					 * "real" block number
					 */
					if (mtcap.mtsubtype == TPSTK9490 || 
					    mtcap.mtsubtype == TPSTKSD3  ||
					    mtcap.mtsubtype == TPTD3600)
						bp->mt_blkno &= 0x003FFFFFL;
					fprintf(stderr, ", block %d", bp->mt_blkno);
				}
			}
			if(bp->mt_fileno)	/* some drivers don't support
				this, so the value will be zero */
				fprintf(stderr, ", file %d", bp->mt_fileno);
		}
		else {
			fprintf(stderr, "\tMedia : Not READY");
			if(status & CT_AUDIO)
				fprintf(stderr, ", in audio mode ");
		}
		fprintf(stderr,"\n");
	}
}
