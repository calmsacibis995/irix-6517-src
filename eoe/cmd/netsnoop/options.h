#ifndef OPTIONS_H
#define OPTIONS_H
/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * Snoop command line and configuration options.
 */

struct options {
	int	count;		/* # of packets to buffer/capture */
	int	stop;		/* whether to stop after count packets */
	int	errflags;	/* error flags to snoop for */
	char	*hexprotos;	/* protocols to decode as hex */
	char	*interface;	/* network interface name */
	char	*listprotos;	/* protocols to list with help messages */
	int	snooplen;	/* bytes to capture after raw header */
	char	*nullprotos;	/* protocols we don't want to view */
	char	*tracefile;	/* write raw output to this tracefile */
	int	queuelimit;	/* socket input queue limit */
	int	rawsequence;	/* don't rewrite snoop sequence numbers */
	int	dostats;	/* whether to show snoop statistics */
	int	interval;	/* snoop for a fixed time interval */
	char	*viewer;	/* packet viewer to use */
	int	viewlevel;	/* packet viewing detail level */
	int	hexopt;		/* whether and what to hexdump */
	int	useyp;		/* whether to use NIS for name lookup */
};

int	setoptions(int, char **, struct options *);
int	scanoptvec(int, char **, struct options *);
/* char*	getrcname(int, char **); */
void	badexpr(int, struct exprerror *);

#endif
