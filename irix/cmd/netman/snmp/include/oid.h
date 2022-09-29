#ifndef __OID_H__
#define __OID_H__
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Object Identifier
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

/* Maximum Length OID supported */
#define OID_MAX_LENGTH		64

typedef unsigned short subID;

/*
 * An ASN.1 object identifier class.
 *
 */
class oid {
	friend class asnObjectIdentifier;
public:
	oid(void);
	oid(const char *s);
	oid(oid &o);
	~oid(void);

	void setValue(const char *s);
	void setValue(subID *s, unsigned int l);
	inline void getValue(subID **id, unsigned int *l);

	// getString() will return a new char[] that will not be deleted
	// on destruction of the class instance.
	char *	getString(char *s = 0);

private:
	unsigned int length;
	subID *subid;
};

// Inline functions
void
oid::getValue(subID **id, unsigned int *l)
{
    *id = subid;
    *l = length;
}
#endif /* !__OID_H__ */
