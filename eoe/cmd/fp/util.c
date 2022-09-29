

#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "mkfp.h"
#include "fp.h"
#include "macLibrary.h"

extern int debug_flag;

#define    SECTOR_SIZE    (512)


/*
 * flush buffered output if any
 */
void
flushoutp(void)
{
    fflush(stdout);
}


void *
safemalloc(u_int chunksize)
{
    void * bufp;

    if ((bufp = (void *) malloc(chunksize)) == NULL) {
        fpError("out of memory");
        exit(MKFP_GENERIC);
    }
    return(bufp);
}


void *
safecalloc(u_int chunksize)
{
    void * bufp;

    if ((bufp = (void *) calloc(chunksize, 1)) == NULL) {
        fpError("out of memory\n");
        exit(MKFP_GENERIC);
    }
    return(bufp);
}



void
fpblockwrite(int fd, u_char * buf, int bufsz)
{
    if (debug_flag > 1) {
	    u_long w;

	    w = lseek(fd, 0, SEEK_CUR);
	    printf("fpblockwrite: write @ %d (sector %d)\n",
							w, w / SECTOR_SIZE);
    }

    if (write(fd, buf, bufsz) != bufsz) {

	if (errno == EROFS)			/* Write-protected? */
	    fpSysError("Medium is write-protected\n");
	else
	    fpSysError("Write error to the media!\n");

        exit(MKFP_GENERIC);
    }
}



void
fpblockread(int fd, u_char * buf, int bufsz)
{
    if (debug_flag > 1) {
	u_long w;

	w = lseek(fd, 0, SEEK_CUR);
	printf("fpblockread: read @ %d (sector %d)\n", w, w / SECTOR_SIZE);
    }

    if (read(fd, buf, bufsz) < 0) {
        fpSysError("Read error from media!\n");
        exit(MKFP_GENERIC);
    }
}


int
rfpblockwrite(int fd, u_char * buf, int bufsz)
{
    if (debug_flag > 1) {
	u_long w;

	w = lseek(fd, 0, SEEK_CUR);
	printf("rfpblockwrite: write @ %d (sector %d)\n",
							w, w / SECTOR_SIZE);
    }

    if (write(fd, buf, bufsz) != bufsz) { 

	if (errno == EROFS)			/* Write-protected? */
	    return E_PROTECTION;
	else
	    return E_WRITE;
    }

    return(E_NONE);
}



int
rfpblockread(int fd, u_char * buf, int bufsz)
{
    if (debug_flag > 1) {
	u_long w;

	w = lseek(fd, 0, SEEK_CUR);
	printf("rfpblockread: read @ %d (sector %d)\n", w, w / SECTOR_SIZE);
    }

    if (read(fd, buf, bufsz) < 0)
	return(E_READ);

    return(E_NONE);
}

