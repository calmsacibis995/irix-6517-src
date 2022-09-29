/*
 *=============================================================================
 *			File:		misc.c
 *			Purpose:	This file contains some miscellaneous
 *					routines that are used throughout hdisk
 *=============================================================================
 */
#include <stdio.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "misc.h"
#include "mkfp.h"

extern	int	fd;
extern	int	debug_flag;

/*
 *-----------------------------------------------------------------------------
 * gerror()
 *-----------------------------------------------------------------------------
 */
void	gerror(char *err_msg)
{
	fprintf(stderr, " mkfp: Error: %s\n", err_msg);
	return;
}

/*
 *-----------------------------------------------------------------------------
 * gopen()
 *-----------------------------------------------------------------------------
 */
void	gopen(char *dev_name)
{
	fd = open(dev_name, O_RDWR | O_NDELAY);
	if (fd == -1){
		gerror("open() fail\n");
		exit (MKFP_GENERIC);
	}
	return;
}


/*
 *-----------------------------------------------------------------------------
 * gmalloc()
 *-----------------------------------------------------------------------------
 */
void	*gmalloc(size_t size)
{
	void	*ptr;

	ptr = malloc(size);
	if (!ptr){
		gerror("malloc() fail\n");
		exit (MKFP_GENERIC);
	}
	bzero(ptr, size);
	return (ptr);
}

/*
 *-----------------------------------------------------------------------------
 * gseek()
 *-----------------------------------------------------------------------------
 */
void	gseek(off_t offset)
{
	off_t	noffset;

	noffset = lseek(fd, offset, SEEK_SET);
	if ((noffset != offset) || (noffset == -1)){
		gerror("lseek() fail\n");
		exit(MKFP_GENERIC);
	}
	return;
}

/*
 *-----------------------------------------------------------------------------
 * gread()
 *-----------------------------------------------------------------------------
 */
void	gread(void *buf, unsigned nbyte)
{
	int	nread;

	if (debug_flag > 1) {
		u_long w;

		w = lseek(fd, 0, SEEK_CUR);
		printf("gread: read @ %d (sector %d)\n", w, w / SECTOR_SIZE);
	}

	nread = read(fd, buf, nbyte);
	if ((nread != nbyte) || (nread == -1)){
		gerror("read() fail\n");
		exit(MKFP_GENERIC);
	}
	return;
}

/*
 *-----------------------------------------------------------------------------
 * gwrite()
 *-----------------------------------------------------------------------------
 */
void	gwrite(void *buf, unsigned nbyte)
{
	int	nwrite;

	if (debug_flag > 1) {
		u_long w;

		w = lseek(fd, 0, SEEK_CUR);
		printf("gwrite: write @ %d (sector %d)\n", w, w / SECTOR_SIZE);
	}

	nwrite = write(fd, buf, nbyte);

	if ((nwrite != nbyte) || (nwrite == -1)) {

		if (errno == EROFS)			/* Write-protected? */
		    gerror("Medium is write-protected\n");
		else
		    gerror("write() fail\n");

		exit(MKFP_GENERIC);
	}
	return;
}

/*
 *-----------------------------------------------------------------------------
 * align_short()
 *-----------------------------------------------------------------------------
 */
u_short align_short(u_short in)
{
        u_short out;

        out = (in & 0xFF) << 8 | in >> 8;
        return (out);
}

/*
 *-----------------------------------------------------------------------------
 * align_long()
 *-----------------------------------------------------------------------------
 */
u_int   align_int(u_int in)
{
        u_int   out;
        u_int   ll, lh, hl, hh;
        u_char  *inp = (u_char *) &in;

        ll = (in & 0x000000FF);
        lh = (in & 0x0000FF00);
        hl = (in & 0x00FF0000);
        hh = (in & 0xFF000000);
        out = ll | lh<<8 | hl<<16 | hh<<24;
        return (out);
}

/*
 *-----------------------------------------------------------------------------
 * spaceskip()
 *-----------------------------------------------------------------------------
 */
char    *spaceskip(char * strp)
{
        while (isspace(*strp) || *strp == '\t')
                strp++;
        return(strp);
}

/*
 *-----------------------------------------------------------------------------
 * ceiling()
 *-----------------------------------------------------------------------------
 */
u_int   ceiling(u_int num, u_int den)
{
        u_int   result;

        result = num/den;
        if (num%den)
                result++;
        return (result);
}

/*
 *----------------------------------------------------------------------------
 * ground()
 * This routine is used to print out the partition size, with units.
 *----------------------------------------------------------------------------
 */
char    *ground(u_long value)
{
	float	val;
        static  char    value_string[40];

        if (value < 1024)
                sprintf(value_string, "%d Bytes", value);
        else if (value >= 1024 && value < 1024*1024){
		val = (float) value/ (float) 1024;
                sprintf(value_string, "%.2f KBytes", val);
	}
        else {
		val = (float) value/(float) (1024*1024);
		sprintf(value_string, "%.2f MBytes", val);
	}
        return (value_string);
}

