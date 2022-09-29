/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SGI sub-agents
 *
 *	$Revision: 1.3 $
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
        char			*cpudesc;
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


#ifndef NEW_SGI_SYSOBJID

// SGI SubIDs for products.  68000 machines are < 20.  ASD machines are
// < 100.  ESD are > 100.  Tens digit is changed for processor board
// (i.e. IP7).  Last digit is number of processors on MP.

const subID IRIS_Unknown = 1;

const subID IRIS_Indigo_R4000 = 113;

const subID IRIS_Indigo2 = 120;
const subID IRIS_Indigo2_R8000 = 121;
const subID IRIS_Indigo2_R10000 = 122;

const subID IRIS_Indy = 130;
const subID IRIS_O2 = 140;
const subID IRIS_Octane = 150;		/* 15x where x is 1-2 */
const subID IRIS_Origin200 = 160;	/* 16x where x is 1-4 */

const subID Challenge = 1000;		/* 10xx where xx is 1-36 */
const subID PowerChallenge = 1500;	/* 15xx where xx is 1-24 */
const subID IRIS_Origin2000 = 2000;		/* 2xxx where xxx is 1-256 */
const subID NextOrigin = 3000;

#endif


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



#ifdef NEW_SGI_SYSOBJID

/* machine class*/
#define SGI_UNKNOWN	       1
#define SGI_WORKSTATION        2
#define SGI_SERVER  	       3
#define SGI_SUPERCOMPUTER      4

const subID IRIS_Unknown = 1;

const subID indigo = 1;	

const subID indigo2= 2;
const subID powerIndigo2= 3;

const subID indy= 4;

const subID o2= 5;

const subID octane= 6;

const subID onyx= 7;
const subID onyx2= 8;
const subID powerOnyx2= 9;

const subID origin200= 1;
const subID origin2000= 2;

const subID challenge= 3;
const subID challengeDM= 4;
const subID challengeL= 5;
const subID challengeM= 6;
const subID challengeXL= 7;
const subID challengeS= 8;

const subID powerChallenge= 9;
const subID powerChallengeM= 10;

const subID crayOrigin2000= 1;

const subID crayT94= 2;
const subID crayT916= 3;
const subID crayT932= 4;

const subID crayJ916= 5;
const subID crayJ931= 6;
const subID crayJ90se= 7;

const subID crayT3E=8;
const subID crayT3E_900= 9;
const subID crayT3E_1200= 10;

#endif

typedef char bool_t;
#define TRUE  1
#define FALSE 0
