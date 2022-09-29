/*
 * Stuff for translating (dev,inode) pairs to file names
 *
 * Stolen from memusage programs
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "process.h"
#include "draw.h"

#define INODEFILE ".gmemusage.inodes"
#define INODEFILETMP ".gmemusage.inodes.tmp"
#define DEFBLOATPATH \
    "/usr/lib:/usr/lib32:/usr/lib64:"\
    "/usr/local:/usr/sysadm:/var/ns/lib:"\
    "/lib:/lib32:/lib64:"\
    "/usr/gfx "
#define PATHSEP ";,: "
#define FINDCMD "/bin/find "
#define FINDARGS "-type f \\( -perm -0100 -o -name \\*so\\* \\) -follow -print "
#define FINDMUNGE " | /bin/xargs /sbin/ls -lLid | " \
    "/bin/sed -e /fonts/d -e /terminfo/d | " \
    "/bin/awk '{printf \"%d %s\\n\", $1, $10 }'  "\
    ";"\
    "/bin/echo "\
    "/dev/zero "\
    "| /bin/xargs /sbin/ls -lLid | "\
    "/bin/awk '{printf \"%d %s\\n\", $1, $11 }'"
    

struct inrec {
	struct inrec *next;
	long inode;
	dev_t rdev;
	char *name;
};

static struct inrec *inodes;
static char *inodePath = NULL;
static char *inodePathTmp = NULL;

static void
MakePaths(void)
{
    char *home;
    int length;

    if (!inodePath) {
	home = getenv("HOME");
	if (!home) {
	    home = "/var/tmp";
	}
	/*
	 * 2 == 1 ('/') + 1 ('\0')
	 */
	length = strlen(home) + strlen(INODEFILE) + 2;
	inodePath = malloc(length);
	snprintf(inodePath, length, "%s/%s", home, INODEFILE);
	length = strlen(home) + strlen(INODEFILETMP) + 2;
	inodePathTmp = malloc(length);
	snprintf(inodePathTmp, length, "%s/%s", home, INODEFILETMP);
    }
}

/*
 *  static char *
 *  basename(char *path)
 *
 *  Description:
 *      Get the base file name of a path.  Local so I don't have to
 *      suck in libgen just for this one trivial fuction.
 *
 *  Parameters:
 *      path
 *
 *  Returns:
 *      Pointer to basename of path (not duplicated!)
 */

static char *
basename(char *path)
{
    char *slash;

    slash = strrchr(path, '/');

    return slash ? slash + 1 : path;
}

static dev_t
easy_stat (char *path)
{
	struct stat statbuf;
	if (stat (path, &statbuf) != 0)
		return -1;
	return statbuf.st_dev;
}

static dev_t
get_device (char *path)
{
	struct stat	st;
	char buff[MAXPATHLEN + 12], str[MAXPATHLEN], buf[256];
	FILE *fp;

	dev_t i;
	if ((i = easy_stat(path)) > 0)
		return i;

	snprintf (buff, sizeof buff, "/etc/devnm %s", path);
	
	if ((fp = popen(buff, "r")) == NULL) {
	    perror("popen");
	    return -1;
	}
	setbuffer(fp, buf, sizeof buf);

	fscanf (fp, "%s", str);
	pclose (fp);

	if (strcmp (str, "devnm:") == 0) {
		return (-1);
	}
	if (strncmp (str, "/dev/", 5) != 0 &&
	    strchr (str, ':') == NULL) {
		strcpy (buff, "/dev/");
	} else {
		strcpy (buff, "");
	}
	strcat (buff, str);

	if (lstat (buff, &st) == 0 &&
	    (st.st_mode & S_IFMT) == S_IFBLK) {
		return (st.st_rdev);
	} else {
		perror ("get_device: lstat");
		return (-1);
	}
}

/*
 *  static int
 *  BuildInodeTable(void)
 *
 *  Description:
 *      Find a whole bunch of files, and get their inode numbers
 *
 *  Returns:
 *      0 if successful, -1 if error
 */

static int
BuildInodeTable(void)
{
    dev_t ndev;
    FILE *fp, *inodefp;
    long inode;
    char str[256], *findCmd, *bloatPath, *dir;
    char buf1[BUFSIZ], buf2[BUFSIZ];
    int errfd;

    /*
     * use bloatPath as the path to look for executables
     */
    bloatPath = getenv("GMEMUSAGEPATH");

    if (!bloatPath) {
	bloatPath = DEFBLOATPATH;
    }

    /*
     * Dup it because strtok is going to modify it
     */
    bloatPath = strdup(bloatPath);

    findCmd = malloc(strlen(bloatPath) + strlen(FINDCMD)
		     + strlen(FINDARGS) + strlen(FINDMUNGE) + 1);

    strcpy(findCmd, FINDCMD);

    for (dir = strtok(bloatPath, PATHSEP); dir;
	 dir = strtok(NULL, PATHSEP)) {
	strcat(findCmd, dir);
	strcat(findCmd, " ");
    }

    strcat(findCmd, FINDARGS);
    strcat(findCmd, FINDMUNGE);

    /*
     * Save our stderr for after the find command, and redirect stderr
     * to /dev/null while running find.  This is so that the user
     * doesn't see find's error messages when it can't find things in
     * our list.
     */
    errfd = dup(2);
    close(2);
    (void)open("/dev/null", O_WRONLY);

    if ((fp = popen(findCmd, "r")) == NULL) {
	dup2(errfd, 2);
	close(errfd);
	perror("popen");
	return -1;
    }
    setbuffer(fp, buf1, sizeof buf1);

    if ((inodefp = fopen(inodePathTmp, "w")) == NULL) {
	pclose(fp);
	dup2(errfd, 2);
	close(errfd);
	perror("fopen");
	return -1;
    }
    setbuffer(fp, buf2, sizeof buf2);
    
    while (fscanf (fp, "%d %s\n", &inode, str) == 2) {
	ndev = get_device (str);
	fprintf (inodefp, "%d %d %s\n", ndev, inode, str);
    }

    pclose (fp);
    fclose(inodefp);

    /*
     * Restore stderr
     */
    dup2(errfd, 2);
    close(errfd);

    /*
     * We don't create the actual database unless all has gone well.
     */
    (void)unlink(inodePath);
    if (link(inodePathTmp, inodePath) == -1) {
	perror("link");
    }
    if (unlink(inodePathTmp) == -1) {
	perror("unlink");
    }

    return 0;
}

/*
 *  static void
 *  add_inode(long inode, dev_t rdev, char *str)
 *
 *  Description:
 *      Add an inode to the internal list
 *
 *  Parameters:
 *      inode  inode to add
 *      rdev   dev to add
 *      str    name to add
 */

static void
add_inode(long inode, dev_t rdev, char *str)
{
	struct inrec *new;

	new = (struct inrec *)malloc(sizeof (struct inrec));
	new->next = inodes;
	new->inode = inode;
	new->rdev = rdev;
	new->name = strdup(str);
	/*
	 * Be careful!  InodeLookup relies on the fact that the new
	 * inode goes at the head of the list; if you change this,
	 * make sure to fix InodeLookup.
	 */
	inodes = new;
}

/*
 * Add dynamic paths into dev/ino map list
 */
static void
prdynpaths(void)
{
	static char *paths[] = { "/var/tmp/.Xshmtrans0",
				 "/tmp/.cadminOSSharedArena",
				 NULL };
	register int i;
	struct stat statd;

	/* Add elements to the list */
	for (i = 0; paths[i] != NULL; ++i) {
		if (stat(paths[i], &statd) < 0)
			continue;
		add_inode (statd.st_dev, statd.st_ino, paths[i]);
	}
}

/*
 *  int
 *  InodeInit(void)
 *
 *  Description:
 *      Initialize inode table.  If it exists and is newer than /unix,
 *      read it in.  Otherwise, create ti.
 *
 *  Returns:
 *      0 if successful, -1 if error
 */

int
InodeInit(void)
{
    struct stat stunix, stinodes;
    char str[MAXPATHLEN], buffer[BUFSIZ];
    int rdev, inode;
    FILE *fp;

    MakePaths();

    if (stat("/unix", &stunix) == -1) {
	perror("/unix");
	return -1;
    }

    if (stat(inodePath, &stinodes) == -1 ||
	stunix.st_mtime > stinodes.st_mtime) {
	WaitMessage("Building inode database.  This will take a while,",
		    "but only has to be done once.");
	if (BuildInodeTable() == -1) {
	    return -1;
	}
    }

    if ((fp = fopen(inodePath, "r")) == NULL) {
	perror("fopen");
	return -1;
    }
    setbuffer(fp, buffer, sizeof buffer);

    while (fscanf(fp,"%d %d %s\n",&rdev,&inode,str) == 3) {
	add_inode (inode, rdev, basename(str));
    }

    fclose(fp);
    prdynpaths();
    return 0;
}

/*
 * Given a vnode at address vloc, find a corresponding path name
 */
char *
InodeLookup(int rdev, int inode)
{
    struct inrec *current;
    static char buf[100];
    
    for (current = inodes; current; current = current->next)
	if (current->inode == inode && current->rdev == rdev)
	    return current->name;

    snprintf(buf, sizeof buf, "#%d", inode);
    add_inode(inode, rdev, buf);
    return inodes->name;	/* rely on side effect of add_indode */
}

/*
 *  int
 *  FindInode(char *str, dev_t *rdev, ino_t *ino)
 *
 *  Description:
 *      Given a string, find the inode and device number
 *
 *  Parameters:
 *      str   string to find device and inode of
 *      rdev  gets device
 *      ino   gets inode
 *
 *  Returns:
 *      0 if successful, -1 if error
 */

int
FindInode(char *str, dev_t *rdev, ino_t *ino)
{
	struct inrec *current;

	for (current = inodes; current; current = current->next)
		if (strcmp(current->name, str) == NULL) {
			if (rdev) *rdev = current->rdev;
			if (ino) *ino = current->inode;
			return 0;
		}
	return -1;
}

void
InvalidateInodeTable(void)
{
    MakePaths();
    (void)unlink(inodePath);
    (void)unlink(inodePathTmp);
}
