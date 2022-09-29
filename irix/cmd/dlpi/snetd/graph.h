#ident "@(#)graph.h	1.11	11/13/92"
/*
 *  SpiderTCP Network Daemon
 *
 *      DAG Definitions
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.graph.h
 *	@(#)graph.h	1.11
 *
 *	Last delta created	17:17:23 7/6/92
 *	This file extracted	14:52:26 11/13/92
 *
 */

/*
 *  The following two data structures, when built from the specification
 *  obtained from the configuration file, describe the STREAMS structure
 *  in terms of a directed acyclic graph whoses nodes are individual
 *  modules and devices.
 *
 *  The first data structure holds a table (actually a linked list) of
 *  information about each module/driver, i.e. its name and type.  A
 *  pointer to the module's node in the directed graph is also kept in
 *  this table.
 *
 *                +------------+
 *    ==--------->|id|type|name|
 *                |------------|
 *              .-|next | graph|----> graph nodes
 *              | +------------+
 *              `-----------------.
 *                +------------+  |
 *                |id|type|name|<-'
 *                |------------|
 *              .-|next | graph|----> graph nodes
 *              | +------------+
 *              v
 *
 *  The second is a representation of the graph itself.  The edges of
 *  the graph represent downstream links.  Associated with each link
 *  is a set of control information to be sent to the *upstream* module/
 *  driver when the link/push is completed.
 *
 *  A file descriptor is associated with each link for internal use by
 *  the STREAMS construction code.
 *
 *                +------+  .-> module table entry
 *                |module|--'
 *    ==--------->|------|     +-------+     +----+   +----+
 *                | links|--.  |control|---->|F(a)|-->|F(a)|
 *                +------+  `->|stream |-.   +----+   +----+ 
 *                             |next o | |
 *                             +-----|-+ `-> downstream node
 *                                   v
 *                             +-------+     +----+   +----+
 *                             |control|---->|F(a)|-->|F(a)|
 *                             |stream |-.   +----+   +----+ 
 *                             |next o | |
 *                             +-----|-+ `-> downstream node
 *                                   v
 *
 *  A linked list keeps track of the stream heads.
 */

#define S_NULL ((char *)0)

struct module		/* per module/driver information */
{
	char		*id;		/* identifying string */
	int		type;		/* module/driver flags */
	char		*name;		/* module/device name */
	struct inst	*graph;		/* link to instantiations in graph */
	struct module	*next;		/* linked list connection */
};

/* flags for module.type */

#define DRIVER	01
#define MODULE	02
#define CLONE	04

typedef struct module	*module_t;	/* module pointer */

#define M_NULL	((struct module *) 0)

struct inst		/* instantiation of node */
{
	struct node	*node;		/* pointer to node */
	int		instant;	/* instantiation number */
	struct inst	*next;		/* next link in chain */
};

typedef struct inst	*inst_t;	/* instantiation pointer */

#define T_NULL	((struct inst *) 0)

struct info		/* control information sent when making links */
{
	int		(*function)();	/* function which does it */
	int		argc;		/* argument count */
	char		**argv;		/* argument list */
	struct info	*next;		/* next in chain */
};

typedef struct info	*info_t;	/* control data info */

#define I_NULL	((struct info *) 0)

struct edge		/* edge in directed acyclic graph */
{
	struct node	*stream;	/* downstream node */
	struct info	*control;	/* control instructions */
	int		fd;		/* downstream file descriptor */
	int		muxid;		/* muxid from I_LINK */
	struct edge	*next;		/* next in chain */
};

typedef struct edge	*edge_t;	/* edge pointer */

#define E_NULL	((struct edge *) 0)

struct node		/* node in directed acyclic graph */
{
	struct module	*module;	/* pointer to module info */
	struct edge	*links;		/* chain of downstream links */
};

typedef struct node	*node_t;	/* node pointer */

#define N_NULL	((struct node *) 0)

struct chain		/* chained list of nodes */
{
	struct node	*chain;		/* node */
	struct chain	*next;		/* linked list pointer */
};

typedef struct chain	*chain_t;	/* linked list pointer */

#define C_NULL	((struct chain *) 0)

extern struct module	*modtab;	/* module table */
extern struct chain	*heads;		/* list of stream heads */
