/* getpath.c:  Get a pathname for a component.
 */

#ident "$Revision: 1.12 $"

#include <arcs/hinv.h>
#include <libsc.h>

/* in gpdata.c */
extern struct cfgdata ParseData[];

static char *
getcomponent(COMPONENT *c)
{
	struct cfgdata *p;

	/* filter out the components that we can't open */
	if ((c->Class != PeripheralClass) &&
	    (c->Class != AdapterClass) &&
	    (c->Class != ControllerClass)) 
		return("");

	for (p=ParseData; p->name ; p++)
		if (c->Type == p->type)
			return(p->name);

	return("unknown");
}

static char *
walk_path(COMPONENT *p, char *buf)
{
	char *s;

	if (!p)
		return(buf);

	buf = walk_path(GetParent(p),buf);

	strcat(buf,s=getcomponent(p));
	buf += strlen(buf);
	if (*s) {
		sprintf(buf,"(%u)",p->Key);
		buf += strlen(buf);
	}

	return(buf);
}

char *
getpath(COMPONENT *c)
{
	static char buf[256];

	buf[0] = '\0';
	walk_path(c,buf);

	return(buf[0] ? buf : NULL);
}
