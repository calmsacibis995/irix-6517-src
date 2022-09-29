/* parse.c
 *   ARCS pathname parser.  GetComponent() is the core of the pathname parser,
 * and is an externally defined ARCS routine.  parse_path() is used by Open(),
 * parse a filename, and return the i/o information and filename.
 *
 *   The pathname parsing uses a depth first search to locate a COMPONENT that
 * _first_ matches the desired path.  Paths can be ambiguous.  Paths can
 * often have missing tokens on the beginning, middle or end--the parsing
 * code searches for a match.
 *
 *   The alternative to this is to have strict matching rules like the older
 * (revision 1.18 and before), which forces long pathnames that no one wants
 * to type in (except for some Compaq people).
 *
 *   This parsing technique is ARCS compliant since it takes the full path
 * names, but will also take shorter pathnames, and in some cases the old
 * standalone names.  For instance, valid paths for the volume header on
 * the system disk of a IP20:
 *	scsi(0)disk(1)rdisk()partition(8)		(full ARCS)
 *	disk(1)part(8)					(shortened path)
 *	dksc(0,1,8)					(old style)
 * 
 * SN0
 *
 * Parsing in SN0 is done by the hwgraph routines. For now, a 
 * combination of datastructures is used to do the parsing. 
 * The KLCONFIG structure stores the driver and install routine
 * addresses. cfgnode_t and COMPONENT is also used just to let
 * compiles go through. 
 * The given path is passed to hwgraph_lookup_path which traverses
 * the graph and returns a pointer to a KLCOMPONENT structure and
 * the rest of the path. The KLCOMPONENT will contain the address
 * of the '_strat' routine which is a switch to the driver routines.
 * Paths will be of the form 
 *  	/dev/tty/hubuart
 * 	/dev/disk/disk0
 */

#ident "$Revision: 1.57 $"

#include <ctype.h>
#include <saio.h>
#include <parser.h>		/* for LINESIZE */
#include <arcs/types.h>
#include <arcs/hinv.h>
#include <arcs/folder.h>
#include <arcs/fs.h>
#include <arcs/cfgtree.h>
#include <arcs/io.h>
#include <arcs/eiob.h>
#include <arcs/errno.h>
#include <libsc.h>
#include <libsk.h>

/* #define	DEBUG */

static char *remainder;		/* unparsed data from GetComponent() */

extern int memsetup();

#ifdef SN0
#include <pgdrv.h>
extern prom_dev_info_t *kl_get_dev_info(char *, char **, IOBLOCK *) ;
extern int dksc_map[] ;
extern int sn_dksc_map[] ;
#endif

static struct pseudodev {
	char *name;
	int len;
	int (*setup)(cfgnode_t **, struct eiob *, char *);
} pseudodev[] = {
	"mem",		3,	memsetup,
	"null",		4,	nullsetup,
	0,0,0
};

static struct aliases {
	char *name;
	char *alias;
	int nparams;
} aliases[] = {
#ifndef SN0
	"dksc",		"scsi(%d)disk(%d)part(%d)",	3,
	"cdrom",	"scsi(%d)disk(%d)part(%d)",	3,
	"tpsc",		"scsi(%d)tape(%d)part(%d)",	3,
	"bootp",	"network(%d)tftp()",		1,
#else /* SN0 */
#ifdef SN_PDI
	"dksc",	"/baseio/pci/%d/scsi_ctlr/0/target/%d/lun/0/disk/partition/%d", 3,
	"bootp","/dev/network/0/", 	0,
	"network", "/dev/network/%d/", 1,
	"cdrom","/baseio/pci/%d/scsi_ctlr/0/target/%d/lun/0/cdrom/partition/%d/", 3,
	"tpsc",	"", 			0,
#else
"bootp",   "/xwidget/alias_bridge0/pci/master_ioc3/ef/",         1,
"dksc",		
"/xwidget/alias_bridge%d/pci/alias_ql%d/scsi_ctlr/0/target/%d/lun/0/disk/partition/%d", 3,
"cdrom",		
"/xwidget/alias_bridge%d/pci/alias_ql%d/scsi_ctlr/0/target/%d/lun/0/cdrom/partition/%d", 3,
"tpsc",		
"/xwidget/alias_bridge%d/pci/alias_ql%d/scsi_ctlr/0/target/%d/lun/0/tape", 3,
#endif
#endif /* SN0 */
	0, 0, 0
};

char *
parseargs(char *path, int nargs, __psint_t *val)
{
	char *p=path;
	int i = 0;

	val[0] = val[1] = val[2] = 0;

	while (*++p) {
		if (isdigit(*p))
			p = (char*)atob_ptr(p,&val[i]);
		if (*p == ')')
			return(++p);		/* return filename */
		if ((*p != ',') || (++i >= nargs))
			break;
	}

	return(0);
}

static char *
checkaliases(char *path, int *needfree)
{
	char *filename, *new;
	struct aliases *p;
	__psint_t val[3];
	ulong tlen;

	tlen = (ulong)(new=index(path,'(')) - (ulong)path;
	if (!new || tlen == 0)
		goto nochange;

	for (p=aliases; p->name; p++) {
		if (!strncmp(p->name,path,tlen)) {
			filename = parseargs(path+tlen,p->nparams,val);
			if (!filename) goto nochange;
			new = malloc(strlen(p->alias) + strlen(filename) +
				     p->nparams*10 + 1);
			if (!new) goto nochange;
			sprintf(new,p->alias,val[0],val[1],val[2]);
			strcat(new,filename);
#ifdef DEBUG
			printf("sub: %s for %s\n",new,path);
#endif
			*needfree = 1;
			return(new);
		}
	}

nochange:
	return(path);
}

static char *
next(char *path, char **newpath, ULONG *key, ulong *tlen)
{
	char *end, *next, *numend;
	int lkey;	/* must use local to avoid 32/64 bit problems */

	if ((end=index(path,'(')) == 0)
		goto bad;
	if ((next=index(end,')')) == 0)
		goto bad;

	numend = (char *)atob(end+1,&lkey);
	if (numend != next)
		goto bad;
	*newpath = next+1;
	*tlen = (ulong)end - (ulong)path;
	*key = lkey;
	return(path);
bad:
	*newpath = path;
	*tlen = 0;
	*key = 0;
	return(0);
}

static int
checkpseudo(char *path, struct eiob *eiob, cfgnode_t **cfg)
{
	struct pseudodev *p;
	char *new;
	ulong tlen;

	tlen = (ulong)(new=index(path,'(')) - (ulong)path;
	if (!new || tlen == 0)
		return(0);

	for (p = pseudodev; p->name ; p++) {
		if ((tlen == p->len) && !strncmp(path,p->name,tlen))
			return((*p->setup)(cfg,eiob,path+tlen));
	}
	return(0);
}

static cfgnode_t *
walk(cfgnode_t *r, CONFIGTYPE match, ULONG key, char *path)
{
	extern struct cfgdata ParseData[];
	cfgnode_t *tmp;

	if (!r) return(r);		/* end recursion */

	if ((match == Anonymous) ||
	    ((r->comp.Type == match) && (r->comp.Key == key))) {
		register struct cfgdata *look;
		ulong tlen;
		int found = 0;
		char *nextpath;
		char *newtoken;
		ULONG nextkey;

		/* Find next token */
		if ((newtoken=next(path,&nextpath,&nextkey,&tlen)) != 0) {
			/* Lookup token */
			for (look = ParseData; look->name; look++) {
				if ((tlen >= look->minlen) &&
				    (!strncmp(newtoken,look->name,tlen))) {
					found = 1;
					tmp=walk(r->child,look->type,
						 nextkey,nextpath);
					if (tmp) return(tmp);
				}
			}
		}

		/* If invalid token, return as remainder */
		if (!found) {
			remainder = path;
			return((match == Anonymous) ? 0 : r);
		}
	}

	tmp=walk(r->child,match,key,path);
	if (tmp) return(tmp);
	tmp=walk(r->peer,match,key,path);
	if (tmp) return(tmp);

	return(0);
}

COMPONENT *
GetComponent(char *path)
{
	int needfree = 0;
	COMPONENT *c;

	path = checkaliases(path, &needfree);

	c = (COMPONENT *)walk((cfgnode_t *)GetChild(NULL),Anonymous,0,path);

	if (needfree)
		free(path);

	return(c);
}

int
parse_path(char *path, struct eiob *iob)
{
	cfgnode_t *p, *adap, *ctrl;
	char *newtoken, *rpath;
	int needfree=0, rc;
	ulong tlen;
	ULONG key;

#ifdef	DEBUG
	printf("parse_path: %s\n", path);
#endif

	/* check for pseudo devices */
	p = 0;
	if ((rc=checkpseudo(path,iob,&p)) > 0)
		goto done;			/* error */
	else if (p)
		goto found;			/* found pseudo device */

	/* parse device pathname */
	path = checkaliases(path,&needfree);
	p = walk((cfgnode_t *)GetChild(NULL),Anonymous,0,path);
	if (!p) {
		rc = ENODEV;
		goto done;
	}

	/* Check if found node has a driver */
	if (!p->driver) {
		/*  If trying to open a controller with no driver, divert
		 * the request to the child, which should be a peripheral.
		 */
		while (p && !p->driver && (p->comp.Class == ControllerClass)) {
#ifdef DEBUG
			printf("parse_path: controller -> trying child\n");
#endif
			p = p->child;
		}
		if (!p || !p->driver) {
#ifdef DEBUG
			printf("parse_path: no driver\n");
#endif
			rc = ENODEV;
			goto done;
		}
	}

	/*  Fill in Controller, and Unit. */
	iob->iob.Unit = p->comp.Key;
	ctrl = p->parent;
	if (ctrl) {
		adap = ctrl->parent;
		if (adap && (adap->comp.Type == SCSIAdapter)) {
			iob->iob.Controller = adap->comp.Key;
			iob->iob.Unit = ctrl->comp.Key;
		}
		else {
			iob->iob.Controller = ctrl->comp.Key;
		}
	}

#ifdef DEBUG
	printf("parse_path: ctrl(%d) unit(%d)\n",iob->iob.Controller,
	       iob->iob.Unit);
#endif
	
	/* Check for partition(X) */
	iob->iob.Partition = 0;
	if ((newtoken=next(remainder,&rpath,&key,&tlen)) != 0) {
		if ((tlen >= 4) && !strncmp(newtoken,"partition",tlen)) {
#ifdef DEBUG
			printf("parse_path: partition(%d)\n",key);
#endif
			iob->iob.Partition = key;
			newtoken = next(rpath,&rpath,&key,&tlen);
		}
	}

	/* check for ARCS specified protocols and SGI added filesystems.
	 */
	if (newtoken) {
		int (*func)();

		/*  Network registers tftp.  Convert bootp() tag to valid
		 * protocol tftp().
		 */
		if (strncmp(newtoken,"bootp",5) == 0) {
			newtoken = "tftp";
			tlen = 4;
		}

		if (strncmp(newtoken,"console",7) == 0) {
#ifdef DEBUG
			printf("parse_path: console(%d)\n",key);
#endif
			/* key can only be 0 or 1 */
			if (key == 0)
				iob->iob.Flags |= F_CONS;
			else if (key == 1)
				iob->iob.Flags |= F_ECONS;
			else {
				rc = EINVAL;
				goto done;
			}
			
			newtoken = 0;
		}
		else if (func = fs_match(newtoken,tlen)) {
#ifdef DEBUG
			printf("parse_path: found protocol!\n");
#endif
			if (key) {	/* protocols do not have parameters */
				rc = EINVAL;
				goto done;
			}
			iob->fsstrat = func;
			newtoken = 0;
		}
	}

	/*  If newtoken is still set, it hasn't been used, so it is the
	 * remainder of the path to be used as a filename.
	 */
	if (newtoken)
		rpath = newtoken;

	/* Copy any remaining path to the filename */
	if (*rpath != '\0') {
#ifdef DEBUG
		printf("parse_path: filename is %s\n", rpath);
#endif
		iob->filename = malloc(strlen(rpath)+1);
		if (iob->filename)
			strcpy(iob->filename,rpath);
	}

found:
	iob->dev = (COMPONENT *)p;
	iob->devstrat = p->driver;

	rc = ESUCCESS;
done:
	if (needfree)
		free(path);
	return(rc);
}

#ifdef	DEBUG
void printnode (COMPONENT *c)
{
	cfgnode_t *cfg;

	cfg = (cfgnode_t *) c;

	printf ("printnode\n");
	printf ("    parent=0x%x, peer=0x%x, child=0x%x, driver=0x%x\n",
		cfg->parent, cfg->peer, cfg->child, cfg->driver);
	printf ("    id=%s\n", c->Identifier);
	printf ("    class=%d, type=%d, flags=0x%x, key=%d\n",
		c->Class, c->Type, c->Flags, c->Key);
	printf ("\n");
		
}
#endif

#ifdef SN0
static char *
klcheckaliases(char *path, int *needfree)
{
	char *filename, *new, *tmp, *cons_path;
	struct aliases *p;
	__psint_t val[3];
	ulong tlen;

	tlen = (ulong)(new=index(path,'(')) - (ulong)path;
	if (!new || tlen == 0)
		goto nochange;

	for (p=aliases; p->name; p++) {
		if (!strncmp(p->name,path,tlen)) {
			filename = parseargs(path+tlen,p->nparams,val);
#ifdef SN_PDI
			if (val[0] >= NUM_DKSC_CHAN) goto nochange ;
#endif
			if (!filename) goto nochange;
			new = malloc(128 + strlen(p->alias) + strlen(filename) +
				     p->nparams*10 + 1);
			if (!new) goto nochange;
			/* Some doctoring of paths is needed for SN0 */
			if (!strncmp(path, "dksc", 4) || 
			    !strncmp(path, "tpsc", 4) ||
			    !strncmp(path, "cdrom", 5)) {
#ifdef SN_PDI
			    cons_path = getenv("ConsolePath") ;
			    if (cons_path) {
			    	strcpy(new, cons_path) ;
				tmp = new + strlen(cons_path) ;
			    }
			    else
				tmp = new ;

			    sprintf(tmp,p->alias, (sn_dksc_map[val[0]]), val[1],val[2]);
#else
			    sprintf(new,p->alias, (dksc_map[val[0]] & 0xff),
						 (dksc_map[val[0]] >> 8),
						  val[1],val[2]);
#endif
			}
			else
				sprintf(new,p->alias,val[0],val[1],val[2]);
			strcat(new,filename);
#ifdef DEBUG
			printf("sub: %s for %s\n",new,path);
#endif
			*needfree = 1;
			return(new);
		}
	}

nochange:
	return(path);
}

char *
klmakealias(char *path)
{
        char *p=path, *startp, *endp;
        int i = 0;
	__psint_t	val[3] ;
	__psint_t nargs = 3 ;
	char 	*buf ;

        val[0] = val[1] = 0 ;
	val[2] = 0;

	if (strncmp(path, "scsi", 4))
		return path ;

        while (startp = index(p, '(')) {
		endp = index(startp, ')') ;
		*endp = 0 ;
                if (isdigit(*(startp+1)))
                        p = (char*)atob_ptr((startp+1),&val[i]);
		*endp = ')' ;
                if (++i >= nargs)
                        break;
        }

	buf = malloc(strlen(path)) ;
        if (strstr(path, "cdrom") != NULL)
                sprintf(buf, "cdrom(%d,%d,%d)", val[0], val[1], val[2]) ;
        else if (strstr(path, "tape") != NULL)
                sprintf(buf, "tpsc(%d,%d,%d)", val[0], val[1], val[2]) ;
        else
                sprintf(buf, "dksc(%d,%d,%d)", val[0], val[1], val[2]) ;
	strcat(buf,endp+1) ;

#if 0
printf("path = %s, alias = %s\n", path, buf) ;
#endif

        return(buf);
}

#ifndef SN_PDI

int
kl_parse_path(char *path, struct eiob *iob)
{
	cfgnode_t *p, *adap, *ctrl;
	char *newtoken, *rpath;
	int needfree=0, rc;
	ulong tlen;
	ULONG key;
	klinfo_t *k ;
	prom_dev_info_t *dev_info;

#ifdef	DEBUG
	printf("kl_parse_path: %s\n", path);
#endif

	remainder = NULL ;
	rpath = NULL ;
	/* check for pseudo devices */
	k = 0 ;
	p = 0;
	if ((rc=checkpseudo(path,iob,&p)) > 0)
		goto done;			/* error */
	else if (p)
		goto found;			/* found pseudo device */

	/* We sometimes receive paths in the old format
 	   scsi(0)disk(0)...
	   This function converts a scsi.. path to dksc format
	   which is understood by SN0.
	*/
	path = klmakealias(path) ;
	/* parse device pathname */
	path = klcheckaliases(path,&needfree);
	
	/*  Fill in Controller, and Unit. */
	/*  kl_get_dev_info fills iob->Unit, Controller and Partition */

	dev_info = kl_get_dev_info(path, &remainder, &iob->iob) ;
	if (!dev_info) {
		rc = ENODEV;
		goto done;
	}
	/* Check if found node has a driver */
	/*  If trying to open a controller with no driver, divert
	 * the request to the child, which should be a peripheral.
	 * For SN0, may be we should look at the parent or ...
	 */
	if (!dev_info->driver) {
		rc = ENODEV;
		goto done;
	}
	k = (klinfo_t *)dev_info->kl_comp;

#ifdef DEBUG
	printf("parse_path: ctrl(%d) unit(%d) file(%s)\n",iob->iob.Controller,
	       iob->iob.Unit, remainder);
#endif
	
	/* Check for partition(X) */
	/* SN0 - We will not support partition in the graph right now,
           but we will extract the partition number from the path */
	rpath = remainder ;

	if ((newtoken=next(remainder,&rpath,&key,&tlen)) != 0) {
		if ((tlen >= 4) && !strncmp(newtoken,"partition",tlen)) {
#ifdef DEBUG
			printf("parse_path: partition(%d)\n",key);
#endif
			iob->iob.Partition = key;
			newtoken = next(rpath,&rpath,&key,&tlen);
		}
	}

	/* check for ARCS specified protocols and SGI added filesystems.
	 */
	if (newtoken) {
		int (*func)();

		/*  Network registers tftp.  Convert bootp() tag to valid
		 * protocol tftp().
		 */
		if (strncmp(newtoken,"bootp",5) == 0) {
			newtoken = "tftp";
			tlen = 4;
		}

		if (strncmp(newtoken,"console",7) == 0) {
#ifdef DEBUG
			printf("parse_path: console(%d)\n",key);
#endif
			/* key can only be 0 or 1 */
			if (key == 0)
				iob->iob.Flags |= F_CONS;
			else if (key == 1)
				iob->iob.Flags |= F_ECONS;
			else {
				rc = EINVAL;
				goto done;
			}
			
			newtoken = 0;
		}
		else if (func = fs_match(newtoken,tlen)) {
#ifdef DEBUG
			printf("parse_path: found protocol!\n");
#endif
			if (key) {	/* protocols do not have parameters */
				rc = EINVAL;
				goto done;
			}
			iob->fsstrat = func;
			newtoken = 0;
		}
	}

	/*  If newtoken is still set, it hasn't been used, so it is the
	 * remainder of the path to be used as a filename.
	 */
	if (newtoken)
		rpath = newtoken;

	/* Copy any remaining path to the filename */
	if (*rpath != '\0') {
#ifdef DEBUG
		printf("parse_path: filename is %s\n", rpath);
#endif
		iob->filename = malloc(strlen(rpath)+1);
		if (iob->filename)
			strcpy(iob->filename,rpath);
	}

found:
	iob->klcompt = (klinfo_t *)k;
	iob->dev     = k->arcs_compt ;
	iob->devstrat = dev_info->driver;

	rc = ESUCCESS;
done:
	if (needfree)
		free(path);
	return(rc);
}
#endif /* !SN_PDI */

#ifdef SN_PDI

extern pd_dev_info_t *snGetPathInfo(char *, char **, IOBLOCK *) ;

int
sn_parse_path(char *path, struct eiob *iob)
{
	cfgnode_t *p, *adap, *ctrl;
	char *newtoken, *rpath;
	int needfree=0, rc;
	ulong tlen;
	ULONG key;
	klinfo_t *k ;
	pd_dev_info_t *pdev ;

#ifdef	DEBUG
	printf("sn_parse_path: %s\n", path);
#endif

	remainder = NULL ;
	rpath = NULL ;
	/* check for pseudo devices */
	k = 0 ;
	p = 0;
	if ((rc=checkpseudo(path,iob,&p)) > 0)
		goto done;			/* error */
	else if (p)
		goto found;			/* found pseudo device */

	/* We sometimes receive paths in the old format
 	   scsi(0)disk(0)...
	   This function converts a scsi.. path to dksc format
	   which is understood by SN0.
	*/
	path = klmakealias(path) ;
	/* parse device pathname */
	path = klcheckaliases(path,&needfree);
	
	/*  Fill in Controller, and Unit. */
	/*  kl_get_dev_info fills iob->Unit, Controller and Partition */

	pdev = snGetPathInfo(path, &remainder, &iob->iob) ;
	if (!pdev) {
		rc = ENODEV;
		goto done;
	}
	/* Check if found node has a driver */
	/*  If trying to open a controller with no driver, divert
	 * the request to the child, which should be a peripheral.
	 * For SN0, may be we should look at the parent or ...
	if (!pdev->pd) {
		rc = ENODEV;
		goto done;
	}
	 */
	k = (klinfo_t *)pdev->pdev_pdi.pdi_k;

#ifdef DEBUG
	printf("sn_parse_path: ctrl(%d) unit(%d) file(%s)\n",iob->iob.Controller,
	       iob->iob.Unit, remainder);
#endif
	
	/* Check for partition(X) */
	/* SN0 - We will not support partition in the graph right now,
           but we will extract the partition number from the path */
	rpath = remainder ;

	if ((newtoken=next(remainder,&rpath,&key,&tlen)) != 0) {
		if ((tlen >= 4) && !strncmp(newtoken,"partition",tlen)) {
#ifdef DEBUG
			printf("parse_path: partition(%d)\n",key);
#endif
			iob->iob.Partition = key;
			newtoken = next(rpath,&rpath,&key,&tlen);
		}
	}

	/* check for ARCS specified protocols and SGI added filesystems.
	 */
	if (newtoken) {
		int (*func)();

		/*  Network registers tftp.  Convert bootp() tag to valid
		 * protocol tftp().
		 */
		if (strncmp(newtoken,"bootp",5) == 0) {
			newtoken = "tftp";
			tlen = 4;
		}

		if (strncmp(newtoken,"console",7) == 0) {
#ifdef DEBUG
			printf("parse_path: console(%d)\n",key);
#endif
			/* key can only be 0 or 1 */
			if (key == 0)
				iob->iob.Flags |= F_CONS;
			else if (key == 1)
				iob->iob.Flags |= F_ECONS;
			else {
				rc = EINVAL;
				goto done;
			}
			
			newtoken = 0;
		}
		else if (func = fs_match(newtoken,tlen)) {
#ifdef DEBUG
			printf("parse_path: found protocol!\n");
#endif
			if (key) {	/* protocols do not have parameters */
				rc = EINVAL;
				goto done;
			}
			iob->fsstrat = func;
			newtoken = 0;
		}
	}

	/*  If newtoken is still set, it hasn't been used, so it is the
	 * remainder of the path to be used as a filename.
	 */
	if (newtoken)
		rpath = newtoken;

	/* Copy any remaining path to the filename */
	if (*rpath != '\0') {
#ifdef DEBUG
		printf("parse_path: filename is %s\n", rpath);
#endif
		iob->filename = malloc(strlen(rpath)+1);
		if (iob->filename)
			strcpy(iob->filename,rpath);
	}

found:
	iob->klcompt = (klinfo_t *)k;
	iob->dev     = k->arcs_compt ;
	iob->devstrat = pdev->pdev_pdi.pdi_devsw_p->_strat;

	rc = ESUCCESS;
done:
	if (needfree)
		free(path);
	return(rc);
}
#endif SN_PDI

#endif /* SN0 */
