/*
 * mkinodetbl - Make a table of inumber <-> pathname mappings for memusage
 * programs (memusage, prgdump, rgaccum, etc...)
 *
 * $Revision: 1.8 $
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

dev_t
easy_stat (char *path)
{
	struct stat statbuf;

	if (stat (path, &statbuf) != 0)
		return -1;
	return statbuf.st_dev;
}


dev_t
get_device (char *path)
{
	struct stat st;
	char buff[BUFSIZ], str[BUFSIZ];
	FILE *fp;
	dev_t i;

	if ((i = easy_stat(path)) > 0)
		return i;

	sprintf (buff, "/etc/devnm %s", path);
	if ((fp = popen (buff, "r")) == NULL) {
		perror ("popen");
		return (-1);
	}
	fscanf (fp, "%s", str);
	fclose (fp);

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

main (int argc, char **argv)
{
	dev_t ndev;
	FILE *fp, *pp;
	long inode;
	char str[BUFSIZ];

#define _TABLE_PATH	"/var/tmp/memusage.inodes"

	fprintf(stderr, "Putting table in %s\n", _TABLE_PATH);

	if ((fp = fopen (_TABLE_PATH, "w")) != NULL) {
		pp = popen("find "
			"/usr/ToolTalk "
			"/usr/bin "
			"/usr/lib "
			"/usr/lib32 "
			"/usr/lib64 "
			"/usr/local "
			"/usr/pcp "
			"/usr/OnRamp "
			"/usr/Cadmin "
			"/usr/CaseVision "
			"/usr/sbin "
			"/usr/bsd "
			"/usr/etc "
			"/lib "
			"/lib32 "
			"/sbin "
			"/bin "
			"/etc "
			"/usr/gfx "
			"/var/ns/lib "
			"-type f "
			"\\( -perm -0100 -o -name \\*so\\* \\) "
			"-print | xargs /sbin/ls -lLid | "
			"sed -e /fonts/d -e /terminfo/d | "
			"awk '{printf \"%d %s\\n\", $1, $10 }'  "
			";"
			"find "
			"/var/ns/cache "
			"-type f "
			"-print | xargs /sbin/ls -lLid | "
			"awk '{printf \"%d %s\\n\", $1, $10 }'  "
			";"
			"echo "
			"/dev/zero "
			"| xargs /sbin/ls -lLid | "
			"awk '{printf \"%d %s\\n\", $1, $11 }'  ",
			"r");
		if (pp != NULL) {
			while (fscanf (pp, "%d %s\n", &inode, str) == 2) {
				ndev = get_device (str);
				fprintf (fp, "%d %d %s\n", ndev, inode, str);
			}
			fclose (pp);
		}
		fclose (fp);
	}
}
