#ident	"lib/libsc/cmd/ls_cmd.c:  $Revision: 1.33 $"

/* 
 * ls_cmd.c - a simple minded ls command 
 */

#include <stringlist.h>
#include <arcs/io.h>
#include <arcs/dirent.h>
#include <arcs/errno.h>
#include <parser.h>
#include <libsc.h>
#include <libsc_internal.h>

#define NENTRIES 4

/* If no / is found, then assume we are listing a disk volhdr, or
 * a tape filesystem, as appropriate
 */

/*ARGSUSED*/
int
ls(int argc, char **argv, char **bunk1, struct cmd_table *bunk2)
{
	DIRECTORYENTRY direntries[NENTRIES];
	struct string_list path_list;
	ULONG fd, cc, len;
	int didprint;
	char *path;
	long rc;

	/* if no argument is provided, use the $path environment
	 * variable to determine which devices to list
	 */
	if (argc == 1) {
		if ((path = (char *) makepath ()) == NULL) {
			printf ("No path information in environment.\n");
			return(1);
		}
		if ((argc = _argvize (path, &path_list)) == 0) {
			printf ("Invalid path information in environment.\n");
			return(1);
		}
		argv = path_list.strptrs - 1; 	/* cool */
		argc++;
	}

	while (--argc > 0) {
		argv++;

		/*  Try given filename, or filename/ if filename is
		 * fails with ENOTDIR.  This is so "ls dksc(0,1,8)"
		 * works.
		 */
		if (rc = Open((CHAR *)*argv, OpenDirectory, &fd)) {
			if (rc == ENOTDIR) {
				char buf[LINESIZE];
				strcpy(buf,*argv);
				strcat(buf,"/");
				if (Open((CHAR *)buf,OpenDirectory,&fd) ==
				    ESUCCESS)
					goto ok;
			}
			if (rc == ENXIO) /* File name, but no file system */
				continue;
			perror(rc,*argv);
			continue;
		}

ok:
		printf("%s:\n", *argv);

		/* search the directory */
		didprint = 0;
		len = 0;

		while ((rc=GetDirectoryEntry(fd,direntries,NENTRIES,&cc)) == 0
			&& cc) {
			ULONG count = 0;
			do {
				DIRECTORYENTRY *dp = &direntries[count++];
				ULONG namelen = dp->FileNameLength;

				if (namelen > FileNameLengthMax) {
					/* truncate filename and try it... */
					dp->FileName[FileNameLengthMax-1]='\0';
					goto done;
				}

				if (len + namelen >= 79) {
					printf ("\n");
					len = 0;
				}
				printf("%s%s", len ? "  " : "", dp->FileName);
				len += 2 + namelen;
				didprint = 1;
			} while (count < cc);;
		}
		if (didprint)
			printf("\n");

		/*  GetDirectoryEntries() returns ENOTDIR after all entries
		 * are read.
		 */
		if (rc && (rc != ENOTDIR))
			perror(rc,*argv);

done:
		if (rc=Close(fd))
			perror(rc,*argv);
	}
	return(0);
}
