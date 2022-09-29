/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SGI sub-agents
 *
 *	$Revision: 1.2 $
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

#include <oid.h>

class asnObject;
class asnObjectIdentifier;
class mibTable;

class sgiHWSubAgent : public subAgent {
	friend void		updateProcessorTable(void);

private:
	asnObjectIdentifier	subtree;
	mibTable *		processortable;
	mibTable *		graphicstable;
	unsigned int		processors;
	asnObjectIdentifier *	systemType;
	asnOctetString *	systemDesc;

	void			readInventory(void);
	void			createSystemDesc(void);

public:
	sgiHWSubAgent(void);
	~sgiHWSubAgent(void);

	// getSystemType() returns a new asnObjectIdentifier that will be
	// the object id of the type of system the daemon is running on.

	asnObjectIdentifier *	getSystemType(void)
			{ return new asnObjectIdentifier(*systemType); }

	// getSystemDesc() returns a new asnOctetString that will be
	// a system description of the system the daemon is running on.

	asnOctetString *	getSystemDesc(void)
			{ return new asnOctetString(*systemDesc); }

	virtual int	get(asnObjectIdentifier *, asnObject **, int *);
	virtual int	getNext(asnObjectIdentifier *, asnObject **, int *);
	virtual int	set(asnObjectIdentifier *, asnObject **, int *);
};

// SGI SubIDs for products.  68000 machines are < 20.  ASD machines are
// < 100.  ESD are > 100.  Tens digit is changed for processor board
// (i.e. IP7).  Last digit is number of processors on MP.

const subID IRIS_Unknown = 1;

const subID IRIS_Indigo_R4000 = 113;

const subID IRIS_Indigo2 = 120;
const subID IRIS_Indigo2_R8000 = 121;
const subID IRIS_Indigo2_R10000 = 122;

const subID IRIS_Indy = 130;

// The Challenge Series
const subID Challenge = 1000;
const subID PowerChallenge = 1500;
const subID NextChallenge = 2000;


class sgiNVSubAgent : public subAgent {
private:
	asnObjectIdentifier	subtree;

public:
	sgiNVSubAgent(void);
	~sgiNVSubAgent(void);

	virtual int	get(asnObjectIdentifier *, asnObject **, int *);
	virtual int	getNext(asnObjectIdentifier *, asnObject **, int *);
	virtual int	set(asnObjectIdentifier *, asnObject **, int *);
};
