#ifndef __desc_h_
#define __desc_h_

/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser Describer
 *
 *	$Revision: 1.2 $
 *	$Date: 1992/10/03 01:07:06 $
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


#include <tuResourceDB.h>

#include "bTopLevel.h"

class tuGadget;


class Desc : public bTopLevel {
public:
    Desc(struct mibNode* n, const char* instanceName, bTopLevel* otherTopLevel);


private:
    void	saveFile(FILE*);
    void	writeSaveFile(FILE*);
    void	closeIt(tuGadget*);
};


#endif /* __desc_h_ */
