
/* This file was ported straightly from fx */

#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dvh.h>
#include <ustat.h>
#include <sys/lvtab.h>
#include <diskinfo.h>
#include <string.h>


/* Check to see if the drive being opened has mounted filesystems.
 * Done even if not in expert mode, since you can re-partition
 * the drive even in 'naive' mode. 
 * Check to see if we are part of a mounted logical volume also.
 * Unfortunately, we have to rely on /etc/lvtab, and it might
 * be /root/etc/lvtab, etc...  Shares an assumption with libdisk
 * and the gopen code that all disk names are CCC#d#xxx
*/
chkmounts(char *special)
{
	struct stat sb, sb2;
	struct ustat ustatarea;
	unsigned token;
	int ret = 0;
	struct ptinfo *pt;
	FILE *lvf;
	char *rels, *reldev;
	struct lvtabent	*lvp;

	if(stat(special, &sb) == -1)
		return 0;	/* shouldn't happen */

	if((token=setdiskinfo(special, "/etc/fstab", 0)) == 0)
		return 0;
	while(pt = getpartition(token)) {
		if(stat(partname(special, pt->ptnum), &sb2) == -1)
			continue;
		if(ustat(sb2.st_rdev,&ustatarea) == 0) {
			ret = 1;
			break;
		}
	}
	enddiskinfo(token);
	if(ret)
		return ret;
	
	if(!(lvf=fopen("/etc/lvtab", "r"))
		&& !(lvf=fopen("/root/etc/lvtab", "r")))
		return 0;

	if(!(rels = strrchr(special, '/')))
		rels = special;	/* shouldn't happen */
	else
		rels++;

	while(!ret && (lvp = getlvent(lvf))) {
		int i;
		if(stat(lvp->devname, &sb2) == -1) {
			char lvname[MAXPATHLEN+1];
			strcpy(lvname, "/dev/");
			strncat(lvname,lvp->devname, sizeof(lvname)-6);
			if(stat(lvname, &sb2) == -1) {
				strcpy(lvname, "/dev/rdsk/");
				strncat(lvname,lvp->devname, sizeof(lvname)-11);
				if(stat(lvname, &sb2) == -1) {
					freelvent(lvp);
					continue;
				}
			}
		}
		for(i=0; i<lvp->ndevs;i++) {
			if(!(reldev = strrchr(lvp->pathnames[i], '/')))
				reldev = lvp->pathnames[i];	/* shouldn't happen */
			else
				reldev++;
			/* ignore partition part when comparing */
			if(!strncmp(rels, reldev, 6) &&
				ustat(sb2.st_rdev,&ustatarea) == 0) {
				ret = 1;
				break;
			}
		}
		freelvent(lvp);
	}
	fclose(lvf);
	return ret;
}
