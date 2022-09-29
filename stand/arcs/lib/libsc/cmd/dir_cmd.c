/*
 * dir_cmd.c - print a directory listing
 *
 * $Revision: 1.5 $
 */
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/dirent.h>
#include <libsc.h>

dir(int argc, char *argv[])
{
    LONG i, rc, done = 0, count = 10, retval;
    DIRECTORYENTRY *bufptr, *buf;
    ULONG fid;

    if (argc < 2)
	return -1;

    if ((buf=(DIRECTORYENTRY *)malloc(sizeof(DIRECTORYENTRY) * count)) == NULL){
	printf("Unable to malloc buffer\n");
	return -1;
    }

    argv++;
    if (Open(*argv, OpenReadOnly, &fid) != ESUCCESS) {
 	printf("\nError Opening File %s", *argv); 
	return -1;
    }

	

    printf("\nDIRECTORY\n");
    while (!done) {
        retval = GetDirEntry(fid, buf, count, &rc);
	switch (retval) {
	case ESUCCESS:
	    bufptr = buf;
	    for (i = 0; i < rc; i++)  {
	        printf("Attribute %x   ", bufptr->FileAttribute);
	        printf("FileName Length %d   ", bufptr->FileNameLength);
	        printf("Filename %s   \n", bufptr->FileName);
	        bufptr++;
		}
	    break;
	case ENOTDIR:
	    done = 1;
	    break;
	case EBADF:
	    printf("\nError Reading Directory\n");
	    done = 1;
	}
    }

    Close(fid);
    free(buf);

    return 0;
}
