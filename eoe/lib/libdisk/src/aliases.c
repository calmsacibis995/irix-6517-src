#include <devconvert.h>

#include <assert.h>
#include <invent.h>
#include <stdio.h>
#include <sys/conf.h>
#include <sys/iograph.h>
#include <sys/scsi.h>

/*****************************************************************************
 * Input Conversion 
 *
 * These routines convert a dev_t, a descriptor, or a special file's
 * path to a devroot.
 */

static int
to_devroot(const char drivername[MAXDEVNAME], char rootname[MAXDEVNAME])
{
    char *pattern, *match;

    if (!strncmp(drivername, "dksc", 4))
	pattern = "/" EDGE_LBL_DISK "/";
    else if (!strncmp(drivername, "tpsc", 4))
	pattern = "/" EDGE_LBL_TAPE "/";
    else if (!strncmp(drivername, "smfd", 4))
	pattern = "/floppy/";
    else if (!strncmp(drivername, "ds", 2))
    {
	/*
	 * The scsi driver is special.  Must remove the final
	 * "/scsi" component.
	 */

	match = rootname + strlen(rootname) - 5;
	if (match > rootname && !strcmp(match, "/" EDGE_LBL_SCSI))
	{   *match = '\0';
	    return 0;
	}
	/*
	 * That didn't work -- try matching "/scsi/" internally.
	 */
	pattern = "/" EDGE_LBL_SCSI "/";
    }
    else
	return -1;

    match = strstr(rootname, pattern);
    if (!match)
	return -1;
    
    *match = '\0';
    return 0;
}

static int
dev_to_devroot(dev_t dev, char rootname[MAXDEVNAME])
{
    char drivername[MAXDEVNAME];
    int rnlen = MAXDEVNAME;
    int dnlen = sizeof drivername;

    if (dev_to_drivername(dev, drivername, &dnlen) == NULL)
	return -1;
    if (dev_to_devname(dev, rootname, &rnlen) == NULL)
	return -1;
    return to_devroot(drivername, rootname);
}

static int
fdes_to_devroot(int fd, char rootname[MAXDEVNAME])
{
    char drivername[MAXDEVNAME];
    int rnlen = MAXDEVNAME;
    int dnlen = sizeof drivername;

    if (fdes_to_drivername(fd, drivername, &dnlen) == NULL)
	return -1;
    if (fdes_to_devname(fd, rootname, &rnlen) == NULL)
	return -1;
    return to_devroot(drivername, rootname);
}

static int
filename_to_devroot(const char *filename, char rootname[MAXDEVNAME])
{
    char drivername[MAXDEVNAME];
    int rnlen = MAXDEVNAME;
    int dnlen = sizeof drivername;

    if (filename_to_drivername((char *) filename, drivername, &dnlen) == NULL)
	return -1;
    if (filename_to_devname((char *) filename, rootname, &rnlen) == NULL)
	return -1;
    return to_devroot(drivername, rootname);
}

/******************************************************************************
 * Output Conversion
 *
 * These routines convert a devroot to a scsi, disk, tape, or floppy name.
 */

static char *
devroot_to_scsiname(char *rootname, char *scsiname_out, int *length_inout)
{
    int len;

    strcat(rootname, "/" EDGE_LBL_SCSI);
    len = (int)strlen(rootname) + 1;
    if (len > *length_inout)
	return NULL;
    strcpy(scsiname_out, rootname);
    *length_inout = len;
    return scsiname_out;
}

static char *
devroot_to_diskname(char *rootname,
		    dv_bool raw_in, int partno_in,
		    char *diskname_out, int *length_inout)
{
    int len;

    strcat(rootname, "/" EDGE_LBL_DISK "/");
    if (partno_in == SCSI_DISK_VH_PARTITION)
	strcat(rootname, EDGE_LBL_VOLUME_HEADER "/");
    else if (partno_in == SCSI_DISK_VOL_PARTITION)
	strcat(rootname, EDGE_LBL_VOLUME "/");
    else
	sprintf(rootname + strlen(rootname),
		EDGE_LBL_PARTITION "/%d/", partno_in);
    if (raw_in)
	strcat(rootname, EDGE_LBL_CHAR);
    else
	strcat(rootname, EDGE_LBL_BLOCK);
    len = (int)strlen(rootname);
    if (len > *length_inout)
	return NULL;
    strcpy(diskname_out, rootname);
    *length_inout = len;
    return diskname_out;
}

/*ARGSUSED*/
static char *
devroot_to_tapename(char *rootname,
		    dv_bool raw_in, dv_bool rewind_in,
		    dv_bool byteswap_in, dv_bool var_bsize_in,
		    int density_in, dv_bool compression_in,
		    char *tapename_out, int *length_inout)
{
    /* can't implement tape yet -- don't know what to return. */
    return NULL;
}

static char *
devroot_to_floppyname(char *rootname,
		      int diameter, int capacity,
		      char *floppyname_out, int *length_inout)
{
    int len;
    const char *suffix;

    if (diameter == 0 && capacity == 0)
	suffix = "3.5hi";
    else if (diameter == 525 && capacity == 360)
	suffix = "48";
    else if (diameter == 525 && capacity == 720)
	suffix = "96";
    else if (diameter == 525 && capacity == 1200)
	suffix = "96hi";
    else if (diameter == 350 && capacity == 720)
	suffix = "3.5";
    else if (diameter == 350 && capacity == 1440)
	suffix = "3.5hi";
    else if (diameter == 350 && capacity == 20331)
	suffix = "3.5.20m";
    else
	return NULL;
    strcat(rootname, "/floppy/");
    strcat(rootname, suffix);
    len = (int)strlen(rootname);
    if (len > *length_inout)
	return NULL;
    strcpy(floppyname_out, rootname);
    *length_inout = len;
    return floppyname_out;
}

/******************************************************************************
 * External Routines
 *
 * These externally visible routines convert a dev_t, a descriptor, or
 * a special file's path to the path to a disk, tape, scsi or floppy
 * special file.
 * 
 * Each is implemented as an input conversion followed by an output
 * conversion.
 */

char *
dev_to_scsiname(dev_t dev_in, char *scsiname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (dev_to_devroot(dev_in, tempname))
	return NULL;
    return devroot_to_scsiname(tempname, scsiname_out, length_inout);
}

char *
fdes_to_scsiname(int fdes_in, char *scsiname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (fdes_to_devroot(fdes_in, tempname))
	return NULL;
    return devroot_to_scsiname(tempname, scsiname_out, length_inout);
}

char *
filename_to_scsiname(const char *filename_in,
			   char *scsiname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (filename_to_devroot(filename_in, tempname))
	return NULL;
    return devroot_to_scsiname(tempname, scsiname_out, length_inout);
}

char *
dev_to_diskname(dev_t dev_in,
		      dv_bool raw_in, int partno_in,
		      char *diskname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (dev_to_devroot(dev_in, tempname))
	return NULL;
    return devroot_to_diskname(tempname,
			       raw_in, partno_in,
			       diskname_out, length_inout);
}

char *
fdes_to_diskname(int fdes_in,
		 dv_bool raw_in, int partno_in,
		 char *diskname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (fdes_to_devroot(fdes_in, tempname))
	return NULL;
    return devroot_to_diskname(tempname,
			       raw_in, partno_in,
			       diskname_out, length_inout);
}

char *
filename_to_diskname(const char *filename_in,
		     dv_bool raw_in, int partno_in,
		     char *diskname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (filename_to_devroot(filename_in, tempname))
	return NULL;
    return devroot_to_diskname(tempname,
			       raw_in, partno_in,
			       diskname_out, length_inout);
}

char *
dev_to_tapename(dev_t dev_in,
		dv_bool raw_in, dv_bool rewind_in,
		dv_bool byteswap_in, dv_bool var_bsize_in,
		int density_in, dv_bool compression_in,
		char *tapename_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (dev_to_devroot(dev_in, tempname))
	return NULL;
    return devroot_to_tapename(tempname,
			       raw_in, rewind_in,
			       byteswap_in, var_bsize_in,
			       density_in, compression_in,
			       tapename_out, length_inout);
}

char *
fdes_to_tapename(int fdes_in,
		 dv_bool raw_in, dv_bool rewind_in,
		 dv_bool byteswap_in, dv_bool var_bsize_in,
		 int density_in, dv_bool compression_in,
		 char *tapename_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (fdes_to_devroot(fdes_in, tempname))
	return NULL;
    return devroot_to_tapename(tempname,
			       raw_in, rewind_in,
			       byteswap_in, var_bsize_in,
			       density_in, compression_in,
			       tapename_out, length_inout);
}


char *
filename_to_tapename(const char *filename_in,
		     dv_bool raw_in, dv_bool rewind_in,
		     dv_bool byteswap_in, dv_bool var_bsize_in,
		     int density_in, dv_bool compression_in,
		     char *tapename_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (filename_to_devroot(filename_in, tempname))
	return NULL;
    return devroot_to_tapename(tempname,
			       raw_in, rewind_in,
			       byteswap_in, var_bsize_in,
			       density_in, compression_in,
			       tapename_out, length_inout);
}

/* diameter is either 525 for 5 1/4" floppies or 350 for 3 1/2" floppies. */

char *
dev_to_floppyname(dev_t dev_in,
		  int diameter, int capacity,
		  char *floppyname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (dev_to_devroot(dev_in, tempname))
	return NULL;
    return devroot_to_floppyname(tempname,
				 diameter, capacity,
				 floppyname_out, length_inout);
}

char *
fdes_to_floppyname(int fdes_in,
		   int diameter, int capacity,
		   char *floppyname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (fdes_to_devroot(fdes_in, tempname))
	return NULL;
    return devroot_to_floppyname(tempname,
				 diameter, capacity,
				 floppyname_out, length_inout);
}

char *
filename_to_floppyname(const char *filename_in,
		       int diameter, int capacity,
		       char *floppyname_out, int *length_inout)
{
    char tempname[MAXDEVNAME];

    if (filename_to_devroot(filename_in, tempname))
	return NULL;
    return devroot_to_floppyname(tempname,
				 diameter, capacity,
				 floppyname_out, length_inout);
}


#ifdef UNIT_TEST_aliases

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

void
three_disks(const char *path, dv_bool raw, int partno)
{
    char *p;
    char buf[MAXDEVNAME];
    int fd;
    struct stat st;
    int len;

    len = sizeof buf;
    p = filename_to_diskname(path, raw, partno, buf, &len);
    if (p)
	printf("filename_to_diskname(\"%s\", raw=%d, part=%d) = \"%s\"\n",
	       path, raw, partno, p);
    else
	printf("filename_to_diskname(\"%s\", raw=%d, part=%d) FAILED\n",
	       path, raw, partno);
	

    fd = open(path, O_RDONLY);
    if (fd < 0)
	perror(path);
    else
    {
	len = sizeof buf;
	p = fdes_to_diskname(fd, raw, partno, buf, &len);
	close(fd);
	if (p)
	    printf("    fdes_to_diskname(\"%s\", raw=%d, part=%d) = \"%s\"\n",
		   path, raw, partno, p);
	else
	    printf("    fdes_to_diskname(\"%s\", raw=%d, part=%d) FAILED\n",
		   path, raw, partno);
    }

    if (stat(path, &st) < 0)
	perror(path);
    else
    {
	len = sizeof buf;
	p = dev_to_diskname(st.st_rdev, raw, partno, buf, &len);
	if (p)
	    printf("     dev_to_diskname(\"%s\", raw=%d, part=%d) = \"%s\"\n",
		   path, raw, partno, p);
	else
	    printf("     dev_to_diskname(\"%s\", raw=%d, part=%d) FAILED\n",
		   path, raw, partno);
    }
}

void
three_floppies(const char *path, int diam, int capacity)
{
    char *p;
    char buf[MAXDEVNAME];
    int fd;
    struct stat st;
    int len;

    len = sizeof buf;
    p = filename_to_floppyname(path, diam, capacity, buf, &len);
    if (p)
	printf("filename_to_floppyname(\"%s\", diam=%d, capacity=%d) = \"%s\"\n",
	       path, diam, capacity, p);
    else
	printf("filename_to_floppyname(\"%s\", diam=%d, capacity=%d) FAILED\n",
	       path, diam, capacity);
	

    fd = open(path, O_RDONLY);
    if (fd < 0)
	perror(path);
    else
    {
	len = sizeof buf;
	p = fdes_to_floppyname(fd, diam, capacity, buf, &len);
	close(fd);
	if (p)
	    printf("    fdes_to_floppyname(\"%s\", diam=%d, capacity=%d) = \"%s\"\n",
		   path, diam, capacity, p);
	else
	    printf("    fdes_to_floppyname(\"%s\", diam=%d, capacity=%d) FAILED\n",
		   path, diam, capacity);
    }

    if (stat(path, &st) < 0)
	perror(path);
    else
    {
	len = sizeof buf;
	p = dev_to_floppyname(st.st_rdev, diam, capacity, buf, &len);
	if (p)
	    printf("     dev_to_floppyname(\"%s\", diam=%d, capacity=%d) = \"%s\"\n",
		   path, diam, capacity, p);
	else
	    printf("     dev_to_floppyname(\"%s\", diam=%d, capacity=%d) FAILED\n",
		   path, diam, capacity);
    }
}

void
three_scsis(const char *path)
{
    char *p;
    char buf[MAXDEVNAME];
    int fd;
    struct stat st;
    int len;

    len = sizeof buf;
    p = filename_to_scsiname(path, buf, &len);
    if (p)
	printf("filename_to_scsiname(\"%s\") = \"%s\"\n",
	       path, p);
    else
	printf("filename_to_scsiname(\"%s\") FAILED\n",
	       path);
	

    fd = open(path, O_RDONLY);
    if (fd < 0)
	perror(path);
    else
    {
	len = sizeof buf;
	p = fdes_to_scsiname(fd, buf, &len);
	close(fd);
	if (p)
	    printf("    fdes_to_scsiname(\"%s\") = \"%s\"\n",
		   path, p);
	else
	    printf("    fdes_to_scsiname(\"%s\") FAILED\n",
		   path);
    }

    if (stat(path, &st) < 0)
	perror(path);
    else
    {
	len = sizeof buf;
	p = dev_to_scsiname(st.st_rdev, buf, &len);
	if (p)
	    printf("     dev_to_scsiname(\"%s\") = \"%s\"\n",
		   path, p);
	else
	    printf("     dev_to_scsiname(\"%s\") FAILED\n",
		   path);
    }
}

void
three_tapes(const char *path,
	    dv_bool raw, dv_bool rew,
	    dv_bool swap, dv_bool var,
	    int dens, dv_bool cmp)
{
    char *p;
    char buf[MAXDEVNAME];
    int fd;
    struct stat st;
    int len;

    len = sizeof buf;
    p = filename_to_tapename(path, raw, rew, swap, var, dens, cmp, buf, &len);
    if (p)
	printf("filename_to_tapename(\"%s\", raw=%d, rew=%d, swap=%d, var=%d, dens=%d, cmp=%d) = \"%s\"\n",
	       path, raw, rew, swap, var,dens, cmp, p);
    else
	printf("filename_to_tapename(\"%s\", rew=%d, swap=%d, var=%d, dens=%d, cmp=%d) FAILED\n",
	       path);
	

    fd = open(path, O_RDONLY);
    if (fd < 0)
	perror(path);
    else
    {
	len = sizeof buf;
	p = fdes_to_tapename(fd, raw, rew, swap, var, dens, cmp, buf, &len);
	close(fd);
	if (p)
	    printf("    fdes_to_tapename(\"%s\", raw=%d, rew=%d, swap=%d, var=%d, dens=%d, cmp=%d) = \"%s\"\n",
		   path, raw, rew, swap, var,dens, cmp, p);
	else
	    printf("    fdes_to_tapename(\"%s\", rew=%d, swap=%d, var=%d, dens=%d, cmp=%d) FAILED\n",
		   path);
    }

    if (stat(path, &st) < 0)
	perror(path);
    else
    {
	len = sizeof buf;
	p = dev_to_tapename(st.st_rdev, raw, rew, swap, var, dens, cmp, buf, &len);
	if (p)
	    printf("     dev_to_tapename(\"%s\", raw=%d, rew=%d, swap=%d, var=%d, dens=%d, cmp=%d) = \"%s\"\n",
		   path, raw, rew, swap, var,dens, cmp, p);
	else
	    printf("     dev_to_tapename(\"%s\", rew=%d, swap=%d, var=%d, dens=%d, cmp=%d) FAILED\n",
		   path);
    }
}

struct flop {
    int diam;
    int capacity;
} flops[] = {
    {   0, 0 },
    { 525,   360 },
    { 525,   720 },
    { 525,  1200 },
    { 350,   720 },
    { 350,  1440 },
    { 350, 20331 }
};

main()
{
    int i, j;

    three_scsis("/dev/root");
    for (i = 0; i <= 1; i++)
	for (j = 0; j <= 10; j++)
	    three_disks("/dev/root", i, j);
    for (i = 0; i < sizeof flops / sizeof flops[0]; i++)
	three_floppies("/dev/root", flops[i].diam, flops[i].capacity);
    three_tapes("/dev/root", 0, 0, 0, 0, 0, 0);
    three_tapes("/dev/root", 1, 0, 0, 0, 0, 0);
    three_tapes("/dev/root", 0, 1, 0, 0, 0, 0);
    three_tapes("/dev/root", 1, 1, 0, 0, 0, 0);
    return 0;
}

#endif /* UNIT_TEST_aliases */
