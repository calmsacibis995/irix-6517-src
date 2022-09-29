/*
 * RegionInfo.c++
 *
 *	definition of RegionInfo class for regview
 *
 * Copyright 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.8 $"

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <bstring.h>
#include <libelf.h>
#include <paths.h>

#include <Vk/VkResource.h>
#include <Vk/VkApp.h>
#include <Vk/VkErrorDialog.h>

#include "RegionInfo.h"

/*
 * Name of callback list that we call each time we poll the region
 * that we're monitoring.
 */
const char RegionInfo::updateCB[] = "updateCB";

/*
 * Name of the callback list that we call when bloatview tells us to
 * monitor a different region.
 */
const char RegionInfo::newRegionCB[] = "newRegionCB";

/*
 *  static int
 *  cap_open(const char* file, int flags)
 *
 *  Description:
 *      Capability bracketing for the open system call.  The
 *      capabilities used correspond to the capability check that the
 *      procfs code does.
 *
 *  Parameters:
 *      file   File to open.
 *      flags  Flags for open call.
 *
 *  Returns:
 *	file descriptor, -1 if error.
 */

static int
cap_open(const char* file, int flags)
{
    cap_t ocap;
    const cap_value_t cap[3] = { CAP_DAC_WRITE,
				 CAP_DAC_READ_SEARCH,
				 CAP_FOWNER };
    int r;

    ocap = cap_acquire(sizeof cap/sizeof cap[0], cap);
    r = open(file, flags);
    cap_surrender(ocap);

    return r;
}

/*
 *  RegionInfo::RegionInfo()
 *
 *  Description:
 *      Constructor for RegionInfo class.  Initialize some data
 *      structures and start monitoring stdin.  stdin is a pipe that
 *      bloatview writes stuff into when it wants us to monitor a
 *      region.
 */

RegionInfo::RegionInfo()
{
    detached = False;
    pageSize = getpagesize();
    pageData = NULL;
    timer = NULL;
    setbuf(stdin, NULL);
    inputId = XtAppAddInput(theApplication->appContext(), 0,
			    (XtPointer)XtInputReadMask, readInputCB, this);

    char *str = VkGetResource("interval", "Interval");
    interval = str ? atoi(str) : 500;
}

/*
 * RegionInfo destructor
 */
RegionInfo::~RegionInfo()
{
}

/*
 *  void RegionInfo::readInput()
 *
 *  Description:
 *      Read a line of input from stdin.  What we get is a command
 *      from bloatview telling is the process id (in decimal) and the
 *      virtual address (in hex) of the region that we're supposed to
 *      monitor now. 
 */

void RegionInfo::readInput()
{
    char line[PATH_MAX + 50];

    if (fgets(line, sizeof line, stdin) == NULL) {
	/*
	 * We get NULL if bloatview exited.  Go ahead and keep
	 * displaying stuff (maybe we should exit here...)
	 */
	return;
    }
    
    pid_t newPid;
    caddr_t newVaddr;
    if (sscanf(line, "%d %x", &newPid, &newVaddr) != 2) {
	(void)fprintf(stderr, "Error parsing gmemusage input!\n");
	return;
    }
    
    pid = newPid;
    vaddr = newVaddr;

    /*
     * Remove the previous timer because update is going to add
     * another one.
     */
    if (timer) {
	XtRemoveTimeOut(timer);
	timer = NULL;
    }

    if (pageData) {
	free(pageData);
	pageData = NULL;
    }

    if (update() == 0) {
	setObjName();
	setOffset();
	callCallbacks(newRegionCB, NULL);
	callCallbacks(updateCB, NULL);
    }
}

/*
 *  int RegionInfo::update()
 *
 *  Description:
 *      Find out stuff about a region using /proc.
 *
 *  Returns:
 *	0 if successful, -1 if error.
 */

int RegionInfo::update()
{
    char proc[PATH_MAX];
    (void)sprintf(proc, "/proc/%05d", pid);

    int fd = cap_open(proc, O_RDONLY);

    if (fd == -1) {
	char *msg;
	char buf[PATH_MAX + 80];
	switch (oserror()) {
	case ENOENT:
	    msg = "processExited";
	    break;
	case EPERM:
	    msg = "permDenied";
	    break;
	default:
	    sprintf(buf, "%s: %s", proc, strerror(oserror()));
	    msg = buf;
	}
	theErrorDialog->postModal(msg);
	return -1;
    }
    
    /*
     * Get process info
     */
    if (ioctl(fd, PIOCPSINFO, &psinfo) == -1) {
	perror(proc);
	(void)close(fd);
	return -1;
    }
    
    int nmaps = 0;
    if (ioctl(fd, PIOCNMAP, &nmaps) == -1) {
	perror(proc);
	(void)close(fd);
	return -1;
    }

    if (!nmaps) {
	(void)close(fd);
	return 0;
    }	

    prmap_sgi_t *maps = new prmap_sgi[nmaps];
    prmap_sgi_arg_t maparg;
    maparg.pr_vaddr = (caddr_t)maps;
    maparg.pr_size = sizeof(prmap_sgi_t) * nmaps;

    /*
     * Find the map that corresponds to the region we want.
     */
    if ((nmaps = ioctl(fd, PIOCMAP_SGI, &maparg)) == -1) {
	perror(proc);
	(void)close(fd);
	delete [] maps;
	return -1;
    }

    for (int i = 0; i < nmaps; i++) {
	if (maps[i].pr_vaddr == vaddr) {
	    map = maps[i];
	    break;
	}
    }
    
    delete [] maps;

    /*
     * Get the page data for the region we want.  This is where the
     * info for the bars comes from.
     */
    int npages = (int)(map.pr_size / pageSize);
    size_t size = sizeof *pageData + sizeof(pgd_t) * (npages - 1);

    if (!pageData) {
	pageData = (prpgd_sgi *)malloc(size);
	bzero(pageData, size);
    } else if (pageData->pr_pglen < npages) {
	pageData = (prpgd_sgi *)realloc(pageData, size);
	bzero(pageData->pr_data + pageData->pr_pglen,
	      (npages - pageData->pr_pglen) * sizeof(pgd_t));
    }
	
    pageData->pr_vaddr = vaddr;
    pageData->pr_pglen = npages;

    if (ioctl(fd, PIOCPGD_SGI, pageData) == -1) {
	perror(proc);
	(void)close(fd);
	return -1;
    }
    
    (void)close(fd);

    for (i = 0; i < pageData->pr_pglen; i++) {
	if ((pageData->pr_data[i].pr_flags & PGF_CLEAR)
	    && (pageData->pr_data[i].pr_flags & (PGF_FAULT|PGF_ISVALID))) {
	    pageData->pr_data[i].pr_flags |= PGF_USRHISTORY;
	} else if ((pageData->pr_data[i].pr_flags & PGF_REFERENCED)
		   && (pageData->pr_data[i].pr_flags & PGF_USRHISTORY)) {
	    pageData->pr_data[i].pr_flags &= ~PGF_USRHISTORY;
	}
    }

    /*
     * Add a timer so that we get called again after a while.
     */
    timer = XtAppAddTimeOut(theApplication->appContext(), interval,
			    updateTimerProc, this);
    return 0;
}

/*
 *  void RegionInfo::setOffset()
 *
 *  Description:
 *      Set the offset from where this region was supposed to go to
 *      where rld actually put it.  This is for reconciling nm output
 *      in SymTab with objects that had to be moved because of
 *      quickstart failure.
 */

void RegionInfo::setOffset()
{
    char buf[PATH_MAX];
    int fd, elfFd;

    offset = 0;

    (void)sprintf(buf, "/proc/%05d", pid);

    if ((fd = open(buf, O_RDONLY)) == -1) {
	return;
    }

    elfFd = ioctl(fd, PIOCOPENM, &map.pr_vaddr);
    (void)close(fd);

    if (elfFd == -1) {
	return;
    }

    (void)lseek(elfFd, 0, SEEK_SET);

    elf_version(EV_CURRENT);
    Elf *elf = elf_begin(elfFd, ELF_C_READ, NULL);

    if (elf) {
	Elf32_Ehdr *ehdr = elf32_getehdr(elf);
	Elf32_Phdr *phdr = elf32_getphdr(elf);

	if (phdr && ehdr) {
	    for (int i = 0; i < ehdr->e_phnum; i++) {
		if (phdr[i].p_offset == map.pr_off) {
		    offset = (char *)map.pr_vaddr - (char *)phdr[i].p_vaddr;
		    break;
		}
	    }
	}
	elf_end(elf);
    }

    (void)close(elfFd);
}

/*
 *  void RegionInfo::detach()
 *
 *  Description:
 *      Sever our connection with bloatview.  If the user tries to
 *      view another region after this, bloatview will launch another
 *      Region Viewer.  We continue monitoring our region, though.
 */

void RegionInfo::detach()
{
    if (!detached) {
	detached = True;
	XtRemoveInput(inputId);
	(void)close(0);
	(void)open("/dev/null", O_RDONLY);
    }
}

/*
 *  void RegionInfo::zap()
 *
 *  Description:
 *      Clear the referenced bits for this region, so the user can see
 *      what pages get touched during test operations.
 */

void RegionInfo::zap()
{
    if (pageData) {
	for (int i = 0; i < pageData->pr_pglen; i++) {
	    pageData->pr_data[i].pr_flags |= PGF_CLEAR;
	}

	update();

	for (i = 0; i < pageData->pr_pglen; i++) {
	    pageData->pr_data[i].pr_flags &= ~PGF_CLEAR;
	}
    }
}

/*
 *  RegionInfo::EPageState RegionInfo::getPageState(pgd_t *pgd)
 *
 *  Description:
 *      Determine the state of a page based on its page data.
 *
 *  Parameters:
 *      pgd  page data of page to determine state of.
 *
 *  Returns:
 *	kResident, kZapped, kInvalid, or kPagedOut.
 */

RegionInfo::EPageState RegionInfo::getPageState(pgd_t *pgd)
{
    if (pgd->pr_flags & PGF_USRHISTORY) {
	return kZapped;
    } else if (pgd->pr_flags & (PGF_REFERENCED|PGF_ISVALID)) {
	return kResident;
    } else if  (!(pgd->pr_flags & PGF_REFHISTORY)) {
	return kInvalid;
    } else {
	return kPagedOut;
    }
}

/*
 *  void RegionInfo::setObjName()
 *
 *  Description:
 *      Look through bloatview's inode database to try to find the
 *      object that corresponds to the region we're monitoring.  The
 *      SymTab stuff needs this in order to get symbols for a region.
 */

void RegionInfo::setObjName()
{
    /*
     * First look at the command line.  If arg[0] exists and matches
     * our region, use it.  This should catch commands launched via
     * full pathnames.
     */
    int foundIt = 0;
    char *args = strdup(psinfo.pr_psargs);
    char *space = strchr(args, ' ');
    if (space != NULL) {
	*space = '\0';
    }

    struct stat st;
    if (stat(args, &st) == 0 &&
	st.st_dev == map.pr_dev && st.st_ino == map.pr_ino) {
	strncpy(objName, args, sizeof objName);
	objName[sizeof objName - 1] = '\0';
	foundIt = 1;
    }
    free(args);

    if (foundIt) {
	return;
    }
	
    /*
     * Look in $PATH for the executable corresponding to this process.
     */
    char *path = getenv("PATH");
    if (path != NULL && findObjInPath(path)) {
	return;
    }

    /*
     * Look in default root path.
     */
    if (findObjInPath(_PATH_ROOTPATH)) {
	return;
    }
	
    /*
     * Either we couldn't find the executable, or this region comes
     * from some other object.  Look in gmemusage's database.
     */
    char *home = getenv("HOME");
    if (home) {
	char inodeMap[PATH_MAX];
	(void)sprintf(inodeMap, "%s/%s", home, ".gmemusage.inodes");
	FILE *fp = fopen(inodeMap, "r");
	if (fp) {
	    char stdioBuf[BUFSIZ];
	    (void)setbuffer(fp, stdioBuf, sizeof stdioBuf);
	    char buf[sizeof objName];
	    // Use fgets/sscanf rather than fscanf to avoid buffer
	    // overruns.
	    while (fgets(buf, sizeof buf, fp) != NULL) {
		dev_t rdev;
		ino_t inode;
		if (sscanf(buf, "%d %lld %s\n", &rdev, &inode,
			   objName) == 3
		    && rdev == map.pr_dev && inode == map.pr_ino) {
		    foundIt = 1;
		    break;
		}
	    }
	    (void)fclose(fp);
	}
    }

    if (!foundIt) {
	objName[0] = '\0';
    }
}

/*
 *  bool RegionInfo::findObjInPath(const char *searchPath)
 *
 *  Description:
 *      See if we can find the object for our region in "searchPath".
 *
 *  Parameters:
 *      searchPath  colon separated list of directories in which to
 *                  look for our object.
 *
 *  Returns:
 *	true if found, false otherwise.
 */

bool RegionInfo::findObjInPath(const char *searchPath)
{
    bool foundIt = false;
    char *path = strdup(searchPath);
    char *lasts = NULL;
    char *dir;

    for (dir = strtok_r(path, ":", &lasts); dir != NULL;
	 dir = strtok_r(NULL, ":", &lasts)) {
	char file[PATH_MAX];
	struct stat st;
	snprintf(file, sizeof file, "%s/%s", dir, psinfo.pr_fname);
	if (stat(file, &st) == 0) {
	    if (st.st_dev == map.pr_dev && st.st_ino == map.pr_ino) {
		foundIt = true;
		strncpy(objName, file, sizeof objName);
		objName[sizeof objName - 1] = '\0';
		break;
	    }
	}
    }

    free(path);

    return foundIt;
}

/*
 * Xt callback function for the XtAppAddInput for stdin (from which we
 * get bloatview commands).
 */
void RegionInfo::readInputCB(XtPointer client, int *, XtInputId *)
{
    /*
     * This static variable is necessary because Vk has message loops
     * embedded in it, which can lead to this function getting called
     * recursively which is not what we want.
     */
    static int alreadyHere = False;
    if (!alreadyHere) {
	alreadyHere = True;
	((RegionInfo *)client)->readInput();
	alreadyHere = False;
    }
}

/*
 * Xt callback function for the XtAppAddTimer for polling the region
 * that we're monitoring.
 */
void RegionInfo::updateTimerProc(XtPointer client, XtIntervalId *)
{
    static int alreadyHere = False;
    if (!alreadyHere) {
	alreadyHere = True;
	if (((RegionInfo *)client)->update() == 0) {
	    ((RegionInfo *)client)->callCallbacks(updateCB, NULL);
	}
	alreadyHere = False;
    }
}
