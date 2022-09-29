#ifndef GETOPT_H
#define GETOPT_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/inventory/RCS/getopt.h,v 1.3 1995/07/11 22:35:27 sup Exp $"

/* getopt.h	common getopt  command string
 *
 * several modules parse the command line looking for arguments specific to
 * that module. Unfortunately, each of the getopt(3) calls needs the
 * complete command string, to know how to parse. This file's purpose is
 * to contain that command string. It also abstracts the option letters,
 * facilitating easy changes.
 */

#define GETOPT_CMDSTRING	"gwrqdL:u:l:s:t:v:m:f:i"

#define	GETOPT_DUMPDEST		'f'	/* dump dest. file (drive.c) */
#define	GETOPT_LEVEL		'l'	/* dump level (content_inode.c) */
#define	GETOPT_SUBTREE		's'	/* subtree dump (content_inode.c) */
#define	GETOPT_VERBOSITY	'v'	/* verbosity level (0 to 4 ) */
#define	GETOPT_DUMPLABEL	'L'	/* dump session label (global.c) */
#define	GETOPT_MEDIALABEL	'M'	/* media object label (content.c) */
#define	GETOPT_RESUME		'R'	/* resume intr dump (content_inode.c) */
#define GETOPT_INVPRINT         'i'     /* just display the inventory */
/*
 * f - dump destination:	drive.c
 * l - dump level		content_inode.c
 * s - subtree			content.c
 * v - verbosity		mlog.c
 * L - dump session label	global.c
 * M - media object label	media.c
 * R - resume interrupted dump	content_inode.c
 */

#endif /* GETOPT_H */
