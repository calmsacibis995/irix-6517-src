/*
 ****************************************************************************
 *    Copyright © 1996 Ordinal Technology Corp.  All Rights Reserved.       *
 *                                                                          *
 *    DO NOT DISCLOSE THIS SOURCE CODE OUTSIDE OF SILICON GRAPHICS, INC.    *
 *  This source code contains unpublished proprietary information of        *
 *  Ordinal Technology Corp.  The copyright notice above is not evidence    *
 *  of any actual or intended publication of this information.              *
 *      This source code is licensed to Silicon Graphics, Inc. for          *
 *  its personal use in developing and maintaining products for the IRIX    *
 *  operating system.  This source code may not be disclosed to any third   *
 *  party outside of Silicon Graphics, Inc. without the prior written       *
 *  consent of Ordinal Technology Corp.                                     *
 ****************************************************************************
 *
 *
 * fileinfo.c    - get optimum i/o size, etc, info about files
 *
 *	$Ordinal-Id: fileinfo.c,v 1.22 1996/11/06 18:49:04 charles Exp $
 */
#ident	"$Revision: 1.1 $"

#include "otcsort.h"
#include <sys/syssgi.h>
#if 0
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>                /* private interface */
#include <sys/xlv_attr.h>               /* private interface */
#endif

#define	CREATE_PERM	(S_IRUSR | S_IWUSR | S_IRGRP  | S_IROTH)

#if 0
typedef struct lvinfo
{
    char		*name;		/* device name */
    unsigned		opt_iosize;	/* unit of optimial i/o, in bytes */
    struct lvinfo	*next;
} lvinfo_t;

lvinfo_t	*Volumes = NULL;
xlv_tab_subvol_t *SubvolSpace;

/*
 * get_subvol_space: memory allocation routine, courtesy of jleong
 */
xlv_tab_subvol_t *get_subvol_space(void)
{
    int                     i, plexsize;
    xlv_tab_plex_t          *plex;
    xlv_tab_subvol_t        *subvol;

    subvol = (xlv_tab_subvol_t *)malloc(sizeof(xlv_tab_subvol_t));
    bzero(subvol, sizeof(xlv_tab_subvol_t));
    plexsize = sizeof(xlv_tab_plex_t) +
	    (sizeof(xlv_tab_vol_elmnt_t) * (XLV_MAX_VE_PER_PLEX-1));

    for (i = 0; i < XLV_MAX_PLEXES; i++)
    {
	plex = (xlv_tab_plex_t *) malloc(plexsize);
	bzero(plex, plexsize);
	plex->max_vol_elmnts = XLV_MAX_VE_PER_PLEX;
	subvol->plex[i] = plex;
    }
    return (subvol);

} /* end of get_subvol_space() */

/*
* get_lvinfo	- obtain all the logical volume information on the system
 *
 *	Searches for all kinds of logical volumes which might be on the system.
 *	This is done only once per nsort process, not once per file or sort.
 *	All found volumes are saved in the Volume list. Any disk devices which
 *	are not in this list are treated as dissociated spindles.
 */
void get_lvinfo(sort_t *sort)
{
    lvinfo_t		lv;
    xlv_attr_cursor_t	cursor;
    xlv_attr_req_t	req;
    xlv_tab_vol_entry_t	vol;
    xlv_tab_subvol_t	*svp;
    int			status;

    if (Volumes != NULL)
	return;

    /*
     * First get all the xlv volumes, if any.
     */
    if (syssgi(SGI_XLV_ATTR_CURSOR, &cursor) >= 0)
    {
        if (NULL == (vol.log_subvol = get_subvol_space()))
	    die("get_lvinfo:Cannot malloc the log subvolume entry.");
        if (NULL == (vol.data_subvol = get_subvol_space()))
	    die("get_lvinfo:Cannot malloc the data subvolume entry.");
        if (NULL == (vol.rt_subvol = get_subvol_space()))
	    die("get_lvinfo:Cannot malloc the realtime subvolume entry.");
	SubvolSpace = vol.data_subvol;

	/*
	 * Enumerate the volumes.
	 */
        req.attr = XLV_ATTR_VOL;
        req.ar_vp = &vol;
        for (;;)
	{
	    status = syssgi(SGI_XLV_ATTR_GET, &cursor, &req);
	    if (status >= 0)
	    {
		/* do something with the subvols -- what?? */
	    }
	    else if (errno == ENFILE || errno == ENOENT)
		break;
	    else
		die("syssgi(SGI_XLV_ATTR_GET) failed: %s", strerror(errno));
        }
    }
}
#endif

/* get_align_info
 *
 */
void get_align_info(fileinfo_t *file)
{
    int			err;
    struct dioattr	dioinfo;

    switch (file->io_mode)
    {
      case IOMODE_MAPPED:
	file->mem_align = SYS.page_size;
	file->min_io = SYS.page_size;
	file->max_io = INT_MAX;
	break;

      case IOMODE_SERIAL:
      case IOMODE_BUFFERED:
	file->mem_align = LL_BYTES;	/* why not start on a line boundry? */
	file->min_io = 1;
	file->max_io = INT_MAX;
	break;

      case IOMODE_DIRECT:
	set_file_flags(file, FDIRECT);
	file->openmode |= O_DIRECT;

	err = fcntl(file->fd, F_DIOINFO, &dioinfo);
	if (err < 0)
	    die("Attempt to obtain direct i/o info for %s failed: %s",
		    file->name, strerror(errno));
	file->mem_align = dioinfo.d_mem;
	file->min_io = dioinfo.d_miniosz;
	file->max_io = dioinfo.d_maxiosz;
	break;
    }
}

/*
 * fix_sort_files	- file-related tasks to do at prepare_sort_def() time
 *
 *	- set the aio_count and io_size appropriately for each file.
 *	  In order from most to least significant, they are:
 *	1 - absolute file restrictions (e.g. aiocount == 1 for pipes)
 *	2 - the infile, outfile, or tempfile specification
 *	3 - the filesystem defaults specified by the user on the command line
 *	4 - the filesystem defaults specified in /usr/nsort/nsort.params
 *	5 - the 'best guesses' compiled into nsort
 */
void fix_sort_files(sort_t *sort)
{
    fileinfo_t	*file;
    int		i;
    u8		file_demand;
    u8		mem_demand;
    u8		recs_left;
    int		max_aio;
    int		max_io_size;

    IN.dio_mem = LL_BYTES;
    IN.dio_io = 1;
    if (!sort->api_invoked)
    {
	/* declare that the input files are compatitible with single-process
	 * mode if we know the input size.  (If we don't know the input size
	 * then this sort could end up becoming an two-pass sort, requiring
	 * extra sprocs for temporary file i/o.)
	 */
	IN.single_ok = (IN.input_size != MAX_I8);

	/* set input alignment requirement to the maximum of:
	 *	1) the line size (as in line-list)
	 *  2) the memory
	 * Set the aio_count to the maximum of the input files' count,
	 * for figuring the memory needs of the aio handling
	 * During the sort IN.aio_cnt is for the current file
	 */
	max_aio = 1;
	max_io_size = 1;
	mem_demand = 8192 * 1024;	/* nsort base memory need of 8-12M */
	recs_left = sort->n_recs;
	if (recs_left == 0)
	    recs_left = MAX_I8;
	for (file = IN.files; file < IN.files + IN.n_files; file++)
	{
	    /* if file mode unspecified and there is no relavent file system
	     * mode definition.
	     */
	    file_demand = file->stbuf.st_size;
	    if (sort->n_recs && (RECORD.flags & RECORD_FIXED) &&
		recs_left * RECORD.length < file->stbuf.st_size)
		file_demand = recs_left * RECORD.length;
	    mem_demand += file_demand;
	    if (file->io_mode == IOMODE_UNSPECIFIED &&
		!get_filesys_iomode(sort, file))
	    {
		/* if we can't seek on this file, we must read it serially
		 */
		if (!CAN_SEEK(file->stbuf.st_mode))
		    file->io_mode = IOMODE_SERIAL;
		else
		{
		    /* if all the input could fit in the buffer cache,
		     * mmap this input file, else use direct i/o.
		     * XXX 'all the input'? could change to per-file map/direct
		     */
		    if (file_demand >= SYS.buffer_cache_sz)
			file->io_mode = IOMODE_DIRECT;
		    else
		    {
			file->io_mode = IOMODE_MAPPED;
			if (RECORD.flags & RECORD_FIXED)
			    recs_left -= i / RECORD.length;
		    }
		}
	    }

	    if (file->io_mode == IOMODE_MAPPED)
		sort->touch_memory = FlagFalse;

	    /* if input file mode is not mapped or serial, can't do single
	     * process mode.
	     */
	    if (!(file->io_mode == IOMODE_MAPPED ||
		  file->io_mode == IOMODE_SERIAL))
	    {
		IN.single_ok = FALSE;
	    }

	    get_align_info(file);
	    
	    if (file->io_size != 0)
	    {
		if (file->io_size % file->min_io)
		    runtime_error(sort, NSORT_IOSIZE_UNALIGNED, file->name,
								file->min_io);
		else if (file->io_size > file->max_io)
		    runtime_error(sort, NSORT_IOSIZE_TOO_LARGE, file->name,
								file->max_io);
	    }
	    if (IN.dio_mem < file->mem_align)
		IN.dio_mem = file->mem_align;
	    if (IN.dio_io < file->min_io)
		IN.dio_io = file->min_io;
	
	    if (file->aio_count == 0 && !get_filesys_aiocount(sort, file))
		file->aio_count = IN.aio_cnt;
	    if (max_aio < file->aio_count)
		max_aio = file->aio_count;
	
	    if (file->io_size == 0 && !get_filesys_iosize(sort, file))
		file->io_size = IN.read_size;
	    if (max_io_size < file->io_size)
		max_io_size = file->io_size;
	}
	IN.aio_cnt = max_aio;
	IN.read_size = max_io_size;	/* XXX no longer looked at after here?*/

	if (OUT.file->name == NULL)
	    assign_outfile(sort, "-");
	
	/* for output files the aio count and write size is 'obtained' from
	 * the fileinfo, but it copied to OUT.aio_count and write_size for use
	 */
	if (OUT.file->aio_count == 0 && !get_filesys_aiocount(sort, OUT.file))
	    OUT.file->aio_count = OUT.aio_cnt;
	OUT.aio_cnt = OUT.file->aio_count;
	if (OUT.file->io_size == 0 && !get_filesys_iosize(sort, OUT.file))
	    OUT.file->io_size = OUT.buf_size;
	OUT.buf_size = OUT.file->io_size;
	
	if (OUT.file->io_mode == IOMODE_UNSPECIFIED &&
	    !get_filesys_iomode(sort, OUT.file))
	{
	    /* if we can't seek on this file, we must write it serially
	     */
	    if (!CAN_SEEK(OUT.file->stbuf.st_mode))
		OUT.file->io_mode = IOMODE_SERIAL;
	    else
	    {
		if (IN.input_size != MAX_I8)
		{
		    mem_demand += IN.input_size;
		    if (sort->edit_growth != 0)
			mem_demand += (IN.input_size / RECORD.length) *
					sort->edit_growth;
		}

		/* if the projected output file size is less than half the
		 * buffer cache size (both input and output can fit in the
		 * buffer cache), use buffered or serial mode, else use
		 * direct mode.
		 */
		OUT.file->io_mode = (mem_demand <
				     SYS.buffer_cache_sz + SYS.free_memory)
				    ? IOMODE_BUFFERED : IOMODE_DIRECT;
	    }
	}

	get_align_info(OUT.file);

	if (OUT.file->io_size != 0 && OUT.file->io_mode == IOMODE_DIRECT)
	{
	    if (OUT.file->io_size % OUT.file->min_io)
		runtime_error(sort, NSORT_IOSIZE_UNALIGNED, OUT.file->name, OUT.file->min_io);
	    else if (OUT.file->io_size > OUT.file->max_io)
		runtime_error(sort, NSORT_IOSIZE_TOO_LARGE, OUT.file->name, OUT.file->max_io);
	}
	/* If we are appending to an existing file our initial write offset
	 * may not permit direct i/o.  If so we'll fall back to buffered.
	 * XXX This ought to be a factor in choosing input buffered/direct i/o.
	 * XXX because it increases the demand on the buffer cache.
	 */
	if (OUT.write_offset % OUT.file->min_io)
	{
	    OUT.file->io_mode = IOMODE_BUFFERED;
	    get_align_info(OUT.file);
	}

	/* if output file is buffered, have sort sprocs perform write()s
	 * after copying records (pointer sort) or merging (record sort).
	 * XXX serial should do this too?
	 */
	if (OUT.file->io_mode == IOMODE_BUFFERED)
	    OUT.file->copies_write = TRUE;

	/* It appears that in IRIX 5.3 and 6.{2,4} a filesystem-buffered write
	 * that is in the last 64KB of a file might not always be seen by a
	 * subsequent direct read. Circumvent this behavior by forcing the
	 * final_write to be done in FSYNC mode
	 */
	if ((OUT.file->io_mode == IOMODE_BUFFERED ||
	     OUT.file->io_mode == IOMODE_SERIAL) && OUT.file->sync)
	    OUT.file->min_io = 64*1024;
	
	OUT.dio_mem = OUT.file->mem_align;
	OUT.dio_io = OUT.file->min_io;

	OUT.write_size = min(OUT.file->max_io, OUT.buf_size);
    }

    /* Set the temp's aio_count to sum of each individual temp file's count.
     * Set the io size to the size of the first temp file ? XXX
     * During the sort TEMP.aio_cnt remains this value, the sum of the
     * aio counts for all the temp files.
     */
    TEMP.dio_mem = LL_BYTES;
    TEMP.dio_io = LL_BYTES;
    if (TEMP.n_files > 0)
    {
	max_aio = TEMP.aio_cnt;
	max_io_size = TEMP.blk_size;
	for (i = 0; i < TEMP.n_files; i++)
	{
	    file = &TEMP.files[i].fileinfo;

	    file->io_mode = IOMODE_DIRECT;

	    get_align_info(file);

	    if (!get_filesys_aiocount(sort, file))
		file->aio_count = TEMP.aio_cnt;
	    if (max_aio < file->aio_count)
		max_aio = file->aio_count;

	    if (!get_filesys_iosize(sort, file))
		file->io_size = TEMP.blk_size;
	    if (file->io_size % file->min_io)
	    {
		runtime_error(sort, NSORT_IOSIZE_UNALIGNED, file->name, file->min_io);
		return;
	    }
	    else if (file->io_size > file->max_io)
	    {
		runtime_error(sort, NSORT_IOSIZE_TOO_LARGE, file->name, file->max_io);
		return;
	    }
	    if (max_io_size < file->io_size)
		max_io_size = file->io_size;

	    if (TEMP.dio_mem < file->mem_align)
		TEMP.dio_mem = file->mem_align;
	    if (TEMP.dio_io < file->min_io)
		TEMP.dio_io = file->min_io;
	}
	TEMP.aio_cnt = max_aio;
	TEMP.blk_size = max_io_size;
    }
}

void get_file_stat(fileinfo_t *file)
{
    struct stat64	statbuf;

    if (fstat64(file->fd, &statbuf) < 0)
	die("fstat64 getting size of %s failed: %s",
	    file->name, strerror(errno));
    
    file->stbuf.st_mode = statbuf.st_mode;
    file->stbuf.st_size = statbuf.st_size;
    file->stbuf.st_dev = statbuf.st_dev;

    /* If we can't seek on the file, then limit the number of outstanding
     * aio's to 1. Also limit the aios to 1 if we'd have to dup() the file
     * descriptor in order to get another one (i.e. we don't have the name)
     * 
     * XXX Do several aio mmap()'ing sprocs help with a dup()'d fd?  At this
     * XXX point in the code we haven't yet parsed the file options to see
     * XXX whether the user wants to mmap()
     */
    if (!CAN_SEEK(file->stbuf.st_mode))
    {
	file->aio_count = 1;
	file->stbuf.st_size = MAX_I8;
    }
}

/*
 * another_infile	- parse an input file specification
 *
 *		filename, optionally followed by filespec options
 *
 */
const char *another_infile(sort_t *sort, const char *p)
{
    fileinfo_t	*infile;
    int		fd;
    int		mode;
    int		errs = sort->n_errors;
    token_t	token;

    PARSE.charno = 0;		/* XXX reset position - ugly */
    PARSE.line = p;
    if (get_token(sort, &p, &token, FileSpecs) != tokFILENAME ||
	token.value.strval[0] == '\0')
	parser_error(sort, &token, NSORT_FILENAME_MISSING);

    /* first attempt to open file with direct i/o, unless environment flag
     * says otherwise.
     */
    if (strcmp(token.value.strval, "-") == 0)
    {
	strcpy(token.value.strval, "<standard input>");
	fd = fileno(stdin);
	mode = -1;	/* can't open stdin again, but we can dup() it */
    }
    else
    {
	mode = O_RDONLY;
	if ((fd = open(token.value.strval, mode)) < 0)
	    runtime_error(sort, NSORT_INPUT_OPEN, token.value.strval,
			  strerror(errno));
    }

    if ((IN.n_files % 4) == 0)	/* allocate room every 4 input files */
    {
	IN.files = (fileinfo_t *) realloc(IN.files,
				    (IN.n_files + 4) * sizeof(fileinfo_t));
    }

    infile = &IN.files[IN.n_files];
    memset(infile, 0, sizeof(*infile));
    infile->name = strdup(token.value.strval);
    infile->fd = fd;
    infile->openmode = mode;

    get_file_stat(infile);

    IN.n_files++;

    parse_file_options(sort, infile, &p, errs, InfileSpecs);
    if (OUT.file->io_mode == IOMODE_BUFFERED)
	OUT.file->io_mode = IOMODE_MAPPED;

    ARENACHECK;

    return (p);
}

/*
 * add_temp_file -	add a temporary file to TEMP.files
 *
 *	Returns:
 *		nsort_msg_t, NSORT_NOERROR (0) when successful
 */
nsort_msg_t add_temp_file(sort_t *sort, const char *tempname)
{
    tempfile_t	*workfile;
    int		fd;
    struct stat	stbuf;

    if (stat(tempname, &stbuf) < 0)
	return (NSORT_TEMPFILE_STAT);

    /* allocate room every 16 temp files */
    if ((TEMP.n_files % 16) == 0)
    {
	TEMP.files = (tempfile_t *)
	    realloc(TEMP.files, (TEMP.n_files + 16) * sizeof(tempfile_t));
    }

    workfile = &TEMP.files[TEMP.n_files];
    memset(workfile, 0, sizeof(*workfile));
    workfile->rm_on_close = FALSE;

    if (S_ISDIR(stbuf.st_mode))
    {
	static char temp_suffix[] = "/nsort.XXXXXX";

	workfile->fileinfo.name = (char *) malloc(strlen(tempname) +
						  sizeof(temp_suffix));
	if (workfile->fileinfo.name == NULL)
	    return (NSORT_MALLOC_FAIL);
	strcpy(workfile->fileinfo.name, tempname);
	strcat(workfile->fileinfo.name, temp_suffix);
	mktemp(workfile->fileinfo.name);
	workfile->rm_on_close = TRUE;
    }
    else if (S_ISREG(stbuf.st_mode))
	workfile->fileinfo.name = strdup(tempname);
    else
	return (NSORT_TEMPFILE_BAD_TYPE);

    TEMP.n_files++;

    workfile->fileinfo.openmode = O_RDWR | O_DIRECT;
    fd = open(workfile->fileinfo.name,
	      workfile->fileinfo.openmode | O_CREAT | O_TRUNC,
	      CREATE_PERM);
    if (fd < 0)
	return (NSORT_TEMPFILE_OPEN);

    workfile->fileinfo.fd = fd;

    workfile->fileinfo.stbuf.st_mode = stbuf.st_mode;
    workfile->fileinfo.stbuf.st_dev = stbuf.st_dev;
    workfile->fileinfo.stbuf.st_size = 0;

    return (NSORT_NOERROR);
}

/*
 * another_tempfile -	remember another workfile (two-pass temp results holder)
 *			Each workfile should be a single spindle so that
 *			reading them in the second pass may be optimized.
 *			No two workfiles should be part of a striped device.
 *
 *		filename
 *	the transfersize:N and count:N qualifiers are handled separately
 *
 *	Temp files are always DIRECT, never mapped
 */
const char *another_tempfile(sort_t *sort, const char *p)
{
    nsort_msg_t	err;
    token_t	token;

    PARSE.charno = 0;		/* XXX reset position - ugly */
    PARSE.line = p;
    if (get_token(sort, &p, &token, FileSpecs) != tokFILENAME ||
	token.value.strval[0] == '\0')
    {
	parser_error(sort, &token, NSORT_FILENAME_MISSING);
	return (p);
    }

    if ((err = add_temp_file(sort, token.value.strval)) != NSORT_NOERROR)
	parser_error(sort, &token, err, token.value.strval, strerror(errno));

    return (p);
}

/*
 * assign_outfile
 *
 *		filename[,transfersize:N][,count:N]
 */
const char *assign_outfile(sort_t *sort, const char *p)
{
    int		fd;
    int		mode;
    int		errs = sort->n_errors;
    token_t	token;

    PARSE.charno = 0;		/* XXX reset position - ugly */
    PARSE.line = p;
    if (OUT.file->name)
    {
	runtime_error(sort, NSORT_OUTPUT_ALREADY, OUT.file->name);
	return (p);
    }

    if (get_token(sort, &p, &token, FileSpecs) != tokFILENAME ||
	token.value.strval[0] == '\0')
	parser_error(sort, &token, NSORT_FILENAME_MISSING);

    /* first attempt to open file with direct i/o, unless environment flag
     * says otherwise.
     */
    if (strcmp(token.value.strval, "-") == 0)
    {
	strcpy(token.value.strval, "<standard output>");
	fd = fileno(stdout);
	mode = -1;	/* can't open stdin again, but we can dup() it */

#if 0
	/* We might be appending to a non-empty file, start our results
	 * after any data already in the file
	 */
	if ((OUT.write_offset = lseek(fd, 0, SEEK_CUR)) == -1)
	    OUT.write_offset = 0;	/* a pipe or fifo, seek innapplicable */
#else
	/* Damn! Can't append to stdout if we do any seeking! where is the end
	 * of the existing data? lseek(fd, 0, SEEK_CUR) lies (always says zero).
	 * We force a no-seeking mode, since we can't get the real file position
	 */
	OUT.file->io_mode = IOMODE_SERIAL;
#endif
    }
    else
    {
	mode = O_WRONLY | O_CREAT;
	if ((fd = open(token.value.strval, mode, CREATE_PERM)) < 0)
	{
	    runtime_error(sort, NSORT_OUTPUT_OPEN, token.value.strval,
			  strerror(errno));
	    return (p);
	}
    }

    OUT.file->name = strdup(token.value.strval);
    OUT.file->fd = fd;
    OUT.file->openmode = mode;

    get_file_stat(OUT.file);

    parse_file_options(sort, OUT.file, &p, errs, OutfileSpecs);
    if (OUT.file->io_mode == IOMODE_MAPPED)
	OUT.file->io_mode = IOMODE_BUFFERED;

    ARENACHECK;

    return (p);
}

/*
 * another_filesys	- get default i/o size and count info for a filesystem
 */
const char *another_filesys(sort_t *sort, const char *p)
{
    fsinfo_t	*fs;
    fileinfo_t	fi;
    token_t	token;
    int		i;
    int		errs = sort->n_errors;
    struct stat	stbuf;

    PARSE.charno = 0;		/* XXX reset position - ugly */
    PARSE.line = p;
    if (get_token(sort, &p, &token, FileSpecs) != tokFILENAME ||
	token.value.strval[0] == '\0')
    {
	parser_error(sort, &token, NSORT_FILESYS_NAME_MISSING);
	return (p);
    }

    if (stat(token.value.strval, &stbuf) < 0)
    {
	parser_error(sort, &token, NSORT_FILESYS_NOTFOUND, strerror(errno));
	return (p);
    }
    if (S_ISBLK(stbuf.st_mode))
    {
	/* use rdev rather than dev (which will be for the root (/dev) fs
	 * how to find out what the dio parameters are in this case?
	 */
	stbuf.st_dev = stbuf.st_rdev;
	
    }

    /* See if this filesystem has already been specified; if so replace
     * the transfersize, count, mode, and sync params previously assigned.
     */
    for (i = 0, fs = SYS.filesys; i < SYS.n_filesys; i++, fs++)
	if (fs->dev == stbuf.st_dev)
	    break;

    if (i >= SYS.n_filesys)
    {
	if ((SYS.n_filesys % 4) == 0)	/* allocate room every 4 filesystems */
	    SYS.filesys = (fsinfo_t *) realloc(SYS.filesys,
					       (SYS.n_filesys + 4) *
					       sizeof(fileinfo_t));

	fs = &SYS.filesys[SYS.n_filesys];
	memset(fs, 0, sizeof(*fs));
	fs->dev = stbuf.st_dev;
	SYS.n_filesys++;
    }

    /* Fake up a ``file'' to pass to parse_file_options, then copy
     * the relevent fields to the fsinfo_t
     */
    memset(&fi, 0, sizeof(fi));
    fi.openmode = O_DIRECT;		/* assume filesystems can do direct? */
    fi.aio_count = fs->aio_count;
    fi.io_size = fs->io_size;
    fi.stbuf.st_mode = S_IFDIR;
    fi.min_io = 512;		/* XXX get real values for S_ISBLK() */
    fi.max_io = 4 * (1024 * 1024 - 1);	
    parse_file_options(sort, &fi, &p, errs, OutfileSpecs);
    fs->aio_count = fi.aio_count;
    fs->io_size = fi.io_size;
    fs->io_mode = fi.io_mode;
    fs->sync = fi.sync;

    return (p);
}

/*
 * get_filesys_iosize	- set a file's transfer size if a -filesys covers it
 */
int get_filesys_iosize(sort_t *sort, fileinfo_t *file)
{
    int	i;
	
    for (i = 0; i < SYS.n_filesys; i++)
    {
	if (SYS.filesys[i].dev == file->stbuf.st_dev &&
	    SYS.filesys[i].io_size != 0)
	{
	    file->io_size = SYS.filesys[i].io_size;
	    return (TRUE);
	}
    }
    return (FALSE);
}

/*
 * get_filesys_aiocount	- set a file's aiocount if a -filesys covers it
 */
int get_filesys_aiocount(sort_t *sort, fileinfo_t *file)
{
    int	i;
	
    for (i = 0; i < SYS.n_filesys; i++)
    {
	if (SYS.filesys[i].dev == file->stbuf.st_dev &&
	    SYS.filesys[i].aio_count != 0)
	{
	    file->aio_count = SYS.filesys[i].aio_count;
	    return (TRUE);
	}
    }
    return (FALSE);
}

/*
 * get_filesys_iomode	- set a file's iomode if a -filesys covers it
 */
int get_filesys_iomode(sort_t *sort, fileinfo_t *file)
{
    int	i;
	
    for (i = 0; i < SYS.n_filesys; i++)
    {
	if (SYS.filesys[i].dev == file->stbuf.st_dev &&
	    SYS.filesys[i].io_mode != IOMODE_UNSPECIFIED)
	{
	    file->io_mode = SYS.filesys[i].io_mode;
	    return (TRUE);
	}
    }
    return (FALSE);
}

void set_file_flags(fileinfo_t *file, int newflags)
{
    int fdflags;

    fdflags = fcntl(file->fd, F_GETFL);
    if (fdflags == -1)
	die("F_GETFL on \"%s\" failed: %s", file->name, strerror(errno));
    if ((fdflags & newflags) != newflags)
    {
	/* if direct is wanted include it in newflags - typically it isn't */
	fdflags &= ~FDIRECT;
	fdflags |= newflags;
	if (fcntl(file->fd, F_SETFL, fdflags) < 0)
	    die("F_SETFL on \"%s\" failed: %s", file->name, strerror(errno));
    }
}
