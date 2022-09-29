/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.5 $"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <devmgmt.h>
#include "libadm.h"


#define LABELSIZ	6
#define BELL	"\007"

#define FORMFS_MSG ",\\n\\ \\ or [f] to format %s and place a filesystem on it"
#define FORMAT_MSG ",\\n\\ \\ or [f] to format the %s"
#define WLABEL_MSG ",\\n\\ \\ or [w] to write a new label on the %s"
#define OLABEL_MSG ",\\n\\ \\ or [o] to use the current label anyway"
#define QUIT_MSG   ",\\n\\ \\ or [q] to quit"

#define ERR_ACCESS	"\n%s (%s) cannot be accessed.\n"
#define ERR_FMT		"\nAttempt to format %s failed.\n"
#define ERR_MKFS	"\nAttempt to place filesystem on %s failed.\n"


static void	elabel(void),
		labelerr(char *, char *),
		doformat(char *, char *, char *, char *);
static int	wilabel(char *),
		ckilabel(char *, int),
		insert(char *, char *, int, char *);
static char	*cdevice; 	/* character device name */
static char	*pname; 	/* device presentation name */
static char	*volume; 	/* volume name */
static char origfsname[LABELSIZ+1];
static char origvolname[LABELSIZ+1];

/* Return:
 *	0 - okay, label matches
 *	1 - device not accessable
 *	2 - unknown device (devattr failed)
 *	3 - user selected quit
 *	4 - label does not match
 */

/* macros from labelit to behave correctly for tape
   is a kludge, should use devmgmt
*/
#ifdef RT
#define IFTAPE(s) (!strncmp(s,"/dev/mt",7)||!strncmp(s,"mt",2))
#define TAPENAMES "'/dev/mt'"
#else
#define IFTAPE(s) (!strncmp(s,"/dev/rmt",8)||!strncmp(s,"rmt",3)||!strncmp(s,"/dev/rtp",8)||!strncmp(s,"rtp",3))
#define TAPENAMES "'/dev/rmt' or '/dev/rtp'"
#endif

int
getvol(char *device, char *label, int options, char *prompt)
{
	return _getvol(device, label, options, prompt, (char *)0);
}

int
_getvol(char *device, char *label, int options, char *prompt, char *norewind)
{
	FILE	*tmp;
	char	*advice, *pt;
	int	n, override;

	cdevice = devattr(device, "cdevice");
	if((cdevice == NULL) || !cdevice[0]) {
		cdevice = devattr(device, "pathname");
		if((cdevice == NULL) || !cdevice)
			return(2);	/* bad device */
	}

	pname = devattr(device, "desc");
	if(pname == NULL) {
		pname = devattr(device, "alias");
		if(!pname)
			pname = device;
	}

	volume = devattr(device, "volume");

	if(label) {
		(void) strncpy(origfsname, label, LABELSIZ);
		origfsname[LABELSIZ] = '\0';
		if(pt = strchr(origfsname, ',')) {
			*pt = '\0';
		}
		if(pt = strchr(label, ',')) {
			(void) strncpy(origvolname, pt+1, LABELSIZ);
			origvolname[LABELSIZ] = '\0';
		} else
			origvolname[0] = '\0';
	}
	
	override = 0;
	for(;;) {
		if(!(options & DM_BATCH) && volume) {
			n = insert(device, label, options, prompt);
			if(n < 0)
				override++;
			else if(n)
				return(n);	/* input function failed */
		}

		if((tmp = fopen(norewind ? norewind : cdevice, "r")) == NULL) {
			/* device was not accessible */
			if(options & DM_BATCH)
				return(1);
			(void) fprintf(stderr, ERR_ACCESS, pname, cdevice);
			if((options & DM_BATCH) || (volume == NULL))
				return(1);
			/* display advice on how to ready device */
			if(advice = devattr(device, "advice"))
				(void) puttext(stderr, advice, 0, 0);
			continue;
		}
		(void) fclose(tmp);

		/* check label on device */
		if(label) {
			if(options & DM_ELABEL)
				elabel();
			else {
				/* check internal label using /etc/labelit */
				if(ckilabel(label, override)) {
					if ((options & DM_BATCH) || volume == NULL)
						return(4);
					continue;
				}
			}
		}
		break;
	}
	return(0);
}

static int
ckilabel(char *label, int flag)
{
	FILE	*pp;
	char	*pt, *look, buffer[512];
	char	fsname[LABELSIZ+1], volname[LABELSIZ+1];
	char	*pvolname, *pfsname;
	int	n, c;

	(void) strncpy(fsname, label, LABELSIZ);
	fsname[LABELSIZ] = '\0';
	if(pt = strchr(fsname, ',')) {
		*pt = '\0';
	}
	if(pt = strchr(label, ',')) {
		(void) strncpy(volname, pt+1, LABELSIZ);
		volname[LABELSIZ] = '\0';
	} else
		volname[0] = '\0';
	
	(void) sprintf(buffer, "/etc/labelit %s", cdevice);
	pp = popen(buffer, "r");
	pt = buffer;
	while((c = getc(pp)) != EOF)
		*pt++ = (char) c;
	*pt = '\0';
	(void) pclose(pp);

	pt = buffer;
	pfsname = pvolname = NULL;
	look = "Current fsname: ";
	n = strlen(look);
	while(*pt) {
		if(!strncmp(pt, look, n)) {
			*pt = '\0';
			pt += strlen(look);
			if(pfsname == NULL) {
				pfsname = pt;
				look = ", Current volname: ";
				n = strlen(look);
			} else if(pvolname == NULL) {
				pvolname = pt;
				look = ", Blocks: ";
				n = strlen(look);
			} else
				break;
		} else
			pt++;
	}

	if(strcmp(fsname, pfsname) || strcmp(volname, pvolname)) {
		/* mismatched label */
		if(flag) {
			(void) sprintf(label, "%s,%s", pfsname, pvolname);
		} else {
			labelerr(pfsname, pvolname);
			return(1);
		}
	}
	return(0);
}

static int
wilabel(char *label)
{
	char	buffer[512];
	char	fsname[LABELSIZ+1];
	char	volname[LABELSIZ+1];
	int	n;

	if(!label || !strlen(origfsname)) {
		if(n = ckstr(fsname, NULL, LABELSIZ, NULL, NULL, NULL, 
				"Enter text for fsname label:"))
			return(n);
	} else
		strcpy(fsname, origfsname);
	if(!label || !strlen(origvolname)) {
		if(n = ckstr(volname, NULL, LABELSIZ, NULL, NULL, NULL, 
				"Enter text for volume label:"))
			return(n);
	} else
		strcpy(volname, origvolname);

	if(IFTAPE(cdevice)) {
		(void) sprintf(buffer, "/etc/labelit %s \"%s\" \"%s\" -n 1>&2", 
			cdevice, fsname, volname);
	} else {
		(void) sprintf(buffer, "/etc/labelit %s \"%s\" \"%s\" 1>&2", 
			cdevice, fsname, volname);
	}
	if(system(buffer)) {
		(void) fprintf(stderr, "\nWrite of label to %s failed.", pname);
		return(1);
	}
	if(label)
		(void) sprintf(label, "%s,%s", fsname, volname);
	return(0);
}

static void
elabel(void)
{
}

static int
insert(char *device, char *label, int options, char *prompt)
{
	int	n;
	char	strval[16], prmpt[512];
	char	*pt, *keyword[5], *fmtcmd, *mkfscmd, *voltxt;

	voltxt = (volume ? volume : "volume");
	if(prompt) {
		(void) strcpy(prmpt, prompt);
		for(pt=prmpt; *prompt; ) {
			if((*prompt == '\\') && (prompt[1] == '%'))
				prompt++;
			else if(*prompt == '%') {
				switch(prompt[1]) {
				  case 'v':
					strcpy(pt, voltxt);
					break;

				  case 'p':
					(void) strcpy(pt, pname);
					break;

				  default:
					*pt = '\0';
					break;
				}
				pt = pt + strlen(pt);
				prompt += 2;
				continue;
			}
			*pt++ = *prompt++;
		}
		*pt = '\0';
	} else {
		(void) sprintf(prmpt, "Insert a %s into %s.", voltxt, pname);
		if(label && (options & DM_ELABEL)) {
			(void) strcat(prmpt, " The following external label ");
			(void) sprintf(prmpt+strlen(prmpt), 
				" should appear on the %s:\\n\\t%s", 
				voltxt, label);
		}
		if(label && !(options & DM_ELABEL)) {
			(void) sprintf(prmpt+strlen(prmpt), 
			"  The %s should be internally labeled as follows:", 
				voltxt);
			(void) sprintf(prmpt+strlen(prmpt), 
				"\\n\\t%s\\n", label);
		}
	}

	pt = prompt = prmpt + strlen(prmpt);

	n = 0;
	pt += sprintf(pt, "\\nType [go] when ready");
	keyword[n++] = "go";

	mkfscmd = NULL;
	if(options & DM_FORMFS) {
		if((fmtcmd = devattr(device, "fmtcmd")) && *fmtcmd &&
		  (mkfscmd = devattr(device, "mkfscmd")) && *mkfscmd) {
			pt += sprintf(pt, FORMFS_MSG, voltxt);
			keyword[n++] = "f";
		}
	} else if(options & DM_FORMAT) {
		if((fmtcmd = devattr(device, "fmtcmd")) && *fmtcmd) {
			pt += sprintf(pt, FORMAT_MSG, voltxt);
			keyword[n++] = "f";
		}
	}
	if(options & DM_WLABEL) {
		pt += sprintf(pt, WLABEL_MSG, voltxt);
		keyword[n++] = "w";
	}
	if(options & DM_OLABEL) {
		pt += sprintf(pt, OLABEL_MSG);
		keyword[n++] = "o";
	}
	keyword[n++] = NULL;
	if(ckquit)
		pt += sprintf(pt, QUIT_MSG);
	*pt++ = ':';
	*pt = '\0';

	pt = prmpt;
	fprintf(stderr, BELL);
	for(;;) {
		if(n = ckkeywd(strval, keyword, NULL, NULL, NULL, pt))
			return(n);

		pt = prompt; /* next prompt is only partial */
		if(!strcmp(strval, "f")) {
			doformat(voltxt, label, fmtcmd, mkfscmd);
			continue;
		} else if(!strcmp(strval, "w")) {
			(void) wilabel(label);
			continue;
		} else if(!strcmp(strval, "o"))
			return(-1);
		break;
	}
	return(0);
}

static void
doformat(char *voltxt, char *label, char *fmtcmd, char *mkfscmd)
{
	char	buffer[512];

	fprintf(stderr, "\t[%s]\n", fmtcmd);
	(void) sprintf(buffer, "(%s) 1>&2", fmtcmd);
	if(system(buffer)) {
		(void) fprintf(stderr, ERR_FMT, voltxt);
		return;
	}
	if(mkfscmd) {
		fprintf(stderr, "\t[%s]\n", mkfscmd);
		(void) sprintf(buffer, "(%s) 1>&2", mkfscmd);
		if(system(buffer)) {
			(void) fprintf(stderr, ERR_MKFS, voltxt);
			return;
		}
	}
	(void) wilabel(label);
}


static void
labelerr(char *fsname, char *volname)
{
	(void) fprintf(stderr, "\nLabel incorrect.\n");
	if(volume)
		(void) fprintf(stderr, 
			"The internal label on the inserted %s is\n", volume);
	else
		(void) fprintf(stderr, "The internal label for %s is", pname);
	(void) fprintf(stderr, "\t%s,%s\n", fsname, volname);
}
