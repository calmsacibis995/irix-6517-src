/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	RMON sub-agent
 *
 *	$Revision: 1.1 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

extern "C" {
#include <snoopstream.h>
#include <histogram.h>
}

class mibTable;

/* Maximum interfaces supported */
#define MAX_IF	16

class rmonSubAgent : public subAgent {
	friend void nvprocess(void *);

private:
	asnObjectIdentifier	subtree;

	char *			hostname;
	struct {
		struct snoopstream	ss;
		struct histogram	hist;
		unsigned int		refcount;
		pid_t			pid;
	} nv[MAX_IF];

	mibTable *		statTable;

	int			startNetVis(int, char *);
	int			stopNetVis(int);
	int			protosinitted;
	void			initprotos(void);

public:
	rmonSubAgent(void);
	~rmonSubAgent(void);

	virtual int	get(asnObjectIdentifier *, asnObject **, int *);
	virtual int	getNext(asnObjectIdentifier *, asnObject **, int *);
	virtual int	set(asnObjectIdentifier *, asnObject **, int *);
};

