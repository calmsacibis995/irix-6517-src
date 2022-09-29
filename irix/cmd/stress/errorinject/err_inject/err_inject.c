#include <stdio.h>
#include <stdlib.h>
#include <sys/syssgi.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/SN/error_force.h>
#include <sys/pda.h>
#include <assert.h>

char *user_fileName = NULL;

#define DEFAULT_EXERCISE "io.txt"
#define IO_ITERS 200

#define FILE_SIZE ((1024*16) * IO_ITERS)

#define DMA_READ_ACCESS 0
#define DMA_WRITE_ACCESS 1

#define NO_CORRUPTION 0
#define CORRUPT_MEM 1
#define PROTECT_MEM 2
#define SET_MEM_STATE 3

#define NO_SUBCODE 'x'
#define MD_DIR_UNDEF 8

#define NODE_CHOICE 0
#define WIDGET_CHOICE 1
#define MODULE_CHOICE 2
#define SLOT_CHOICE 3
#define TABLE_CHOICE 4
#define CODE_CHOICE 5
#define MAX_CHOICE 6

#define ERR_TABLE_ENTRY(_table,_code,_subcode,_label) { _table,_code,_subcode,_label }
#define CHOICE_TABLE_ENTRY(_prompt,_init_val)         { _prompt,0,_init_val }

#define GET_CHOICE(_choice) \
     ((_choice >= 0 && _choice < MAX_CHOICE) ? &choice_table[_choice] : NULL)


#define SELECT_DEV_WITH_CHECK(_params)  \
        { if (select_dev(_params) == -1) return -1; }
#define SELECT_NODE_WITH_CHECK(_node) \
        { if (assign_param(NODE_CHOICE,_node) == -1) return -1; }
#define SELECT_NODE_N_WIDGET_WITH_CHECK(_node, _widget) \
        { if (assign_param(NODE_CHOICE,_node) || assign_param(WIDGET_CHOICE,_widget)) \
              return -1; }  



typedef struct choice_entry_s
{
  char *prompt_msg;
  int chosen;
  int value;
} choice_entry_t;


choice_entry_t choice_table [] = 
{

  CHOICE_TABLE_ENTRY("Select Node: ",CNODEID_NONE),
  CHOICE_TABLE_ENTRY("Select Widget: ",0),
  CHOICE_TABLE_ENTRY("Select Module: ",0),
  CHOICE_TABLE_ENTRY("Select Slot: ",0),
  CHOICE_TABLE_ENTRY("Select Table: ",0),
  CHOICE_TABLE_ENTRY("Select Code: ",0),
};


typedef enum input_mode {
  UNDEF_INPUT =0,
  FILE_INPUT,
  INTERACTIVE_INPUT,
  COMMAND_LINE_INPUT
} input_mode_t;

input_mode_t input_mode = UNDEF_INPUT;
FILE *input_file = NULL;

typedef struct err_entry_s 
{
  int err_table;
  int err_code;
  char err_subcode;
  char *err_label;
} err_entry_t;

err_entry_t table19_errors [] = 
{
  ERR_TABLE_ENTRY(19,1,NO_SUBCODE, "Illegal Read Access (no access in IIWA)"),
  ERR_TABLE_ENTRY(19,3,NO_SUBCODE, "Error Bit set in Crosstalk Header (Request)"),
  ERR_TABLE_ENTRY(19,6,NO_SUBCODE, "Illegal Write Access (no access in IIWA)"),
  ERR_TABLE_ENTRY(19,7,NO_SUBCODE, "Error Bit set in Crosstalk Header and/or Sideband"),
  ERR_TABLE_ENTRY(19,10,NO_SUBCODE, "Timeout on PIO Read"),
  ERR_TABLE_ENTRY(19,11,NO_SUBCODE, "Unsolicited Read Response on Crosstalk"),
  ERR_TABLE_ENTRY(19,12,NO_SUBCODE, "Error Bit set in Crosstalk Header (Response)"),
  ERR_TABLE_ENTRY(19,14,NO_SUBCODE, "Unsolicited Write Response on Crosstalk"),
  ERR_TABLE_ENTRY(19,16,NO_SUBCODE, "Link Reset from Crosstalk"),
  ERR_TABLE_ENTRY(19,0,NO_SUBCODE, NULL),
};

err_entry_t table20_errors [] = 
{
  ERR_TABLE_ENTRY(20,1,NO_SUBCODE, 
		  "AERR (Access Error) response on II Uncac Rd Shared (non-BTE)"),	
  ERR_TABLE_ENTRY(20,2,NO_SUBCODE, "TimeOut response on II UncacRd Shared (non-BTE)"),
  ERR_TABLE_ENTRY(20,3,NO_SUBCODE, "UCE response on II Uncac Rd Shared (non-BTE)"),
  ERR_TABLE_ENTRY(20,6,'a', "AERR response on II BTE Pull"),  
  ERR_TABLE_ENTRY(20,6,'b', "DERR response on II BTE Pull"),  
  ERR_TABLE_ENTRY(20,6,'c', "PERR response on II BTE Pull"),  
  ERR_TABLE_ENTRY(20,7,NO_SUBCODE, "TimeOut response on II BTE Pull"),		  
  ERR_TABLE_ENTRY(20,8,NO_SUBCODE, "UCE response on II BTE Pull "),
  ERR_TABLE_ENTRY(20,11,NO_SUBCODE, "AERR response on II Rd Exclusive"),
  ERR_TABLE_ENTRY(20,12,NO_SUBCODE, "TimeOut response on II Rd Exclusive"),
  ERR_TABLE_ENTRY(20,13,NO_SUBCODE, "UCE response on II Rd Exclusive"),
  ERR_TABLE_ENTRY(20,16,NO_SUBCODE, "WERR on II Wr Access (non-BTE)"),
  ERR_TABLE_ENTRY(20,17,NO_SUBCODE, "TimeOut on II Wr Access (non-BTE)"),	    
  ERR_TABLE_ENTRY(20,18,'a', "PERR on II BTE Push"),  
  ERR_TABLE_ENTRY(20,18,'b', "WERR on II BTE Push"),        
  ERR_TABLE_ENTRY(20,19,NO_SUBCODE, "TimeOut on II BTE Push"),  
  ERR_TABLE_ENTRY(20,23,NO_SUBCODE, "Illegal SN1Net PIO access (No access in IOWA)"), 
  ERR_TABLE_ENTRY(20,24,NO_SUBCODE, "II or widget registers (No access in ILAPR)"),
  ERR_TABLE_ENTRY(20,25,NO_SUBCODE, 
		  "Valid PIO access encountering the ERROR bit set in the PRB"),  
  ERR_TABLE_ENTRY(20,29,NO_SUBCODE, "Crosstalk credit time out"),  
  ERR_TABLE_ENTRY(20,30,NO_SUBCODE, "Warm Reset from CPU"),  
  ERR_TABLE_ENTRY(20,31,NO_SUBCODE, 
		  "Spurious Message Received (CRB Table doesn't hit any line)"),
  ERR_TABLE_ENTRY(20,0,NO_SUBCODE, NULL) 

};

extern int errno;


/* Useful for debugging, it prints the contents of the error
   parameters */
void
print_params(io_error_params_t *params)
{
  printf("Param Contents:\n"
         "---------------\n"
         "Sub_code: 0x%llx\n"
         "Node:     %lld\n"
         "Widget:   0x%llx\n"
         "Module:   %lld\n"
         "Slot:     %lld\n"
         "usr_def1: %lld\n"
         "usr_def2: %lld\n",
         params->sub_code,
         params->node,
         params->widget,
         params->module,
         params->slot,
         params->usr_def1,
         params->usr_def2);
      
}

/* Just prints one error entry to the user */
void
print_entry(err_entry_t *entry)
{
  printf("\t%d%c\t%s\n",entry->err_code,
	 entry->err_subcode == NO_SUBCODE ? ' ' : entry->err_subcode,
	 entry->err_label);
}	

/* Prints all of the errors that the program can generate */
void 
print_tables()
{
  int ii;
  printf("\n\t\t\tTable 19\n\n");

  for (ii = 0; table19_errors[ii].err_label; ii++) {
    print_entry(&table19_errors[ii]);
  }

  printf("\n\t\t\tTable 20\n\n");

  for (ii=0; table20_errors[ii].err_label; ii++) {
    print_entry(&table20_errors[ii]);
  }

  printf("\n\n\t\t\t\t\t\tBrought to you by Batman...\n\n");
}

/* Will return an error entry given table, code, and sub_code Returns
a NULL if it cannot be found */

err_entry_t*
find_entry(int table_code, int code, char subcode)
{

  err_entry_t *table;
  int ii;

  switch(table_code) {
  case 19:
    table = table19_errors;
    break;
  case 20:
    table = table20_errors;
    break;
  default:
    printf("Unknown Table lookup. Table %d\n",table_code);
    return NULL;
  }

  
  for (ii=0; table[ii].err_label; ii++) {

    if (table[ii].err_table == table_code && table[ii].err_code == code &&
	table[ii].err_subcode == subcode) {
      return &table[ii];
    }

  }

  return NULL;
  

}

int 
str_to_num(char *buff,int *parameter)
{
  int length,ii;
  int all_safe = 1;

  /* Make sure all the values are valid digits */
  length = strlen(buff);
  
  /* First check for a decimal number */
  for (ii=0; ii < length; ii++) {
    if (!isdigit(buff[ii]) && !isspace(buff[ii])) {
      all_safe = 0;
      break;
    }
  }
  
  /* We have a decimal! */
  if (all_safe) {
    *parameter = atoi(buff);
    return 0;
  }

  /* If not try for a hex number */
  for (ii=0; ii < length; ii++) {
    /* If it has any non-hex numbers it is invalid */
    if (!isxdigit(buff[ii]) && buff[ii] != 'x' && buff[ii] != 'X' &&
        !isspace(buff[ii])) {
      printf("Not a valid decimal or hex number\n");
      return -1;
    }
  }

  *parameter = strtol(buff,NULL,0x10);

  return 0;
}


int 
user_assign_param(int choice_idx) 
{
  char buff[1024];
  char *end;
  choice_entry_t *choice = GET_CHOICE(choice_idx);
  if (choice == NULL) {
    return -1;
  }

  /* Get the user input */
  printf("%s",choice->prompt_msg);
  fgets(buff,1024,stdin);

  /* Terminate the string at the number */
  end = strchr(buff,'\n');
  if (end) {
    *end = NULL;
  }

  return str_to_num(buff,&choice->value);

}

int 
assign_param(int choice_idx, __uint64_t *param) 
{

  choice_entry_t *choice = GET_CHOICE(choice_idx);
  
  if (choice == NULL) {
    return -1;
  }
  
  /* If we have not chosen yet ask the user */
  if (!choice->chosen) {
    if (user_assign_param(choice_idx) == -1) {
      return -1;
    }	
  }

  /* It is chosen already */
  *param = choice->value;

  return 0;

}


char 
select_choice_style()
{

  char buff[1024];

  /* First check the choice table */
  if (choice_table[NODE_CHOICE].chosen) {
    return 'n';
  }

  if (choice_table[MODULE_CHOICE].chosen && choice_table[SLOT_CHOICE].chosen) {
    return 'm';
  }

 /* Now check user input */
  printf("\n\tDevice Selection\n"
	 "\t----------------\n"
	 "[M]odule & Slot\n"
	 "[N]ode & Widget\n\n"
	 "Choice : "
	 );

  fflush(stdout);

  fgets(buff,1024,stdin);

  if (strchr(buff,'m') || strchr(buff,'M')) {
    return 'm';
  }

  if (strchr(buff,'n') || strchr(buff,'N')) {
    return 'n';
  } 
    
  printf("Invalid choice\n");
  return '\0';

}

/* 
 Often the user wants to select one device for a simple test, this
 function will ask the user by what manner do they want to select the
 device (node/widget or module/slot), and then store the user's choice
 in the error params to be passed to the kernel. 
*/

int
select_dev(io_error_params_t *params) { 

  char buff[1024];
  char choice;
  /* Needed for initialization checks in kernel */
  params->node = (cnodeid_t)-1;
  
  
  switch(select_choice_style())
    {
    case 'm':
      if (assign_param(MODULE_CHOICE,&params->module) || 
	  assign_param(SLOT_CHOICE,&params->slot)) {
	return -1;
      }
      break;
	
    case 'n':
      SELECT_NODE_N_WIDGET_WITH_CHECK(&params->node,&params->widget);
      break;
    default:
      printf("Invalid choice\n");
      return -1;
    }

  return 0;

}

int 
corrupt_mem(int code, char *buff,int state) 
{
  md_error_params_t md_params;
  
  bzero(&md_params,sizeof(md_error_params_t));
  switch (code) {
  case NO_CORRUPTION:
    return MD_DIR_UNDEF;
  case SET_MEM_STATE:
    md_params.sub_code = ETYPE_SET_DIR_STATE;
    md_params.mem_addr = (__uint64_t)buff;
    md_params.flags = ERRF_USER;
    md_params.usr_def = state;
    break;
  case CORRUPT_MEM:
    md_params.sub_code = ETYPE_MEMERR;
    md_params.mem_addr = (__uint64_t)buff;
    md_params.flags = ERRF_USER;
    md_params.read_write = ERRF_NO_ACTION;
    break;
  case PROTECT_MEM:
    md_params.sub_code = ETYPE_PROT;
    md_params.mem_addr = (__uint64_t)buff;
    md_params.flags = ERRF_USER;
    md_params.read_write = ERRF_NO_ACTION;
    break;
  default:
    printf("Unknown Corruption code\n");
    return -1;
  }
#ifndef KERNEL_DISABLE
  if ((state = syssgi(SGI_ERROR_FORCE,md_params)) == -1) {
    printf("Corruption Error: Syssgi failed\n");
    return -1;
  }
#endif
  printf("State: %d\n",state);
  return state;

}

  /* We need to know if the file exists before we start mesisng with
     the kernel. Otherwise we disrupt the kernel for no reason, since
     the test will fail. */
int 
fileReady(char *filename)
{
  char buff[FILE_SIZE];
  FILE *targetFile; 
  /* If not create it */
  if((targetFile = fopen(filename,"wb")) == NULL) {
    printf("Cannot open file\n");
    return 0;
  }
  
  /* Make it big */
  memset(buff,NO_SUBCODE,FILE_SIZE);
  
  if (fwrite(buff,FILE_SIZE,1,targetFile) == 0) {
    printf("Cannot write to file\n");
    fclose(targetFile);
    return 0;
  }

  /* close it up */
  fclose(targetFile);
  return 1;
  

}

int 
dma_access(int access, int corrupt_code)
{
  int retVal;

  char exercise_file_space[10240];
  char *exercise_file = exercise_file_space;
  int ii,exerFile;
  int loop_limit = IO_ITERS;
  struct dioattr ioCntl;
  char *buff;
  int open_mask = O_RDWR | O_DIRECT;
  int state=0;

  memset(exercise_file,0,10240);
  if(input_mode == INTERACTIVE_INPUT && user_fileName == NULL) {
    printf("Select file for dma execution [%s]:",DEFAULT_EXERCISE);
    exercise_file = gets(exercise_file);
    printf("\n");
  } 

  if(user_fileName) {
    strcpy(exercise_file,user_fileName);
  }

  if (exercise_file == NULL || exercise_file[0] == NULL) {
    exercise_file = exercise_file_space;
    strcpy(exercise_file,DEFAULT_EXERCISE);
  }

  printf("File Name is %s\n",exercise_file);
  
  if (!fileReady(exercise_file)) {
    printf("Cannot create %s file\n",exercise_file);
    return -1;
  }	
  
  exerFile = open(exercise_file,open_mask,0666);
  if (exerFile == -1) {
    printf("Cannot open file\n");
    return -1;
  }
  
  retVal = fcntl(exerFile, F_DIOINFO,&ioCntl);
  assert(retVal == 0);

  buff = (char*)memalign(ioCntl.d_mem,ioCntl.d_maxiosz);
  
  for (ii=0; ii < ioCntl.d_maxiosz; ii++) {
    buff[ii] = NO_SUBCODE;
  }

  if ((state = corrupt_mem(corrupt_code,buff,state)) != -1) {

    for (ii = 0; ii < loop_limit; ii++) {
      /* Executing an uncached write to get a DMA READ */
      if (access == DMA_READ_ACCESS) {
	retVal = write(exerFile,buff,ioCntl.d_miniosz);
      } else {

	assert(access == DMA_WRITE_ACCESS);
	retVal = read(exerFile,buff,ioCntl.d_miniosz);
      }
      /* Do that error check */
      if (retVal == -1) {
	perror("PERROR:");
	return -1;
      }	
    }

  }	

  if (state != MD_DIR_UNDEF) {
    corrupt_mem(SET_MEM_STATE,buff,state);
  }
  
  if (close(exerFile)) {
      perror("PERROR:Exercise Close");
      return -1;
  }

  free (buff);

  return 0;
}

int
execute_with_dma(io_error_params_t *params,__uint64_t code,int direction)
{

  params->sub_code = code;

#ifndef KERNEL_DISABLE
  if(syssgi(SGI_ERROR_FORCE,*params) == -1) {
    return -1;
  }
#endif	

  print_params(params);

  if(dma_access(direction,NO_CORRUPTION) == -1) {
    return -1;
  }

  return 0;
}

int 
execute_error(err_entry_t *choice)
{
  error_params_t params;

  /* clear out our parameters */
  bzero(&params,sizeof(error_params_t));

  switch(choice->err_table) {

    /* This is the subcodes or actions for the errors in table 19 */
  case 19:
    switch(choice->err_code) {
    case 1:
      {
	/* 1. Clear the devices's access bit in the HUb's II_IIWA register.
	   2. Initiate a partial Xtalk read from disabled device */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	return execute_with_dma(&params.io_params,ETYPE_CLR_READ_ACCESS,
				DMA_READ_ACCESS); 
      }
    case 3:
      {
	/* Purpose : Initiate PIO write to nonexistent widget.*/
	SELECT_DEV_WITH_CHECK(&params.io_params);
	params.io_params.sub_code = ETYPE_XTALK_HDR_REQ;
      }
    break;
    case 6:
      {
	/* 1. Clear the devices's access bit in the HUb's II_IIWA register.
	   2. Initiate a partial XTalk write from disabled device */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	return execute_with_dma(&params.io_params,ETYPE_CLR_READ_ACCESS,
				DMA_WRITE_ACCESS); 
      }
    case 7:
      {
	params.io_params.sub_code = ETYPE_XTALK_SIDEBAND;
      }
    break;
    case 10:
      {
	/* PIO read of an absent widget  */
	params.io_params.sub_code = ETYPE_ABSENT_WIDGET;
      }
    break;
    case 11:
      {
	/* This is sneaky. It involves forging the Widget_ID of on
	   widget into another. Then reading from that widget. This
	   will create a seemingly unsolicited response */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	params.io_params.sub_code = ETYPE_FORGE_ID_READ;
      }
    break;
    case 12:
      {
	/* Purpose : Initiate PIO write to nonexistent widget after
	 enabling NAKs in XTalk*/
	SELECT_DEV_WITH_CHECK(&params.io_params);
	params.io_params.sub_code = ETYPE_XTALK_HDR_RSP;
      }
    break;
    case 14:
      {
	/* This is sneaky. It involves forging the Widget_ID of on
	   widget into another. Then writing to that widget. This
	   will create a seemingly unsolicited response */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	params.io_params.sub_code = ETYPE_FORGE_ID_WRITE;
      }
    break;
    case 16:
      {
	 /* Warm reset by writng to the LINK_X_RESET register for Origin 2000
	    and resetting the hub for the Speedo */
	SELECT_NODE_N_WIDGET_WITH_CHECK(&params.net_params.node, 
					&params.net_params.port);
	params.io_params.sub_code = ETYPE_LINK_RESET;
      }
    break;
    default:
      printf("Unknown error 19.%d\n",choice->err_code);
      return -1;
    }
  break;

  case 20:
    switch(choice->err_code) {
    case 1:
      {
	/* Initiate a partial read from XTalk to non-populated memory. */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	return execute_with_dma(&params.io_params,ETYPE_XTALK_USES_NON_POP_MEM,
				DMA_READ_ACCESS);
      }
    case 2:
      {
	/* Initiate a partial read from XTalk to a nonexistent node. */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	return execute_with_dma(&params.io_params,ETYPE_XTALK_USES_ABSENT_NODE,
				DMA_READ_ACCESS);
      }
    case 3:
      {
	/* An Xtalk read of corrupted Memory */
	return dma_access(DMA_READ_ACCESS,CORRUPT_MEM);
      }
    case 6:
      switch(choice->err_subcode) {
      case 'a':
	{
	  /* A BTE pull of non populated memory */
	  SELECT_NODE_WITH_CHECK(&params.mem_params.usr_def);
	  params.io_params.sub_code = ETYPE_AERR_BTE_PULL;
	}
      break;
      case 'b':
	{
	  /* A BTE pull of memory whose BDDIR is corrupted */
	  SELECT_NODE_WITH_CHECK(&params.mem_params.usr_def);
	  params.io_params.sub_code = ETYPE_DERR_BTE_PULL;
	}
      break;
      case 'c':
	{
	  /* A BTE pull of poisoned memory */
	  params.io_params.sub_code = ETYPE_PERR_BTE_PULL;
	}
      break;
      }	
      break;
    case 8:
      {
	/*  A BTE of corrupted memory */
	SELECT_NODE_WITH_CHECK(&params.mem_params.usr_def);
	params.io_params.sub_code = ETYPE_CORRUPT_BTE_PULL;
      }
    break;
    case 11:
      {
	/* Initiate a partial WRITE from XTalk to non-populated memory. */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	return execute_with_dma(&params.io_params,ETYPE_XTALK_USES_NON_POP_MEM,
				DMA_WRITE_ACCESS);
      }
    case 12:
      {
	/* Initiate a partial read from XTalk to a nonexistent node. */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	return execute_with_dma(&params.io_params,ETYPE_XTALK_USES_ABSENT_NODE,
				DMA_WRITE_ACCESS);
      }
    case 13:
      {
	/* Xtalk write to corrupted memory */
	return dma_access(DMA_WRITE_ACCESS,CORRUPT_MEM);
      }
    case 16:
      {
	/* An Xtalk write to protected memory */
	return dma_access(DMA_WRITE_ACCESS,PROTECT_MEM);
      }
    case 18:
      switch(choice->err_subcode) {
      case 'a':
	{
	  /* A BTE push of poisoned memory */
	  params.io_params.sub_code = ETYPE_PERR_BTE_PUSH;
	}
      break;
      case 'b': 
	{	
	  /* A BTE push of memory whose BDDIR information is corrupted */
	  SELECT_NODE_WITH_CHECK(&params.mem_params.usr_def);
	  params.io_params.sub_code = ETYPE_WERR_BTE_PUSH;
	}
      break;
      }
      break;
    case 19:
      {
	/* A BTE push to a memory on a nonexistent node */
	params.io_params.sub_code = ETYPE_NO_NODE_BTE_PUSH;
      }
    break;
    case 23:
      {      
	/* Here we clear the II_IOWA register for the appropriate
           widget, then do a PIO write. */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	params.io_params.sub_code = ETYPE_CLR_OUTBND_ACCESS;
      }
    break;
    case 24:
      {
	/* Purpose : Disable II_ILAPR and then execute a PIO write */
	SELECT_DEV_WITH_CHECK(&params.io_params);
	params.io_params.sub_code = ETYPE_CLR_LOCAL_ACCESS;
      }
    break;
    case 25:
      {
	/* Purpose : Initiate PIO write to nonexistent widget. Twice.*/
	SELECT_DEV_WITH_CHECK(&params.io_params);
	params.io_params.sub_code = ETYPE_PIO_N_PRB_ERR;
      }
    break;
    case 29:
      {
	SELECT_DEV_WITH_CHECK(&params.io_params);
	params.io_params.sub_code = ETYPE_XTALK_CREDIT_TIMEOUT;
      }
    break;
    case 30:
      {
	 /* Warm reset by writng to the LINK_X_RESET register for Origin 2000
	    and resetting the hub for the Speedo */
	SELECT_NODE_N_WIDGET_WITH_CHECK(&params.net_params.node, 
					&params.net_params.port);
	params.io_params.sub_code = ETYPE_LINK_RESET;
      }
    break;
    case 31:
    {
	SELECT_NODE_WITH_CHECK(&params.io_params.node);
	return execute_with_dma(&params.io_params,ETYPE_SET_CRB_TIMEOUT,
				DMA_READ_ACCESS);
    }
    case 7:
    case 17:
      printf("This is error is not implemented yet. Go bug mhr\n");
      return 0;
    default:
      printf("Unknown Table 20 error: %d\n",choice->err_code);
      return -1;
    }
    break;

  default:
    printf("Unknown Table %d\n",choice->err_table);
    return -1;

  }	

  print_params(&params.io_params);

#ifndef KERNEL_DISABLE
  /* Now we actually do the work in the kernel */
  if (syssgi(SGI_ERROR_FORCE,params) == -1) {
    return -1;
  }
#endif
  return 0;

}

void 
usage()
{
  printf("        err_inject:\n"
	 "---------------------------\n"
	 "-c <table> <code> <subcode>\n"
	 "-i (for interactive mode)\n"
	 "-l (list errors)\n"
	 "-f <dma_file>\n"
	 "-n <node>\n"
	 "-w <widget>\n"
	 "-m <module>\n"
	 "-s <slot>\n");
}

int
arg_choice(int choice_idx, char* arg)
{

  choice_entry_t *choice = GET_CHOICE(choice_idx);

  if(choice == NULL) {
    return -1;
  }

  if(input_mode != COMMAND_LINE_INPUT) {
    printf("Must specify '-c' for command-line choices (such as '-n,-w,-m,-s').\n");
    return -1;
  }

  choice->value = atoi(arg);
  choice->chosen = 1;

  return 0;
}

err_entry_t*
parse_args(int argc, char *argv[]) 
{

  err_entry_t *choice = NULL;
  __uint64_t err_table, err_code;
  char err_subcode = NO_SUBCODE;
  int opt;

  if (argc == 1) {
    usage();
    return NULL;
  }

  while((opt=getopt(argc,argv,"lic:n:w:m:s:f:")) != -1) {  
    switch(opt) {
    case 'l':
      print_tables();
      return NULL;
    case 'i':
      input_mode = INTERACTIVE_INPUT;
      /* Get the table and code first*/
      assign_param(TABLE_CHOICE,&err_table);
      assign_param(CODE_CHOICE,&err_code);
      /* If it has a sub code ask for that too */
      if(find_entry(err_table,err_code,'a')) {
	printf("Select Sub Code: ");
	scanf("%c",&err_subcode);
	printf("\n");
      }
      break;
    case 'c':
      input_mode = COMMAND_LINE_INPUT;
      err_table = atoi(optarg);
      err_code = atoi(argv[optind++]);
      if (argc >optind && isalpha(argv[optind][0])) {
	err_subcode = argv[optind++][0];
      }
      break;
    case 'n':
      if(arg_choice(NODE_CHOICE,optarg) == -1) { return NULL; }	
      break;
    case 'w':
      if(arg_choice(WIDGET_CHOICE,optarg) == -1) { return NULL; }	
      break;
    case 'm':
      if(arg_choice(MODULE_CHOICE,optarg) == -1) { return NULL; }	
      break;
    case 's':
      if(arg_choice(SLOT_CHOICE,optarg) == -1) { return NULL; }	
      break;
    case 'f':
      user_fileName = optarg;
      break;
    case ':':
    default:
      usage();
      return NULL;
    }	
  }

  if (input_mode == UNDEF_INPUT) {
    usage();
    return NULL;
  }
  choice = find_entry(err_table,err_code,err_subcode);
  if (choice == NULL) {
    printf ("Invalid Error Choice: Table %lld, Code %lld%c\n",
	    err_table,err_code,err_subcode == NO_SUBCODE ? ' ' : err_subcode);
  }		

  return choice;
  
}

    
int
main(int argc, char *argv[])
{

  err_entry_t *choice = NULL;

  choice = parse_args(argc,argv);

  if (choice == NULL) {
    return 1;
  }
  
  /* Tell the user their test */
  printf("Injecting error %d.%d%c: %s\n",  
	 choice->err_table,choice->err_code,
	 choice->err_subcode == NO_SUBCODE ? ' ' : choice->err_subcode, 
	 choice->err_label);

  if (execute_error(choice)) {
    printf("Error Injection failed\n");
    return 1;
  }
  return 0;
}

