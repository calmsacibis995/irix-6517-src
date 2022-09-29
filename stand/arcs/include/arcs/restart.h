/* ARCS restart block
 */

#ifndef _ARCS_RESTART_H
#define _ARCS_RESTART_H

#ident "$Revision: 1.13 $"

#include <arcs/types.h>

#define SSA_WORDS	128
#define SSA_LENGTH	(sizeof(LONG)*SSA_WORDS)

typedef struct RestartBlock {
	LONG			Signature;
	ULONG			Length;
	SHORT			Version;
	SHORT			Revision;
	struct RestartBlock	*Next;
	LONG			RestartAddress;
	LONG			BootMasterID;
	LONG			ProcessorID;
	LONG			BootStatus;
	LONG			Checksum;
	LONG			SSALength;
	LONG			SSArea[SSA_WORDS];
} RestartBlock;

/* flags for BootStatus */
#define BS_BSTARTED	0x01		/* Boot Started */
#define BS_BFINISHED	0x02		/* Boot Finished */
#define BS_RSTARTED	0x04		/* Restart Started */
#define BS_RFINISHED	0x08		/* Restart Finished */
#define BS_PFSTARTED	0x10		/* Power Fail Started */
#define BS_PFFINISHED	0x20		/* Power Fail Finished */
#define BS_PREADY	0x40		/* Processor Ready */

extern void Halt(void);
extern void PowerDown(void);
extern void Restart(void);
extern void Reboot(void);
extern void EnterInteractiveMode(void);

#define RB_SIGNATURE	0x42545352
#define RB_VERSION	SGI_ARCS_VERS
#define RB_REVISION	SGI_ARCS_REV

extern void init_rb(void);
extern int isvalid_rb(void);
extern int save_rb(int, char **, char **);
extern int restore_rb(int *, char ***, char ***);
extern RestartBlock *get_rb (void);
extern RestartBlock *get_cpu_rb (int);

#endif
