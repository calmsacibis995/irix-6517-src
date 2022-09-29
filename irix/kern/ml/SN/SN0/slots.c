/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1996 Silicon Graphics, Inc.                             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * slots.c
 *	Support for slot nbumbering on SN0 machines
 */

#ident "$Revision: 1.29 $"

#include <sys/types.h>
#include <sys/idbg.h>
#include <sys/SN/slotnum.h>
#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xbow.h>
#include <sys/SN/agent.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>

#ifndef _STANDALONE
#include <sys/systm.h>
#include <sys/pda.h>
#else
#include <libsc.h>
#endif
/*
 * Origin 2000:
 *
 *	Xbow 0  Old    New
 *	------
 *	port 8:  W0 	IO1 (BaseIO slot)
 *	     9:  N0	N1
 *	     A:  N2	N3
 *	     B:  W3	IO4
 *	     C:  W1	IO2 (PCI Module slot)
 *	     D:  W5	IO6
 * 	     E:  W4	IO5
 * 	     F:  W2	IO3
 *
 *	Xbow 1
 * 	------
 * 	port 8:  W10 	IO11
 *	     9:  N3	N4
 * 	     A:  N1	N2
 *	     B:  W7	IO8
 * 	     C:  W9	IO10
 * 	     D:  W11	IO12
 *	     E:  W6	IO7
 *	     F:  W8	IO9
 *
 * Onyx 2 Deskside:
 *
 *	Xbow 0  Slot
 *	------
 *	port 8: IO4 (Gfx slot)
 *	     9: N2
 *	     A: N1
 *	     B: IO2 (PCI Module slot)
 *	     C: IO6
 *	     D: IO5
 * 	     E: IO3
 * 	     F: IO1 (BaseIO slot)
 *
 * Speedo xbox:
 *	
 *	Xbow 
 *	----
 *	Port 	Slot
 *	8	io7
 *	9	io1
 *	A	Motherboard
 *	B	io2
 * 	C	io3
 *	D	io4
 *	E	io5
 *	F	io6
 */

static int kxbowslot_table[MAX_XBOW_PORTS] = {
	SLOTNUM_XTALK_CLASS | 4,	/* Gfx slot */
	SLOTNUM_KNODE_CLASS | 2,
	SLOTNUM_KNODE_CLASS | 1,
	SLOTNUM_XTALK_CLASS | 2,	/* PCI wart */
	SLOTNUM_XTALK_CLASS | 6,
	SLOTNUM_XTALK_CLASS | 5,
	SLOTNUM_XTALK_CLASS | 3,
	SLOTNUM_XTALK_CLASS | 1		/* Base IO */
};

static int xbow0slot_table[MAX_XBOW_PORTS] = {
	SLOTNUM_XTALK_CLASS | 1,	/* Base IO */
	SLOTNUM_NODE_CLASS | 1,
	SLOTNUM_NODE_CLASS | 3,
	SLOTNUM_XTALK_CLASS | 4,
	SLOTNUM_XTALK_CLASS | 2,	/* PCI wart */
	SLOTNUM_XTALK_CLASS | 6,
	SLOTNUM_XTALK_CLASS | 5,
	SLOTNUM_XTALK_CLASS | 3
};

static int xbow1slot_table[MAX_XBOW_PORTS] = {
	SLOTNUM_XTALK_CLASS | 11,
	SLOTNUM_NODE_CLASS | 4,
	SLOTNUM_NODE_CLASS | 2,
	SLOTNUM_XTALK_CLASS | 8,
	SLOTNUM_XTALK_CLASS | 10,
	SLOTNUM_XTALK_CLASS | 12,
	SLOTNUM_XTALK_CLASS | 7,
	SLOTNUM_XTALK_CLASS | 9
};

static int xboxslot_table[MAX_XBOW_PORTS] = {
	SLOTNUM_XTALK_CLASS | 7,
	SLOTNUM_XTALK_CLASS | 1,
	SLOTNUM_NODE_CLASS  | 1,
	SLOTNUM_XTALK_CLASS | 2,
	SLOTNUM_XTALK_CLASS | 3,
	SLOTNUM_XTALK_CLASS | 4,
	SLOTNUM_XTALK_CLASS | 5,
	SLOTNUM_XTALK_CLASS | 6
};
#define NODE_SLOTBIT_SIZE	8
#define ROUTER_SLOTBIT_SIZE	16

static int nodeslot_table[NODE_SLOTBIT_SIZE] = {
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_KNODE_CLASS | 2,
	SLOTNUM_KNODE_CLASS | 1,
	SLOTNUM_NODE_CLASS | 4,
	SLOTNUM_NODE_CLASS | 3,
	SLOTNUM_NODE_CLASS | 2,
	SLOTNUM_NODE_CLASS | 1
};

/* Define a new slot table for the 12p 4io config */
static int CONFIG_12P4I_nodeslot_table[NODE_SLOTBIT_SIZE] = {
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_NODE_CLASS | 6,
	SLOTNUM_NODE_CLASS | 5,
	SLOTNUM_NODE_CLASS | 4,
	SLOTNUM_NODE_CLASS | 3,
	SLOTNUM_NODE_CLASS | 2,
	SLOTNUM_NODE_CLASS | 1
};

static int slotwidget_table[4] = {
	0x9,
	0xa,
	0xa,
	0x9
};
/* In the case of speedo with an xbox .
 * The master hub always goes to port 10 of the xbow.
 * Slave if present will go to port  9.
 * NOTE: Dual hosting of xbox is not fully supported yet.
 */

static int xboxslotwidget_table[2] = {
	0xa,
	0xa
};

static int kslotwidget_table[4] = {
	0xa,
	0x9,
	0xff,
	0xff
};

static int routerslot_table[ROUTER_SLOTBIT_SIZE] = {
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_ROUTER_CLASS | 2,
	SLOTNUM_ROUTER_CLASS | 1
};


static int meta_routerslot_table[ROUTER_SLOTBIT_SIZE] = {
	SLOTNUM_ROUTER_CLASS | 8,
	SLOTNUM_ROUTER_CLASS | 7,
	SLOTNUM_ROUTER_CLASS | 6,
	SLOTNUM_ROUTER_CLASS | 5,
	SLOTNUM_ROUTER_CLASS | 4,
	SLOTNUM_ROUTER_CLASS | 3,
	SLOTNUM_ROUTER_CLASS | 2,
	SLOTNUM_ROUTER_CLASS | 1,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS,
	SLOTNUM_INVALID_CLASS
};


/* Inverse mapping from the displayed slot number to widget number */
/* Needed for command line parsing of flash prom commands. */

static int slot_to_widget_table[] = {
	8, 0xC, 0xF, 0xB, 0xE, 0xD, 0x1E, 0x1B, 0x1F, 0x1C, 0x18, 0x1D
} ;

static int xboxslot_to_widget_table[] = {
	0x9,0xb,0xc,0xd,0xe,0xf,8,-1
};
static int kslot_to_widget_table[] = {
	0xf, 0xb, 0xe, 0x8, 0xd, 0xc, -1, -1, -1, -1, -1, -1
};

slotid_t
xbwidget_to_xtslot(int crossbow, int widget)
{
	if ((widget < BASE_XBOW_PORT) || (widget >= MAX_PORT_NUM))
		return SLOTNUM_INVALID_CLASS;

	if (SLOTNUM_GETCLASS(get_my_slotid()) == SLOTNUM_KNODE_CLASS) {
		/* KEGO */
		if (crossbow != 0) {
			printf("xbwidget_to_xtslot: Error - crossbow nonzero on Kego (%d).\n", crossbow);
			return SLOTNUM_INVALID_CLASS;
		}

		return kxbowslot_table[widget - BASE_XBOW_PORT];

	} else {
		/* Speedo with xbox */
		if (SN00) 
			return(xboxslot_table[widget - BASE_XBOW_PORT]);

		/* 8P12IO */
		if (crossbow == 0)
			return xbow0slot_table[widget - BASE_XBOW_PORT];
		else if (crossbow == 1)
			return xbow1slot_table[widget - BASE_XBOW_PORT];
		else
			return SLOTNUM_INVALID_CLASS;
	}
}


slotid_t
hub_slotbits_to_slot(slotid_t slotbits)
{
	slotid_t new_slotbits;
#ifndef _STANDALONE
	if (private.p_sn00) {
#else
	if (SN00) {
#endif
		new_slotbits = (slotbits & MSU_SN00_SLOTID_MASK) >> MSU_SN00_SLOTID_SHFT;
		return(2 - new_slotbits); 
	} else
		new_slotbits = slotbits & MSU_SN0_SLOTID_MASK;
	if (new_slotbits >= NODE_SLOTBIT_SIZE) {
		return SLOTNUM_INVALID_CLASS;
	} else {

		if (CONFIG_12P4I) 
			return(CONFIG_12P4I_nodeslot_table[new_slotbits]);
		else {
			return nodeslot_table[new_slotbits];
		}
	}
}

xwidgetnum_t
hub_slot_to_widget(slotid_t slot)
{
	if (SLOTNUM_GETSLOT(slot) >= NODE_SLOTBIT_SIZE) {
		return -1;
	} else {
		if (SLOTNUM_GETCLASS(slot) == SLOTNUM_KNODE_CLASS) {
			return kslotwidget_table[SLOTNUM_GETSLOT(slot) - 1];
		} else {
			/* Right now this is needed for an SN00 with an xbox.
			 * In the case of single host it is assumed that the
			 * hub comes in on port 10 of the xbow.
			 */
			if (SN00)
				return(xboxslotwidget_table[SLOTNUM_GETSLOT(slot) - 1]);
			return slotwidget_table[SLOTNUM_GETSLOT(slot) - 1];
		}
	}
}

slotid_t
hub_slot_to_crossbow(slotid_t hub_slot)
{
	/* Crossbow 0 is connected to nodes 1 and 3. */
	/* Crossbow 1 is connected to nodes 2 and 4. */

	if (SLOTNUM_GETCLASS(hub_slot) == SLOTNUM_KNODE_CLASS)
		return 0;
	else
		return ((hub_slot - 1) & 1);
}

slotid_t
router_slotbits_to_slot(slotid_t slotbits)
{
	if (slotbits >= ROUTER_SLOTBIT_SIZE)
		return SLOTNUM_INVALID_CLASS;
	else
		return routerslot_table[slotbits];
}

slotid_t
meta_router_slotbits_to_slot(slotid_t slotbits)
{
	if (slotbits >= ROUTER_SLOTBIT_SIZE)
		return SLOTNUM_INVALID_CLASS;
	else
		return meta_routerslot_table[slotbits];
}

/*
 * This is static because calling outside of this
 * file is the wrong thing to do. Outside callers should call
 * get_slotname() as systems like the Origin 200 can and do
 * have special slot naming needs
 */

static char *
get_slotclass(slotid_t slotnum)
{
	switch (SLOTNUM_GETCLASS(slotnum)) {
		case SLOTNUM_NODE_CLASS:
		case SLOTNUM_KNODE_CLASS:
			return "n";
		case SLOTNUM_ROUTER_CLASS:
			return "r";
		case SLOTNUM_XTALK_CLASS:
			return "io";
		case SLOTNUM_INVALID_CLASS:
			return "Invalid";
		default:
			return "Unknown";
	}
}

void
get_slotname(slotid_t slotnum, char *name)
{
	/*
	 * On Origin 200 slots 1 and 2 are really the 
	 * motherboard
	 */
#ifndef _STANDALONE
	if (private.p_sn00) {
#else
	if (SN00){
#endif
		if (((SLOTNUM_GETCLASS(slotnum) == SLOTNUM_NODE_CLASS) &&
		     (SLOTNUM_GETSLOT(slotnum) == 1)) ||
		    ((SLOTNUM_GETCLASS(slotnum) == SLOTNUM_NODE_CLASS) &&
		     (SLOTNUM_GETSLOT(slotnum) == 2))) {
			strcpy(name, SN00_MOTHERBOARD);
			return;
		}
	}

	sprintf(name, "%s%d", get_slotclass(slotnum), SLOTNUM_GETSLOT(slotnum));
}


slotid_t
get_node_slotid(nasid_t nasid)
{
	slotid_t slotbits = REMOTE_HUB_L(nasid, MD_SLOTID_USTAT) & MSU_SLOTID_MASK;
	return (hub_slotbits_to_slot(slotbits));
}

slotid_t
get_my_slotid()
{
	slotid_t slotbits = LOCAL_HUB_L(MD_SLOTID_USTAT) & MSU_SLOTID_MASK;
	return (hub_slotbits_to_slot(slotbits));
}

slotid_t
get_node_crossbow(nasid_t nasid)
{
	return hub_slot_to_crossbow(get_node_slotid(nasid));
}

void
get_my_slotname(char *name)
{
	get_slotname(get_my_slotid(), name);
}

slotid_t
get_widget_slotnum(int xbow, int widget)
{
	return xbwidget_to_xtslot(xbow, widget);
}

void
get_widget_slotname(int xbow, int widget, char *name)
{
	get_slotname(get_widget_slotnum(xbow, widget), name);
}

void
router_slotbits_to_slotname(int slotbits, char *name)
{
	slotid_t slotnum;

	slotnum = router_slotbits_to_slot(slotbits);

	get_slotname(slotnum, name);
}

int
slot_to_widget(int slotnum)
{
	if (SLOTNUM_GETCLASS(get_my_slotid()) == SLOTNUM_KNODE_CLASS) {
		/* KEGO */
		return kslot_to_widget_table[slotnum];
	} else {
		/* Speedo with xbox */
		if (SN00) {
			if (slotnum >= MAX_XBOW_PORTS)
				return(-1);
			else
				return(xboxslot_to_widget_table[slotnum]);
		}
		else
			return slot_to_widget_table[slotnum];
	}
}

/*
 * int hub_slot_get(void)
 *
 *   Returns current node card slot number from 1 (rightmost) to 4 (leftmost).
 *
 *   On KEGO, there are only two node cards with slot numbers 5 and 6; this
 *   routine converts those to 1 and 2.
 *
 */
slotid_t
hub_slot_get()
{
	return (SLOTNUM_GETSLOT(get_my_slotid()));
}

#ifdef _STANDALONE
/*
 * Function:		get_module_slot
 * Args:		bridge_base -> bridge widget base
 *			module, slot -> ptrs to be filled up
 * Retunrs:		nothing
 * Note:		moduleid must be set properly in the log
 */
void get_module_slot(__psunsigned_t bridge_base, moduleid_t *module, int *slot)
{
        int     nasid, wid_num;
        char    buf[8];

        nasid = NASID_GET(bridge_base);

        ip27log_getenv(nasid, IP27LOG_MODULE_KEY, buf, "0", 0);
        *module = atoi(buf);

        wid_num = WIDGETID_GET(bridge_base);
        *slot = SLOTNUM_GETSLOT(xbwidget_to_xtslot(get_node_crossbow(nasid), 
		wid_num));
}
#endif

/* Check if a node in a particular slot can talk to the
 * elsc. In 12p 4io config nodes in slots 5 & 6 are 
 * disabled from talking to the elsc.
 */
int
node_can_talk_to_elsc(void)
{
	if (CONFIG_12P4I) {
		if (hub_slot_get() <= 
		    HIGHEST_I2C_VISIBLE_NODESLOT)
			return 1;
		else
			return 0;
	}
	return 1;
}

/* 
 * is_xbox_config
 *	Checks if the configuration is a speedo connected to an xbox.
 */
int
is_xbox_config(nasid_t nasid)
{
	if (!SN00)
		return(0);
#if defined(_STANDALONE)
	/* Check if the hub is talking to a xbow */
	if (hubii_widget_is_xbow(nasid))
		return(1);
#else
	/* Xbow present is a convenienc macro to check if a particular
	 * hub is talking to an xbow over its xtalk link.
	 */
#define XBOW_PRESENT(_n) 	\
	(((((*(volatile __int32_t *)(NODE_SWIN_BASE(_n, 0) + WIDGET_ID))) & \
	   WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT) == XBOW_WIDGET_PART_NUM)

	if (XBOW_PRESENT(nasid))
		return(1);
#endif	
	return(0);
}
/*
 * ip27log_xbox_nasid_clear
 *	Clear the promlog variable to indicate the nasid attached to the xbox.
 */
void
ip27log_xbox_nasid_clear(nasid_t nasid)
{
	(void)ip27log_unsetenv(nasid,IP27LOG_XBOX_NASID,0);
}

/*
 * ip27log_xbox_nasid_set
 *	Set the promlog variable to indicate the nasid attached to the xbox.
 */
void
ip27log_xbox_nasid_set(nasid_t	nasid,nasid_t xbox_nasid)
{
	char	buf[8];
	
	sprintf(buf,"%d",xbox_nasid);
	(void)ip27log_setenv(nasid,IP27LOG_XBOX_NASID,buf,0);
}
/*
 * ip27log_xbox_nasid_get
 *	Read the promlog xbox_nasid variable to find out nasid of the node
 *	attached to the xbox.
 */
nasid_t
ip27log_xbox_nasid_get(nasid_t	nasid) 
{
	char	buf[8];
	char	invalid_nasid[8];
	
	/* The default value for this promlog variable is INVALID_NASID */
	sprintf(invalid_nasid,"%d",INVALID_NASID);
	(void)ip27log_getenv(nasid,IP27LOG_XBOX_NASID,buf,invalid_nasid,0);
	return(atoi(buf));
}
/*
 * xbox_nasid_get
 *	Return the nasid of the node connected to the xbox.	
 */
nasid_t
xbox_nasid_get(void)
{

	/* If we are not a speedo then this function shouldn't be used
	 */
	if (!SN00)
		return(INVALID_NASID);

	/* Get the xbox_nasid from the promlog */
	return(ip27log_xbox_nasid_get(get_nasid()));

}
