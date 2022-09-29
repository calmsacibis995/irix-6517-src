/*
 *  SpiderTCP Network Daemon
 *
 *      Streams Building
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.build.c
 *	@(#)build.c	1.13
 *
 *	Last delta created	17:11:11 7/6/92
 *	This file extracted	14:52:18 11/13/92
 *
 */

#ident "@(#)build.c	1.13	11/13/92"

#include "debug.h"
#include "graph.h"
#include "function.h"
#include "utils.h"

extern int	doopen();
extern		doclose();
extern		dopush();
extern int	dolink();

static int	descend();

/*
 *  build()	--	build the STREAMS network
 */

void
build()
{
	chain_t	head;

	/*
	 *  Run through the stream heads, working down through modules
	 *  and multiplexers, etc until all streams are constructed.
	 */

	for (head = heads; head != C_NULL; head = head->next)
	{
		XPRINT1("BUILD down from %s\n", head->chain->module->id);

		/*
		 *  Construct everything from driver up.
		 *
		 *  Ignore the file descriptor returned, since this
		 *  is the control stream and must remain open to
		 *  keep the stream together.
		 */

		(void) descend(head->chain);
	}
}

static int
descend(node)
node_t	node;
{
	int	control;	/* control stream file descriptor */
	edge_t	e;
	info_t	i;

	/*
	 *  If we've reached the stream end (must be driver),
	 *  just open the driver and return the file descriptor.
	 *
	 *  Otherwise, recursively ((c) GSS 1988) process each
	 *  downstream link, linking or pushing as appropriate.
	 *
	 *  It's difficult to work out an optimum strategy for
	 *  opening/closing file descriptors here.  The choice
	 *  seems to be between having all the drivers above
	 *  and below a single multiplexor open at one time,
	 *  or to have all drivers along the full length of a
	 *  stream open simultaneously.  We choose the latter
	 *  approach here 'cause the code's nicer, but it's a
	 *  simple matter to modify the following code to do
	 *  otherwise.
	 */

	if (!node->links)	/* no links - at stream end */
	{
		if (node->module->type & MODULE)
			exit(error("no driver below module \"%s\"",
				node->module->name));
		else		/* driver - simply open and return */
			return doopen(node->module->name);
	}

	/*
	 *  For a module, simple push the module and pass up the
	 *  control stream file descriptor.
	 */

	if (node->module->type & MODULE)
	{
		if (node->links->next)	/* too many downstream links */
			exit(error("attempt to multiplex module \"%s\"",
				node->module->name));

		e = node->links;

		XPRINT1("DOWNSTREAM to %s\n", e->stream->module->id);

		/*
		 *  Recursively descend the link.
		 */

		e->fd = descend(e->stream);

		/*
		 *  Push the module onto the given stream.
		 */

		dopush(e->fd, node->module->name);

		/*
		 *  Perform any control actions necessary.
		 */

		for (i = e->control; i != I_NULL; i = i->next)
			(*i->function)(i->argc, i->argv, e->fd, 0);

		/*
		 *  Pass up the file descriptor.
		 */

		return e->fd;
	}

	/*
	 *  For a driver, open the upper (possibly multiplexing)
	 *  driver, then descend each downstream link in turn,
	 *  linking the lower stream under the driver and closing
	 *  the now unneeded file descriptor.
	 */

	control = doopen(node->module->name);

	for (e = node->links; e != E_NULL; e = e->next)
	{
		if (e->fd != -1) continue;	/* already done */

		/*
		 *  If there is no lower driver, there was an
		 *  explicit muxid value given, which is passed
		 *  directly to the control action functions.
		 */

		if (!e->stream)
		{
			XPRINT1("EXPLICIT MUXID %d\n", e->muxid);
			e->fd = doexpmux(control, e->muxid);
		}
		else
		/*
		 *  Descend and link
		 */
		{
			XPRINT1("DOWNSTREAM to %s\n", e->stream->module->id);

			/*
		 	 *  Recursively descend the link.
			 */

			e->fd = descend(e->stream);

			/*
			 *  Link in the lower driver, close file descriptor.
			 */

			e->muxid = dolink(control, e->fd); doclose(e->fd);
		}

		/*
		 *  Perform any control actions necessary.
		 */

		for (i = e->control; i != I_NULL; i = i->next)
			(*i->function)(i->argc, i->argv, control, e->muxid);
	}

	return control;
}
