#include "options.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>		/* For varargs */
#include <stdlib.h>		/* For string conversion routines */
#include <string.h>
#include <bstring.h>
#include <ctype.h>		/* For character definitions */
#include <errno.h>
#include <rpc/rpc.h>

#define XDR_SHORTCUT

#include "debug.h"
#include "fcagent_structs.h"
#include "fcagent_rpc.h"

typedef struct {
  /* Initializable via command line */
  int     _command;
  char   *_hostname;
  int     _channel_id;
  int     _encl_phys_id;
  int    *_target_id;
  int     _target_count;
  CLIENT *_cl;
} AppData;
#define CMD         (app_data._command)
#define HOSTNAME    (app_data._hostname)
#define CH_ID       (app_data._channel_id)
#define ENCL_PHYSID (app_data._encl_phys_id)
#define T_ID        (app_data._target_id)
#define T_COUNT     (app_data._target_count)
#define CL          (app_data._cl)

typedef struct {
  char *cmd_name;
  int (*cmd_id)();
  int   cmd_need_hostname;
  int   cmd_need_channel_id;
  int   cmd_need_target_id;
} command_table_struct_t;

/* 
 * FORWARD DECLARATIONS
 */
int fccli_getversion();
int fccli_getstatus();
int fccli_remove();
int fccli_insert();
int fccli_ledon();
int fccli_ledoff();
int fccli_flash();
int fccli_bypass();
int fccli_unbypass();
char *stat2str(int stat);
char *fanstat2str(int stat);
char *psstat2str(int stat);
char *lccstat2str(int stat);
int channel_exists(int ch_id);
int target_exists(int ch_id, int t_id, enc_stat_t **out_stat);

int fccli_show_ch_status(cfg_t *cfg, int which_ch_index);
int fccli_show_encl_status(int ch_id, int encl_id, int long_status);

/*
 * GLOBAL VARIABLES
 */
AppData app_data = {
  -1,				/* command */
  NULL,				/* hostname */
  -1,				/* channel_id */
  -1,				/* encl_id */
  NULL, 0,			/* target_id, target_count */
  NULL				/* cl */
};

command_table_struct_t commands[] = {
  { "GETVERSION",    fccli_getversion, 0, 0, 0 },
  { "GETSTATUS",     fccli_getstatus,  0, 0, 0 },
  { "REMOVE",        fccli_remove,     0, 1, 1 },
  { "INSERT",        fccli_insert,     0, 1, 1 },
  { "LEDON",         fccli_ledon,      0, 1, 1 },
  { "LEDOFF",        fccli_ledoff,     0, 1, 1 },
  { "FLASH",         fccli_flash,      0, 1, 1 },
  { "BYPASS",        fccli_bypass,     0, 1, 1 },
  { "UNBYPASS",      fccli_unbypass,   0, 1, 1 },
  { "getversion",    fccli_getversion, 0, 0, 0 },
  { "getstatus",     fccli_getstatus,  0, 0, 0 },
  { "remove",        fccli_remove,     0, 1, 1 },
  { "insert",        fccli_insert,     0, 1, 1 },
  { "ledon",         fccli_ledon,      0, 1, 1 },
  { "ledoff",        fccli_ledoff,     0, 1, 1 },
  { "flash",         fccli_flash,      0, 1, 1 },
  { "bypass",        fccli_bypass,     0, 1, 1 },
  { "unbypass",      fccli_unbypass,   0, 1, 1 },
  { NULL,            NULL,             0, 0, 0 }
};

void cleanup()
{
}

void Syntax(int argc, char **argv)
{
  int i;

#if 0
  if (argc > 1) {
    printf("Unknown options :\n");
    for (i = 1; i < argc; ++i) {
      printf("%s, ", argv[i]);
    }
    printf("\n");
  }
#endif
  printf("Usage : %s GETVERSION [-h hostname]\n", argv[0]);
  printf("      : %s GETSTATUS [-h hostname] [-c channel ID [ -e eid | -t tid1 [tid2 ... [tidn]] ]\n", argv[0]);
  printf("      : %s REMOVE [-h hostname] -c channel ID -t tid1 [tid2 ... [tidn]]\n", argv[0]);
  printf("      : %s INSERT [-h hostname] -c channel ID -t tid1 [tid2 ... [tidn]]\n", argv[0]);
  printf("      : %s LEDON [-h hostname] -c channel ID -t tid1 [tid2 ... [tidn]]\n", argv[0]);
  printf("      : %s LEDOFF [-h hostname] -c channel ID -t tid1 [tid2 ... [tidn]]\n", argv[0]);
  printf("      : %s FLASH [-h hostname] -c channel ID -t tid1 [tid2 ... [tidn]]\n", argv[0]);
  printf("      : %s BYPASS [-h hostname] -c channel ID -t tid1 [tid2 ... [tidn]]\n", argv[0]);
  printf("      : %s UNBYPASS [-h hostname] -c channel ID -t tid1 [tid2 ... [tidn]]\n", argv[0]);
}

main(int argc, char **argv)
{
  int cmd_index = -1;
  int i, j;
  char *p;
  
  if (argc < 2)
    goto syntax_error;

  /* Parse command */
  for (i = 0; commands[i].cmd_name; ++i) {
    if (strcmp(commands[i].cmd_name, argv[1]) == 0) {
      cmd_index = i;
      break;
    }
  }
  if (cmd_index == -1)
    goto syntax_error;

  /* Parse rest of arguments */
  for (i = 2; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'h':
	++i;
	HOSTNAME = argv[i];
	break;
      case 'c':
	if (argv[i][2])
	   goto syntax_error;
	 ++i; 
	 CH_ID = strtol(argv[i], &p, 10);
	if (*p)
	  goto syntax_error;
	break;
      case 'e':
	if (argv[i][2])
	   goto syntax_error;
	 ++i;
	 ENCL_PHYSID = strtol(argv[i], &p, 10);
	if (*p)
	  goto syntax_error;
	break;
      case 't':
	if (argv[i][2])
	  goto syntax_error;
	++i;
	T_COUNT = argc-i;
	T_ID = calloc(T_COUNT, sizeof(int));
	for (j = i; j < argc; ++j) {
	  T_ID[j-i] = strtol(argv[j], &p, 10);
	  if (*p)
	    goto syntax_error;
	}
	i = argc;
	break;
      default:
	goto syntax_error;
      }
    }
  }

  /* Check parameters */
  if (commands[cmd_index].cmd_need_hostname && HOSTNAME == NULL)
    goto syntax_error;
  if (commands[cmd_index].cmd_need_channel_id && CH_ID == -1)
    goto syntax_error;  
  if (commands[cmd_index].cmd_need_target_id && T_ID == 0)
    goto syntax_error;  

  if (HOSTNAME == NULL)
    HOSTNAME = "localhost";

#if 0
  printf("COMMAND      = %s\n", commands[cmd_index].cmd_name);
  printf("HOSTNAME     = %s\n", HOSTNAME);
  printf("CHANNEL ID   = %d\n", CH_ID);
  printf("ENCLOSURE ID = %d\n", ENCL_PHYSID);
  printf("TARGET IDS   = ");
  for (i = 0; i < T_COUNT; ++i)
    printf("%d ", T_ID[i]);
  printf("\n");
#endif

  CL = clnt_create(HOSTNAME, AGENT_PROG, AGENT_VERS, "tcp");
  if (CL == NULL) {
    clnt_pcreateerror(HOSTNAME);
    cleanup();
    exit(1);
  }

  if (commands[cmd_index].cmd_id)
    (*commands[cmd_index].cmd_id)();
  
  cleanup();
  exit(0);

 syntax_error:
  Syntax(argc, argv);
  cleanup();
  exit(1);
}

int fccli_getversion()
{
  char **result_version;

  result_version = agent_get_vers_1(NULL, CL);
  if (result_version == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  printf("%s\n", *(char **)result_version);
  free(*result_version);
  return(0);
}

int fccli_getstatus()
{
  cfg_t *result_config;
  enc_addr_t in_addr;
  enc_stat_t *out_stat;
  int i, j;

  /* 
   * Get configuration
   */
  result_config = agent_get_config_1(NULL, CL);
  if (result_config == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (result_config->cfg_t_len == 0) {
    printf("No channels found.\n");
    return(0);
  }
  
  /* 
   * Did the user specify a channel ID ? If the user didn't specify a
   * channel, print out a summary status of all channels. If the user
   * did specify a channel, print a detailed summary of enclosure
   * status.
   */
  if (CH_ID == -1) {
    fccli_show_ch_status(result_config, -1);
  }
  else {
    for (i = 0; i < result_config->cfg_t_len; ++i) {
      if (result_config->cfg_t_val[i].ch_id == CH_ID) {
	/* Did the user specify an ENCL ID ? */
	if (ENCL_PHYSID == -1 && T_COUNT == 0) {
	  fccli_show_ch_status(result_config, i);
	}
	else 
        if (ENCL_PHYSID >= 0) {
	  for (j = 0; j < result_config->cfg_t_val[i].encl_id_len; ++j) {
	    in_addr.ch_id = CH_ID;
	    in_addr.encl_id = result_config->cfg_t_val[i].encl_id_val[j];
	    out_stat = agent_get_status_1(&in_addr, CL);
	    if (out_stat == NULL) {
	      clnt_perror(CL, HOSTNAME);
	      return(-1);
	    }
	    if (out_stat->encl_phys_id == ENCL_PHYSID) {
	      fccli_show_encl_status(CH_ID, out_stat->encl_id, 1);
	      return(0);
	    }
	  }
	  printf("Enclosure %d does not exist on channel %d on host '%s'.\n", 
		 ENCL_PHYSID,CH_ID, HOSTNAME);
	  return(-1);
	}
	else {
	  printf("Target status not implemented yet.\n");
	  return(-1);
	}
	return(0);
      }
    }
    printf("Channel %d does not exist on '%s'.\n", CH_ID, HOSTNAME);
    return(-1);
  }
  return(0);
}

int fccli_show_ch_status(cfg_t *cfg, int which_ch_index)
{
  int drv_stat=0, fan_stat=0, ps_stat=0, lcc_stat_failed=0, lcc_stat_missing=0;
  int i, j, k;
  enc_addr_t in_addr;
  enc_stat_t *out_stat;
  int jbods_found = 0, invalid_jbods_found = 0;


  if (which_ch_index == -1) {
    printf("CHANNEL | DRIVES    POWER     FANS      PEER LCC  \n");
    printf("--------+-----------------------------------------\n");
#if 0
    printf("-xxxx---|--xxxx------xxxx-----xxxx-------xxxx-----\n");
    printf("01234567890123456789012345678901234567890123456789\n");
#endif
    
    for (i = 0; i < cfg->cfg_t_len; ++i) {
      drv_stat=fan_stat=ps_stat=lcc_stat_failed=lcc_stat_missing=0;
      invalid_jbods_found = 0;
      if (cfg->cfg_t_val[i].encl_id_len) {
	for (j = 0; j < cfg->cfg_t_val[i].encl_id_len; ++j) {
	  in_addr.ch_id = cfg->cfg_t_val[i].ch_id;
	  in_addr.encl_id = cfg->cfg_t_val[i].encl_id_val[j];
	  out_stat = agent_get_status_1(&in_addr, CL);
	  if (out_stat == NULL) {
	    clnt_perror(CL, HOSTNAME);
	    return(-1);
	  }
	  if (in_addr.encl_id != out_stat->encl_id) {
	    printf("fccli_show_encl_status(): Enclosure ID mismatch.\n");
	    return(-1);
	  }
	  if (out_stat->encl_status == STS_INVALID) {
	    ++invalid_jbods_found;
	    continue;
	  }

	  for (k = 0; k < out_stat->drv_len; ++k) {
	    drv_e_t drv = (drv_e_t) out_stat->drv_val[k].drv_stat_t_val;
	    if (drv->status == STS_FAILED || drv->status == STS_PEER_FAILED)
	      ++drv_stat;
	  }
	  for (k = 0; k < out_stat->fan_len; ++k) {
	    fan_e_t fan = (fan_e_t) out_stat->fan_val[k].fan_stat_t_val;
	    if (fan->status == STS_FAILED || fan->status == STS_PEER_FAILED)
	      ++fan_stat;
	  }
	  for (k = 0; k < out_stat->ps_len; ++k) {
	    ps_e_t ps = (ps_e_t) out_stat->ps_val[k].ps_stat_t_val;
	    if (ps->status == STS_FAILED || ps->status == STS_PEER_FAILED)
	      ++ps_stat;
	  }
	  for (k = 0; k < out_stat->lcc_len; ++k) {
	    lcc_e_t lcc = (lcc_e_t) out_stat->lcc_val[k].lcc_stat_t_val;
	    if (!lcc->peer_present)
	      ++lcc_stat_missing;
	    if (lcc->peer_failed)
	      ++lcc_stat_failed;
	  }
	} /* for (j) */
	++jbods_found;
	printf(" %4d   |  %4s      %4s     %4s       %4s\n",
	       cfg->cfg_t_val[i].ch_id,
	       invalid_jbods_found ? "PART" : (drv_stat ? "FAIL" : " OK "),
	       invalid_jbods_found ? "PART" : (ps_stat  ? "FAIL" : " OK "),
	       invalid_jbods_found ? "PART" : (fan_stat ? "FAIL" : " OK "),
	       invalid_jbods_found ? "PART" : (lcc_stat_failed ? "FAIL" : 
					       lcc_stat_missing ? " OUT" : " OK "));
      }
      else {
	printf(" %4d   |  \n", cfg->cfg_t_val[i].ch_id);
      }	/* if (cfg->cfg_t_val[i].encl_id_len)  */
    } /* for (i) */
    if (jbods_found == 0)
      printf("<No FC JBOD boxes found>\n");
  } /* if (which_ch_index == -1) */
  else {
    printf("ENCLOSURE | DRIVES    POWER     FANS      PEER LCC  \n");
    printf("----------+-----------------------------------------\n");
#if 0
    printf("---xxxx---|--xxxx------xxxx-----xxxx-------xxxx-----\n");
    printf("0123456789012345678901234567890123456789012345678901\n");
#endif

    if (cfg->cfg_t_val[which_ch_index].encl_id_len) {
      for (j = 0; j < cfg->cfg_t_val[which_ch_index].encl_id_len; ++j) {
	drv_stat=fan_stat=ps_stat=lcc_stat_failed=lcc_stat_missing=0;
	in_addr.ch_id = cfg->cfg_t_val[which_ch_index].ch_id;
	in_addr.encl_id = cfg->cfg_t_val[which_ch_index].encl_id_val[j];
	out_stat = agent_get_status_1(&in_addr, CL);
	if (out_stat == NULL) {
	  clnt_perror(CL, HOSTNAME);
	  return(-1);
	}
	if (in_addr.encl_id != out_stat->encl_id) {
	  printf("fccli_show_encl_status(): Enclosure ID mismatch.\n");
	  return(-1);
	}
	if (out_stat->encl_status == STS_INVALID) {
	  printf("   %4d   |  %4s      %4s     %4s       %4s\n",
		 out_stat->encl_phys_id,
		 "UNKN",
		 "UNKN",
		 "UNKN",
		 "UNKN");
	  continue;
	}

	for (k = 0; k < out_stat->drv_len; ++k) {
	  drv_e_t drv = (drv_e_t) out_stat->drv_val[k].drv_stat_t_val;
	  if (drv->status == STS_FAILED || drv->status == STS_PEER_FAILED)
	    ++drv_stat;
	}
	for (k = 0; k < out_stat->fan_len; ++k) {
	  fan_e_t fan = (fan_e_t) out_stat->fan_val[k].fan_stat_t_val;
	  if (fan->status == STS_FAILED || fan->status == STS_PEER_FAILED)
	    ++fan_stat;
	}
	for (k = 0; k < out_stat->ps_len; ++k) {
	  ps_e_t ps = (ps_e_t) out_stat->ps_val[k].ps_stat_t_val;
	  if (ps->status == STS_FAILED || ps->status == STS_PEER_FAILED)
	    ++ps_stat;
	}
	for (k = 0; k < out_stat->lcc_len; ++k) {
	  lcc_e_t lcc = (lcc_e_t) out_stat->lcc_val[k].lcc_stat_t_val;
	  if (!lcc->peer_present)
	    ++lcc_stat_missing;
	  if (lcc->peer_failed)
	    ++lcc_stat_failed;
	}
	printf("   %4d   |  %4s      %4s     %4s       %4s\n",
	       out_stat->encl_phys_id,
	       drv_stat ? "FAIL" : " OK ",
	       ps_stat  ? "FAIL" : " OK ",
	       fan_stat ? "FAIL" : " OK ",
	       (lcc_stat_failed ? "FAIL" : 
		lcc_stat_missing ? " OUT" : " OK "));
      }
    }
  }

  return(0);
}

int fccli_show_encl_status(int ch_id, int encl_id, int long_status)
{
  enc_addr_t in_addr;
  enc_stat_t *out_stat;
  char tmpString[20];
  int k, l;

  in_addr.ch_id = ch_id;
  in_addr.encl_id = encl_id;
  out_stat = agent_get_status_1(&in_addr, CL);
  if (out_stat == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (encl_id != out_stat->encl_id) {
    printf("fccli_show_encl_status(): Enclosure ID mismatch.\n");
    return(-1);
  }
  if (long_status) {
    int i;
#if 0
    unsigned long long wwn;
    u_char *p1, *p2;
#endif
    
    if (out_stat->encl_status == STS_INVALID) {
      printf(" Enclosure %d, status %s\n", out_stat->encl_phys_id, "UNKNOWN");
      return(0);
    }
    printf(" Enclosure %d, status %s\n", out_stat->encl_phys_id, stat2str(out_stat->encl_status));
    strncpy(tmpString, out_stat->encl_vid, 8); tmpString[8] = '\0';
    printf("  Vendor ID:     %s\n", tmpString);
    strncpy(tmpString, out_stat->encl_pid, 8); tmpString[8] = '\0';
    printf("  Product ID:    %s\n", tmpString);
    { 
      /* 
       * Break up LCC string into it's constituent parts
       * Bytes:  0- 9 = board part #
       *        10-21 = serial number
       *        22-23 = CLARiiON mfg rev #
       *        24-27 = mfg date characters; WWFY, WW=work week, FY=fiscal year
       *        28-30 = firmware rev level
       */
      char brd_part_num[16];
      char serial_num[16];
      char rev_level[16];
      char mfg_date[16];
      char firmware_rev[16];
      
      bzero(brd_part_num, sizeof(brd_part_num));     
      bcopy(out_stat->encl_lccstr_val, brd_part_num, 10); 

      bzero(serial_num, sizeof(serial_num));                   
      bcopy(out_stat->encl_lccstr_val+10, serial_num, 12); 

      bzero(rev_level, sizeof(rev_level));
      bcopy(out_stat->encl_lccstr_val+22, rev_level, 2); 

      bzero(mfg_date, sizeof(mfg_date));
      bcopy(out_stat->encl_lccstr_val+24, mfg_date, 4); 

      bzero(firmware_rev, sizeof(firmware_rev));
      bcopy(out_stat->encl_lccstr_val+28, firmware_rev, 3); 

      printf("  LCC SN#:       %s\n", serial_num);
      printf("  LCC Mfg. Date: %s\n", mfg_date);
      printf("  LCC code rev:  %s\n", firmware_rev);
    }
    printf("  FRUs:          %d disk slot(s)\n"
	   "                 %d fan slot(s)\n"
	   "                 %d power supply slot(s)\n"
	   "                 %d peer LCC slot(s)\n",
	   out_stat->drv_len, 
	   out_stat->fan_len, 
	   out_stat->ps_len, 
	   out_stat->lcc_len);
    printf("\n");
  }

#if 0
  printf("  +---------------------------------------+------+\n");
  printf("  | [ee] xxxx                             |      |\n");
  printf("  +---------------------------------------+      |\n");
  printf("  | * | * | * | * | * | * | * | * | * | * | P    |\n");
  printf("  | x | x | x | x | x | x | x | x | x | x | E  L |\n");
  printf("  | x | x | x | x | x | x | x | x | x | x | E  C |\n");
  printf("  | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | R  C |\n");
  printf("  +---------------------------------------+      |\n");
  printf("  |---------------FANS-xxxx---------------| xxxx |\n");
  printf("  |------PS0-xxxx-----------PS1-xxxx------|      |\n");
  printf("  +---------------------------------------+------+\n");
#endif

  if (out_stat->lcc_len > 1) {
    fatal("%d LCC elements reported.\n");
  }
  if (out_stat->ps_len > 2) {
    fatal("%d Power Supply elements reported.\n");
  }


  printf("  +---------------------------------------+------+\n");
  printf("  | [%2d] %4s                             |      |\n", 
	 out_stat->encl_phys_id, out_stat->encl_status ? "FAIL" : " OK ");
  printf("  +---------------------------------------+      |\n");
  for (l = 0; l < 4; ++l) {
    printf("  |");
    for (k = 0; k < out_stat->drv_len; ++k) {
      drv_e_t drv = (drv_e_t) out_stat->drv_val[k].drv_stat_t_val;
      int fault_led = 0;
      if (drv->drv_asserting_fault || drv->enc_asserting_fault)
	fault_led = 1;
      if (l == 0)
	printf(" %c |", fault_led ? '*' : ' ');
      else 
      if (l == 1 || l == 2)
	switch(drv->status) {
	case STS_OK:
	  printf(" %c |", l==1 ? 'O' : 'K'); break;
	case STS_OFF:
	case STS_NOT_PRESENT:
	  printf(" %c |", l==1 ? ' ' : ' '); break;
	case STS_FAILED:
	  printf(" %c |", l==1 ? 'F' : 'L'); break;
	case STS_BYPASSED:
	  printf(" %c |", l==1 ? 'B' : 'P'); break;
	default:
	  printf(" %c |", l==1 ? '?' : '?'); break;
	}
      else {
	if (drv->phys_id < 100)
	  printf(" %-2d|", drv->phys_id);
	else
	  printf("%3d|", drv->phys_id);
      }
    }
    switch (l) {
    case 0:
      printf(" P    |\n");
      break;
    case 1:
    case 2:
      printf(" E  L |\n");
      break;
    case 3:
      printf(" R  C |\n");
      break;
    }
  }

  {
    fan_e_t fan = (fan_e_t) out_stat->fan_val[0].fan_stat_t_val;
    ps_e_t ps0 = (ps_e_t) out_stat->ps_val[0].ps_stat_t_val;
    ps_e_t ps1 = (ps_e_t) out_stat->ps_val[1].ps_stat_t_val;
    lcc_e_t lcc = (lcc_e_t) out_stat->lcc_val[0].lcc_stat_t_val;

    printf("  +---------------------------------------+      |\n");
    printf("  |               FANS %4s               | %4s |\n",
	   fanstat2str(fan->status),
	   (lcc->peer_present ? 
	    (lcc->peer_failed ? lccstat2str(STS_FAILED) : lccstat2str(STS_OK)) : 
	    lccstat2str(STS_NOT_PRESENT)));
    printf("  |      PS0 %4s           PS1 %4s      |      |\n", 
	   psstat2str(ps0->status), psstat2str(ps1->status));
    printf("  +---------------------------------------+------+\n");
  }


#if 0
    printf("  Peer LCC:       ");
    for (k = 0; k < out_stat->lcc_len; ++k) {
      printf("%-7d ", k);
    }
    printf("\n");
    printf("                  ");      
    for (k = 0; k < out_stat->lcc_len; ++k) {
      lcc_e_t lcc = (lcc_e_t) out_stat->lcc_val[k].lcc_stat_t_val;
      if (lcc->peer_present) {
	if (lcc->peer_failed)
	  printf("%-7s ", stat2str(STS_FAILED));
	else
	  printf("%-7s ", stat2str(STS_OK));
      }
      else
	printf("%-7s ", stat2str(STS_NOT_PRESENT));
    }
    printf("\n");
  }
#endif

  return(0);
}

int fccli_remove()
{
  drive_op_t dop;
  fcid_bitmap_t bm;
  int *result;
  int i;
      
  /*
   * Verify channel ID exists
   */
  if (channel_exists(CH_ID) == 0) {
    printf("Channel %d does not exist\n", CH_ID);
    return(0);
  }

  dop.ch_id = CH_ID;
  dop.op = 0;

  dop.fcid_bm.fcid_t_val = FCID_NEW();
  dop.fcid_bm.fcid_t_len = sizeof(fcid_bitmap_struct_t);
  bm = (fcid_bitmap_t)dop.fcid_bm.fcid_t_val;

  for (i = 0; i < T_COUNT; ++i) {
    FCID_SET(bm, T_ID[i]);
  }

  result = agent_set_drv_remove_1(&dop, CL);
  if (result == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (*result) {
    if (*result < 0)
      printf("REMOVE failed for device %d\n", -*result);
    else
      printf("REMOVE Failed\n");
  }
  return(0);
}

int fccli_insert()
{
  drive_op_t dop;
  fcid_bitmap_t bm;
  int *result;
  int i;
      
  /*
   * Verify channel ID exists
   */
  if (channel_exists(CH_ID) == 0) {
    printf("Channel %d does not exist\n", CH_ID);
    return(0);
  }

  dop.ch_id = CH_ID;
  dop.op = 0;

  dop.fcid_bm.fcid_t_val = FCID_NEW();
  dop.fcid_bm.fcid_t_len = sizeof(fcid_bitmap_struct_t);
  bm = (fcid_bitmap_t)dop.fcid_bm.fcid_t_val;

  for (i = 0; i < T_COUNT; ++i) {
    FCID_SET(bm, T_ID[i]);
  }

  result = agent_set_drv_insert_1(&dop, CL);
  if (result == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (*result) {
    if (*result < 0)
      printf("INSERT failed for device %d\n", -*result);
    else
      printf("INSERT Failed\n");
  }
  return(0);
}

int fccli_ledon()
{
  drive_op_t dop;
  fcid_bitmap_t bm;
  int *result;
  int i;
      
  /*
   * Verify channel ID exists
   */
  if (channel_exists(CH_ID) == 0) {
    printf("Channel %d does not exist\n", CH_ID);
    return(0);
  }

  dop.ch_id = CH_ID;
  dop.op = 1;

  dop.fcid_bm.fcid_t_val = FCID_NEW();
  dop.fcid_bm.fcid_t_len = sizeof(fcid_bitmap_struct_t);
  bm = (fcid_bitmap_t)dop.fcid_bm.fcid_t_val;

  for (i = 0; i < T_COUNT; ++i) {
    FCID_SET(bm, T_ID[i]);
  }

  result = agent_set_drv_led_1(&dop, CL);
  if (result == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (*result) {
    if (*result < 0)
      printf("LEDON failed for device %d\n", -*result);
    else
      printf("LEDON Failed\n");
  }
  return(0);
}

int fccli_ledoff()
{
  drive_op_t dop;
  fcid_bitmap_t bm;
  int *result;
  int i;
      
  /*
   * Verify channel ID exists
   */
  if (channel_exists(CH_ID) == 0) {
    printf("Channel %d does not exist\n", CH_ID);
    return(0);
  }

  dop.ch_id = CH_ID;
  dop.op = 0;

  dop.fcid_bm.fcid_t_val = FCID_NEW();
  dop.fcid_bm.fcid_t_len = sizeof(fcid_bitmap_struct_t);
  bm = (fcid_bitmap_t)dop.fcid_bm.fcid_t_val;

  for (i = 0; i < T_COUNT; ++i) {
    FCID_SET(bm, T_ID[i]);
  }

  result = agent_set_drv_led_1(&dop, CL);
  if (result == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (*result) {
    if (*result < 0)
      printf("LEDOFF failed for device %d\n", -*result);
    else
      printf("LEDOFF Failed\n");
  }
  return(0);
}

int fccli_flash()
{
  drive_op_t dop;
  fcid_bitmap_t bm;
  int *result;
  int i;
      
#if 0
  /*
   * Verify channel ID exists
   */
  if (channel_exists(CH_ID) == 0) {
    printf("Channel %d does not exist\n", CH_ID);
    return(0);
  }

  dop.ch_id = CH_ID;
  dop.op = 2;

  dop.fcid_bm.fcid_t_val = FCID_NEW();
  dop.fcid_bm.fcid_t_len = sizeof(fcid_bitmap_struct_t);
  bm = (fcid_bitmap_t)dop.fcid_bm.fcid_t_val;

  for (i = 0; i < T_COUNT; ++i) {
    FCID_SET(bm, T_ID[i]);
  }

  result = agent_set_drv_led_1(&dop, CL);
  if (result == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (*result) {
    if (*result < 0)
      printf("FLASH failed for device %d\n", -*result);
    else
      printf("FLASH Failed\n");
  }
#else
  printf("Sorry. FLASH command not implemented yet.\n");
#endif
  return(0);
}

int fccli_bypass()
{
  drive_op_t dop;
  fcid_bitmap_t bm;
  int *result;
  int i, j;

  /*
   * Verify channel ID exists
   */
  if (channel_exists(CH_ID) == 0) {
    printf("Channel %d does not exist\n", CH_ID);
    return(0);
  }

  /*
   * Check whether this is an allowed bypass
   */
  for (i = 0; i < T_COUNT; ++i) {   
    enc_stat_t *encl_stat;
    int connect_port=-1;
    drv_e_t drv[10];
    int t_id = T_ID[i]%10;
    int illegal_bypass = 0;

    if (target_exists(CH_ID, T_ID[i], &encl_stat)) {
      for (j = 0; j < encl_stat->drv_len; ++j) {
	drv[j] = (drv_e_t) encl_stat->drv_val[j].drv_stat_t_val;
	if (drv[j]->status == STS_OK)
	  connect_port = drv[j]->connect_port;
      }
	
      switch (connect_port) {
      case 0:
	if (t_id != 0 && t_id != 2)
	  goto bypass_check;	  
	if ((t_id == 0) && (drv[0]->status == STS_OK) && (drv[2]->status != STS_OK)) {
	  illegal_bypass = 1;
	  goto bypass_check;
	}
	if ((t_id == 2) && (drv[2]->status == STS_OK) && (drv[0]->status != STS_OK)) {
	  illegal_bypass = 1;
	  goto bypass_check;
	}
	break;
      case 1:
	if (t_id != 1 && t_id != 3)
	  goto bypass_check;
	if ((t_id == 1) && (drv[1]->status == STS_OK) && (drv[3]->status != STS_OK)) {
	  illegal_bypass = 1;
	  goto bypass_check;
	}
	if ((t_id == 3) && (drv[3]->status == STS_OK) && (drv[1]->status != STS_OK)) {
	  illegal_bypass = 1;
	  goto bypass_check;
	}
	break;
      default:
	printf("BYPASS failed for device %d\n", T_ID[i]);
	return(0);
      }
    }
    else {
      printf("Target %d does not exist on channel %d\n", T_ID[i], CH_ID);
      return(0);
    }
  bypass_check:
    if (illegal_bypass) {
      printf("BYPASS failed for device %d: Disallowed operation\n", T_ID[i]);
      return(0);
    }
  } /* for (i) */

  dop.ch_id = CH_ID;
  dop.op = 1;			/* 1 = enabled bypass */

  dop.fcid_bm.fcid_t_val = FCID_NEW();
  dop.fcid_bm.fcid_t_len = sizeof(fcid_bitmap_struct_t);
  bm = (fcid_bitmap_t)dop.fcid_bm.fcid_t_val;

  for (i = 0; i < T_COUNT; ++i) {
    FCID_SET(bm, T_ID[i]);
  }

  result = agent_set_drv_bypass_1(&dop, CL);
  if (result == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (*result) {
    if (*result < 0)
      printf("BYPASS failed for device %d\n", -*result);
    else
      printf("BYPASS Failed\n");
  }
  return(0);
}

int fccli_unbypass()
{
  drive_op_t dop;
  fcid_bitmap_t bm;
  int *result;
  int i;
      
  /*
   * Verify channel ID exists
   */
  if (channel_exists(CH_ID) == 0) {
    printf("Channel %d does not exist\n", CH_ID);
    return(0);
  }

  dop.ch_id = CH_ID;
  dop.op = 0;			/* 0 = disable bypass */

  dop.fcid_bm.fcid_t_val = FCID_NEW();
  dop.fcid_bm.fcid_t_len = sizeof(fcid_bitmap_struct_t);
  bm = (fcid_bitmap_t)dop.fcid_bm.fcid_t_val;

  for (i = 0; i < T_COUNT; ++i) {
    FCID_SET(bm, T_ID[i]);
  }

  result = agent_set_drv_bypass_1(&dop, CL);
  if (result == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (*result) {
    if (*result < 0)
      printf("UNBYPASS failed for device %d\n", -*result);
    else
      printf("UNBYPASS Failed\n");
  }
  return(0);
}

char *stat2str(int stat)
{
  switch (stat) {
  case STS_OK:
    return("OK");
  case STS_OFF:
    return("OFF");
  case STS_FAILED:
  case STS_PEER_FAILED:
    return("FAIL");
  case STS_NOT_PRESENT:
    return("OUT");
  case STS_BYPASSED:
    return("BYPASS");
  default:
    return("UNKNOWN");
  }
}

char *fanstat2str(int stat)
{
  switch (stat) {
  case STS_OK:
    return(" OK ");
  case STS_OFF:
    return(" OFF");
  case STS_FAILED:
    return("FAIL");
  case STS_NOT_PRESENT:
    return(" OUT");
  default:
    return("????");
  }
}

char *psstat2str(int stat)
{
  switch (stat) {
  case STS_OK:
    return(" OK ");
  case STS_OFF:
    return(" OFF");
  case STS_FAILED:
    return("FAIL");
  case STS_NOT_PRESENT:
    return(" OUT");
  default:
    return("????");
  }
}

char *lccstat2str(int stat)
{
  switch (stat) {
  case STS_OK:
    return(" OK ");
  case STS_OFF:
    return(" OFF");
  case STS_FAILED:
  case STS_PEER_FAILED:
    return("FAIL");
  case STS_NOT_PRESENT:
    return(" OUT");
  default:
    return("????");
  }
}

int channel_exists(int ch_id)
{
  cfg_t *result_config;
  enc_addr_t in_addr;
  enc_stat_t *out_stat;
  int i;

  /* 
   * Get configuration
   */
  result_config = agent_get_config_1(NULL, CL);
  if (result_config == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (result_config->cfg_t_len == 0)
    return(0);

  for (i = 0; i < result_config->cfg_t_len; ++i) {
    if (result_config->cfg_t_val[i].ch_id == ch_id)
      return(1);
  }
  return(0);
}

int target_exists(int ch_id, int t_id, enc_stat_t **out_stat)
{
  cfg_t *result_config;
  enc_addr_t in_addr;
  int i, j;

  /* 
   * Get configuration
   */
  result_config = agent_get_config_1(NULL, CL);
  if (result_config == NULL) {
    clnt_perror(CL, HOSTNAME);
    return(-1);
  }
  if (result_config->cfg_t_len == 0) {
    printf("No channels found.\n");
    return(0);
  }

  /*
   * Get channel status
   */
  for (i = 0; i < result_config->cfg_t_len; ++i) {
    if (result_config->cfg_t_val[i].ch_id == ch_id) 
      for (j = 0; j < result_config->cfg_t_val[i].encl_id_len; ++j) {
	in_addr.ch_id = ch_id;
	in_addr.encl_id = result_config->cfg_t_val[i].encl_id_val[j];
	*out_stat = agent_get_status_1(&in_addr, CL);
	if (*out_stat == NULL) {
	  clnt_perror(CL, HOSTNAME);
	  return(0);
	}
	if ((*out_stat)->encl_phys_id == t_id/10)
	  return(1);
      }
  }
  return(0);
}
