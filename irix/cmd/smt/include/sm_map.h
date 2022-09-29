#ifndef SM_MAP_H
#define SM_MAP_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.10 $
 */

/*
 * SGI-specific Operations.
 */
#define FDDI_SNMP_INFO		1
#define FDDI_SNMP_UP		2
#define FDDI_SNMP_DOWN		3
#define FDDI_SNMP_THRUA		4
#define FDDI_SNMP_THRUB		5
#define FDDI_SNMP_WRAPA		6
#define FDDI_SNMP_WRAPB		7
#define FDDI_SNMP_STAT		8
#define FDDI_SNMP_SINGLE	9
#define FDDI_SNMP_DUAL		10
#define FDDI_SNMP_DUMP		11
#define FDDI_SNMP_REGFS		12
#define FDDI_SNMP_UNREGFS	13
#define FDDI_SNMP_TRACE		14
#define FDDI_SNMP_MAINT		15
#define FDDI_SNMP_SENDFRAME	16
#define FDDI_SNMP_FSSTAT	17
#define FDDI_SNMP_FSDELETE	18
#define FDDI_SNMP_VERSION	19

typedef struct {
	u_long	mac_ct;
	char names[10][10];
} IPHASE_INFO;

typedef struct {
	SMT_MAC mac;
	SMT_PHY phy_0;
	SMT_PHY phy_1;
} MAC_INFO;

typedef struct {
	u_long		phy_ct;
	struct smt_conf conf_0;
	struct smt_conf conf_1;
} PORT_INFO;

extern void map_setpeer(char*);
extern char *map_getpeer();
extern void map_settimo(int, int);
extern void map_resettimo(void);
extern int map_open(char *, char *(*)());
extern void map_close(void);
extern int map_smt(char*, int, void*, int, int);
extern void map_exit(int);

#endif
