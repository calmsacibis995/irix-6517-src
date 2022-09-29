#ifndef GETOPT_H
#define GETOPT_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/dump/RCS/getopt.h,v 1.12 1998/05/13 20:48:44 kcm Exp $"

/* getopt.h	common getopt  command string
 *
 * several modules parse the command line looking for arguments specific to
 * that module. Unfortunately, each of the getopt(3) calls needs the
 * complete command string, to know how to parse. This file's purpose is
 * to contain that command string. It also abstracts the option letters,
 * facilitating easy changes.
 */

#define GETOPT_CMDSTRING	"ab:c:f:hl:mop:s:v:AB:CEFG:H:I:JL:M:NO:PRSTUVWY:Z"

#define GETOPT_DUMPASOFFLINE	'a'	/* dump DMF dualstate files as offline */
#define	GETOPT_BLOCKSIZE	'b'	/* blocksize for rmt */
#define	GETOPT_ALERTPROG	'c'	/* Media Change Alert prog(content.c) */
#define	GETOPT_DUMPDEST		'f'	/* dump dest. file (drive.c) */
#define	GETOPT_HELP		'h'	/* display version and usage */
#define	GETOPT_LEVEL		'l'	/* dump level (content_inode.c) */
#define GETOPT_MINRMT		'm'	/* use minimal rmt protocol */
#define GETOPT_OVERWRITE	'o'	/* overwrite data on tape */
#define GETOPT_PROGRESS		'p'	/* interval between progress reports */
#define	GETOPT_SUBTREE		's'	/* subtree dump (content_inode.c) */
#define	GETOPT_VERBOSITY	'v'	/* verbosity level (0 to 4 ) */
#define	GETOPT_NOEXTATTR	'A'	/* do not dump ext. file attributes */
#define	GETOPT_BASED		'B'	/* specify session to base increment */
#define GETOPT_RECCHKSUM	'C'	/* use record checksums */
#define	GETOPT_ERASE		'E'	/* pre-erase media */
#define GETOPT_FORCE		'F'	/* don't prompt (getopt.c) */
#define GETOPT_MINSTACKSZ	'G'	/* minimum stack size (bytes) */
#define GETOPT_MAXSTACKSZ	'H'	/* maximum stack size (bytes) */
#define GETOPT_INVPRINT         'I'     /* just display the inventory */
#define	GETOPT_NOINVUPDATE	'J'	/* do not update the dump inventory */
#define	GETOPT_DUMPLABEL	'L'	/* dump session label (global.c) */
#define	GETOPT_MEDIALABEL	'M'	/* media object label (media.c) */
#define	GETOPT_TIMESTAMP	'N'	/* show timestamps in log msgs */
#define	GETOPT_OPTFILE		'O'	/* specifycmd line options file */
#define	GETOPT_RINGPIN		'P'	/* pin down I/O buffer ring */
#define	GETOPT_RESUME		'R'	/* resume intr dump (content_inode.c) */
#define	GETOPT_SINGLEMFILE	'S'	/* don't use multiple media files */
#define	GETOPT_NOTIMEOUTS	'T'	/* don't timeout dialogs */
#define	GETOPT_UNLOAD		'U'	/* unload media when change needed */
#define	GETOPT_SHOWLOGSS	'V'	/* show subsystem of log messages */
#define	GETOPT_SHOWLOGLEVEL	'W'	/* show level of log messages */
#define	GETOPT_RINGLEN		'Y'	/* specify I/O buffer ring length */
#define	GETOPT_MINIROOT		'Z'	/* apply miniroot restrictions */

#endif /* GETOPT_H */
