/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Traffic Gadget
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

class tuGadget;
class tuDial;
class GLTileGadget;
class NetLook;

class TrafficGadget
{
public:
	TrafficGadget(NetLook *nl, tuGadget *parent, const char *instanceName);

	void update(void);

protected:
	// Callbacks from UI
	void setBasis(tuGadget *);
	void setMode(tuGadget *);
	void setRescale(tuGadget *);
	void setRescaleValue(tuGadget *);
	void setTimeout(tuGadget *);
	void setTimeoutValue(tuGadget *);
	void setMinColorValue(tuGadget *);
	void setMaxColorValue(tuGadget *);
	void setPacketColorStep(tuGadget *);
	void setByteColorStep(tuGadget *);
	void closeIt(tuGadget *);
	void help(tuGadget *);

	void setEnabled(tuGadget *, tuBool);
	void setTrafficVolumeLabels(void);

private:
	const char *name;
	tuGadget *ui;
	NetLook *netlook;
	GLTileGadget *colorTile;
	tuDial *packetStepDial;
	tuGadget *packetStepLabel;
	tuDial *byteStepDial;
	tuGadget *byteStepLabel;
	tuDial *activeDial;
	tuGadget *minColorLabel;
	tuGadget *maxColorLabel;
	tuGadget *unitColorLabel;
};
