/***********************************************************************\
*	File:		pod_cmd.c					*
*									*
*	Contains a list of commands to access Mapram/F/VMECC and test   *
*	them.								*
*	pod_cmd is a misnomer. But sticks for time being		*
*									*
\***********************************************************************/

#ifndef	NEWTSIM		/* Entire File */
#ident	"arcs/I04prom/pod_cmd.c:  $Revision: 1.11 $"

#include <parser.h>

#include "pod_fvdefs.h"
#include <sys/EVEREST/evconfig.h>

/* External declarations of the commands */
int  mapram_cmd(int, char **);
int  io4_cmd(int, char **);
int  iaram_cmd(int, char **);
int  fchip_cmd(int, char **);
int  vmecc_cmd(int, char **);
int  vmelb_cmd(int, char **);
int  help_cmd(int, char **);
int  fchip_reg(int, char **);
int  vme_reg(int, char **);
int  vme_a64lb(int, char **);
int  io_errdsp(int, char **);
int  io_errclr(int, char **);
int  vme_stres(int, char **);

int  cmd_chk(int, char **, int, int (*)() ); 
int  printf(char *fmt, ...);

#define	HELP	"help"

/*
 * podcmd_table : Table containing the list of pod commands which can be 
 *	invoked by the menu level pod_cmd 
 */
static struct cmd_table podcmd_table[] = {
	{ ".io4reg",	CT_ROUTINE io4_cmd,
			"IO4 reg test\t: io4reg io4_slot\n"},
	{ "mapram",	CT_ROUTINE iaram_cmd,
			"Mapram test\t:mapram io4_slot\n"},
	{ "fchip",	CT_ROUTINE fchip_cmd,
			"Fchip \t\t:fchip io4_slot ioa_num\n"},
	{ "vmecc",	CT_ROUTINE vmecc_cmd,
			"VMEcc \t\t:vmecc io4_slot ioa_num\n"},
	{ "vmelb",	CT_ROUTINE vmelb_cmd,
			"VME loopback\t:vmelb io4_slot ioa_num\n"},
	{ "fregs",	CT_ROUTINE fchip_reg,
			"Fchip regs\t:fregs io4_slot ioa_num\n"},
	{ "vregs",	CT_ROUTINE vme_reg,
			"VMECC regs\t:vregs io4_slot ioa_num\n"},
	{ "vmea64",	CT_ROUTINE vme_a64lb,
			"VMEcc A64 loopback: vmea64 slot anum\n"},
	{ "vstress",	CT_ROUTINE vme_stres,
			"VME stress\t vstress slot anum\n"}, 
	{ ".errdsp",	CT_ROUTINE io_errdsp,
			"Error reg\t errdsp io4_slot ioa_num\n"},
	{ ".errclr",	CT_ROUTINE io_errclr,
			"ErrReg clr\t errclr io4_slot ioa_num\n"},
	{ HELP,		CT_ROUTINE help_cmd,  "List Diagnostic commands\n"},
	{ 0,		0,		"" }
};

#define	PODC_EEXIST	": No such command \n"
#define PODC_IOAN	": invalid number of parameters\n "
#define PODC_SLOT	": invalid slot number\n"
#define PODC_ANUM	"bad adapter number - "

extern int check_io4(unsigned, unsigned);
extern int check_vmecc(int, int, int);
extern int check_fchip(int, int, int);
extern int fregs(int, int, int);
extern int vregs(int, int, int);
extern int vmea64_lpbk(int, int, int);
extern int check_mapram(int, int);
extern int io_errreg(int, int, int);
extern int io_errcreg(int, int, int);
extern int doall(int, int, int);

int atoi(char *);
int config_io4(int );

/*
 * Function :do_cmd  
 * Description :
 *	Execute the pod_cmd specified as part of the command line in argv
 */
/*ARGSUSED*/
int
do_cmd(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{

    struct cmd_table *pct;

    if ((argc < 2) || (argc > 4)){
	message("%s %s",argv[0], PODC_IOAN);
	return(1);
    }

    if ((pct = lookup_cmd(podcmd_table, argv[1])) == (struct cmd_table *)0){
	message("%s %s",argv[1], PODC_EEXIST);
	return(1);
    }

    if ((pct->ct_routine)((argc-1), &argv[1], argp, 0))
	message(pct->ct_usage);				/* Command failed */

    return 0;
}

/* Steps to be followed by all the command functions.
 *
 * Convert the io4_slot to the io4_window number 
 * Find out if this board has been configured, otherwise
 * do the needed configuration steps.
 *
 * Call the appropriate routine 
 */
int io4_cmd(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 1, check_io4));
}
int iaram_cmd(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 1, check_iaram));    
}
int mapram_cmd(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 1, check_mapram));    

}
int fchip_cmd(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 2, check_fchip));
}

int vmecc_cmd(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 2, check_vmecc));
}
int vmelb_cmd(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 2, pod_vmelpbk));
}

int fchip_reg(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 2, fregs));
}

int vme_reg(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 2, vregs));
}

int vme_a64lb(int argc, char **argv)
{
    message("vmea64 lpbk routine called \n");
    return(cmd_chk(argc, argv, 2, vmea64_lpbk));
}

int io_errdsp(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 2, io_errreg));
}

int io_errclr(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 2, io_errcreg));
}

int vme_stres(int argc, char **argv)
{
    return(cmd_chk(argc, argv, 2, doall));
}


int
cmd_chk(int argc, char **argv, int exp_par, int (*func)() )
{
   /* do all the checking based on the input parameters, and call
    * the function 
    */

    int io4_slot, iowin;
    int anum=0;
   
    if (argc != (exp_par+1)){
       message("%s %s",argv[0], PODC_IOAN);
       return(1);
    }

    io4_slot = atoi(argv[1]);

    if ((io4_slot <= 0) || (io4_slot >= EV_MAX_SLOTS)){
	message("%s: %d %s",argv[0], io4_slot, PODC_SLOT);
	return(1);
    }

    if (exp_par == 2){
	anum = atoi(argv[2]);
        if ((anum < 0) || (anum > 7)){
	    message("%s: %s: %d\n",argv[0], anum, PODC_ANUM);
	    return(1);
        }
    }

    if ((iowin = config_io4(io4_slot)) == 0){
	message("%s: Bad IO4 slot number - %d, exiting\n",argv[0], io4_slot);
	return(1);
    }

    (func)(iowin, io4_slot, anum);

    return(0);

}

/*ARGSUSED*/
int help_cmd(int argc, char **argv)
{
    /* List the diagnostic commands available */

    struct cmd_table *pct = podcmd_table;

    printf("Following Diagnostics commands are available\n");

    for (; pct->ct_string; pct++){
	if ((pct->ct_string[0] == '.') || (strcmp(pct->ct_string , HELP) == 0))
	    continue;
	printf(pct->ct_usage);
    }
    return 0;
}

int
doall(int window, int ioslot, int anum)
{
    uint	err=0;
    for (;;) {
	printf("Loopcount : %d\n", err++);
	if (pod_vmelpbk(window, ioslot, anum))
	    break;
    }
    return 1;
}

int config_io4(int ioslot) 
{
    int window  = EVCFGINFO->ecfg_board[ioslot].eb_io.eb_winnum;
#if  0
    EV_SET_CONFIG(ioslot, IO4_CONF_LW, window);
    EV_SET_CONFIG(ioslot, IO4_CONF_SW, window);
    EV_SET_CONFIG(ioslot, IO4_CONF_ENDIAN, !getendian());
    EV_SET_CONFIG(ioslot, IO4_CONF_INTRMASK, 0x2); 
#endif

    return(window);

}

#endif
