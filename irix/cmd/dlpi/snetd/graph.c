/*
 *  SpiderTCP Network Daemon
 *
 *      DAG Construction
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.graph.c
 *	@(#)graph.c	1.19
 *
 *	Last delta created	17:15:14 7/6/92
 *	This file extracted	14:52:25 11/13/92
 *
 */

#ident "@(#)graph.c	1.19	11/13/92"

#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "graph.h"
#include "function.h"
#include "utils.h"

extern	lerror();

struct module	*modtab = M_NULL;	/* module table */
struct chain	*heads = C_NULL;	/* list of stream heads */

static module_t	mlookup();
static edge_t	mkedge();

static int
mflags(s)
char *s;
{
	int	flags = 0;

	if (!s) return flags;

	while (*s) switch (*s++)
	{
	case 'd':	flags |= DRIVER;	break;
	case 'c':	flags |= CLONE;		break;
	case 'm':	flags |= MODULE;	break;
	default:	lerror("warning: ignoring '%c' flag", *(s-1));
			break;
	}

	if (flags == 0)
		exit(lerror("must specify 'd' or 'm' flag"));

	if (flags == CLONE)
	{
		lerror("warning: assuming 'c' flags means driver");
		flags |= DRIVER;
	}

	if ((flags&DRIVER) && (flags&MODULE))
	{
		lerror("warning: 'm' flag ignored for driver");
		flags &= ~MODULE;
	}

	if ((flags&MODULE) && (flags&CLONE))
	{
		lerror("warning: 'c' flag ignored for module");
		flags &= ~CLONE;
	}

	return flags;
}

/*
 *  insert()	--	insert a module description into the table
 */

void
insert(id, flags, name)
char	*id;
char	*flags;
char	*name;
{
	module_t	mod;

	XPRINT3("INSERT id=%s flags=%s name=%s\n", id, flags, name);

	mod = (module_t) memory(sizeof(struct module));
	mod->id = id;
	mod->type = mflags(flags);
	mod->name = name;
	mod->graph = T_NULL;
	mod->next = M_NULL;

	if (modtab != M_NULL)		/* append to end of table */
	{
		module_t	m;

		for (m = modtab; m->next != M_NULL; m = m->next)
			;

		m->next = mod;
	}
	else				/* first in table */
		modtab = mod;
}

/*
 *  mlookup()	--	look up an entry in the module table
 */

static module_t
mlookup(id)
char	*id;
{
	module_t	mod;

	for (mod = modtab; mod != M_NULL; mod = mod->next)
		if (streq(mod->id, id))
			break;

	if (mod == M_NULL)
		exit(lerror("unknown module: \"%s\"", id));

	return mod;
}

/*
 *  graph_link()	--	link a pair of nodes in the directed graph
 */

void
graph_link(upper, lower, info)
char	*upper;
char	*lower;
list_t	info;
{
	module_t	umod, lmod;
	node_t		unode, lnode;
	int		nu=0, nl=0;
	inst_t		ui, li;
	edge_t		l, link;
	info_t		*i;
	chain_t		d, dp;
	char		*s;

	PRINT2("LINK %s under %s", lower, upper);

	/*
	 *  Process upper module...
	 */

	{
	 	/* Check for explicit module instantiation */
		if (s = strchr(upper, ':'))
		{
			*s++ = '\0';
			if (!isdigit(*s))
				exit(lerror("non-numeric instantiation"));
			nu = atoi(s);
		}

		/* Locate upper module */
		umod = mlookup(upper);

		/* Create graph node if not already there */
		for (ui = umod->graph; ui != T_NULL; ui = ui->next)
			if (ui->instant == nu)
				break;

		if (ui == T_NULL)		/* new instantiation */
		{
			unode = (node_t) memory(sizeof(struct node));
			unode->module = umod;
			unode->links = E_NULL;

			ui = (inst_t) memory(sizeof(struct inst));
			ui->instant = nu;
			ui->node = unode;
			ui->next = umod->graph;

			umod->graph = ui;

			/*
			 *  Place on list of stream heads until
			 *  proven innocent.  The head of the
			 *  list will do (we're not too worried
			 *  about queue-jumping here).
			 */

			for (d = heads; d != C_NULL; d = d->next)
				if (d->chain->module == umod)
					break;

			if (d == C_NULL)	/* he wasn't already there */
			{
				XPRINT1("* adding %s to heads\n", umod->name);

				d = (chain_t) memory(sizeof(struct chain));
				d->chain = unode;
				d->next = heads;
				heads = d;
			}
		}
		else				/* found instantiation */
			unode = ui->node;
	}


	/*
	 *  Process lower module and make edge...
	 */

	
	if (streq(lower, "-") || isdigit(lower[0]))
	{	/* no muxid or explicit muxid */
		int muxid;

		if (lower[0] == '-')	/* No actual link */
			muxid = 0;
		else
		{
			/* Explicit muxid: check is numeric and non-zero */
			for (s = lower; *s;)
				if (!isdigit(*s++))
					exit(lerror("non-numeric muxid"));
			if ((muxid = atoi(lower)) == 0)
				exit(lerror("zero explicit muxid"));
		}

		/*
		 *  Create a new link between upper node and "nowhere".
		 *  Fill in muxid value.
		 */

		link = mkedge(N_NULL);
		link->muxid = muxid;
	}
	else
	{
	 	/* Check for explicit module instantiation */
		if (s = strchr(lower, ':'))
		{
			*s++ = '\0';
			if (!isdigit(*s))
				exit(lerror("non-numeric instantiation"));
			nl = atoi(s);
		}

		/* Locate lower module */
		lmod = mlookup(lower);

		/* Create graph node if not already there */
		for (li = lmod->graph; li != T_NULL; li = li->next)
			if (li->instant == nl)
				break;

		if (li == T_NULL)		/* new instantiation */
		{
			lnode = (node_t) memory(sizeof(struct node));
			lnode->module = lmod;
			lnode->links = E_NULL;

			li = (inst_t) memory(sizeof(struct inst));
			li->instant = nl;
			li->node = lnode;
			li->next = lmod->graph;

			lmod->graph = li;
		}
		else				/* found instantiation */
		{
			lnode = li->node;

			/*
			 *  OK - he's proved his innocence.
			 *  ("I'm really not a stream head, honest!").
			 *  Let him off the hook if we haven't already.
			 */

			for (d = heads; d != C_NULL; dp = d, d = d->next)
				if (d->chain->module == lmod)
					break;

			if (d != C_NULL) /* he was there - cut the rope */
			{
				XPRINT1("* taking %s off heads\n", lmod->name);

				if (d == heads)	/* he was the list head */
					heads = heads->next;
				else		/* he was part way down */
					dp->next = d->next;

				mfree(d);
			}
		}

		/*
		 *  Create a new link between upper and lower nodes.
		 */

		link = mkedge(lnode);
	}

	/*
	 *  Tag new edge on to the end of the link list.
	 */

	if (unode->links == E_NULL)
		unode->links = link;
	else
	{
		for (l = unode->links; l->next != E_NULL; l = l->next)
			;

		l->next = link;
	}

	/*
	 *  Associate control data with edge.
	 */

	for (i = &link->control; info; i = &(*i)->next, info = info->next)
	{
		list_t		message = (list_t) info->data;
		list_t		lp;
		function_t	f;
		int		n;

		PRINT1(" [%s]", message->data);

		/*
		 *  Look up function description...
		 */

		if ((f = flookup(message->data)) == F_NULL)
			exit(lerror("unknown control action: %s",
				message->data));

		/*
		 *  Allocate info structure and append to list...
		 *
		 *  Do a quick run through first to count the arguments,
		 *  then allocate the argument list and do a second run
		 *  through to construct the list.
		 */

		*i = (info_t) memory(sizeof(struct info));

		(*i)->function = f->fcall;

		for ((*i)->argc=1, lp=message; lp->next; lp=lp->next)
			(*i)->argc++;

		(*i)->argv = (char **) array((*i)->argc + 1, sizeof(char *));

		for (n=0, lp=message; n < (*i)->argc; n++, lp=lp->next)
		{
			PRINT1(" \"%s\"", lp->data);
			(*i)->argv[n] = lp->data;
		}
		(*i)->argv[n] = S_NULL;

		(*i)->next = I_NULL;
	}

	PRINT("\n");
}

static edge_t
mkedge(lnode)
node_t	lnode;
{
	edge_t	e = (edge_t) memory(sizeof(struct edge));

	e->stream = lnode;
	e->control = I_NULL;
	e->fd = -1;
	e->muxid = 0;
	e->next = E_NULL;

	return e;
}
