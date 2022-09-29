/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	HP processes sub-agent
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

/* Definitions for the processFlags MIB object. They map the MIB enumeration to
   the flag codes found in sys/proc.h
 */

#define PT_FLAG_INCORE	1	/* in core */
#define PT_FLAG_SSYS	2	/* system resident process */
#define PT_FLAG_LOCKED	4	/* locked */
#define PT_FLAG_STRC	8	/* traced */
#define PT_FLAG_STRC2	16	/* profiling */
 
class mibTable;

class hpProcSubAgent : public subAgent {
	friend void updateProcTable(void);

private:
	asnObjectIdentifier	subtree;

	unsigned int		cache_time;
	long			tickspersec;
	time_t			sysboottime_secs; // To quickly calculate processStart
	unsigned int		procnumber;
	mibTable *		proctable;

public:
	hpProcSubAgent(void);
	~hpProcSubAgent(void);

	virtual int	get(asnObjectIdentifier *, asnObject **, int *);
	virtual int	getNext(asnObjectIdentifier *, asnObject **, int *);
	virtual int	set(asnObjectIdentifier *, asnObject **, int *);
};
