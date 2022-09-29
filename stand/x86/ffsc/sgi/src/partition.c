/*
 * $Id: partition.c,v 1.2 1999/05/14 02:21:45 sasha Exp $
 */
#include <vxworks.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ffsc.h"
#include "addrd.h"
#include "elsc.h"
#include "log.h"
#include "misc.h"
#include "msg.h"
#include "nvram.h"
#include "route.h"
#include "user.h"



/* 
 * Globals
 *
 * The below contains a global copy of the partition information
 * from the data found on the local rack + all other racks.
 */
part_info_t* partInfo = NULL;   /* Head pointer for partition info list */

/*
 * Mutual exclusion semaphore; for locking on the list.
 */
SEM_ID part_mutex;
/*
 * Initialize the semaphore and other startup tasks. We do this very early,
 * right after the init code is done in init.c .
 */
void part_init(){
  part_mutex = semMCreate(SEM_Q_PRIORITY | SEM_INVERSION_SAFE);
  if(ffscDebug.Flags & FDBF_PARTINFO) 
    ffscMsg("NOTICE: Partition map allocated.\r\n");
}


void lock_part_info(){
  
  if(OK != semTake(part_mutex, WAIT_FOREVER)){
    if(ffscDebug.Flags & FDBF_PARTINFO) 
      ffscMsg("lock_part_info failed.\r\n");
  }
#ifndef PRODUCTION
  else
    if(ffscDebug.Flags & FDBF_PARTINFO) 
      ffscMsg("NOTICE: Partition map locked.\r\n");
#endif
}

void unlock_part_info(){

  if(OK != semGive(part_mutex)){
    if(ffscDebug.Flags & FDBF_PARTINFO) 
      ffscMsg("unlock_part_info failed.\r\n");
  }
#ifndef PRODUCTION
  else
    if(ffscDebug.Flags & FDBF_PARTINFO) 
      ffscMsg("NOTICE: Partition map unlocked.\r\n");
#endif
}

/*
 * Prints out the partitioning information for a single part_info_t* 
 * passed in as an arg.
 */
void print_part(part_info_t** info)
{
  int i = 0;
  ffscMsg("Partition %d: [" , (*info)->partId);
  for(i = 0; i < (*info)->moduleCount; i++){
    if(i != (*info)->moduleCount -1)
      ffscMsg("%d,", (*info)->modules[i]);
    else
      ffscMsg("%d]:", (*info)->modules[i]);
  }
  if((*info)->consoleModule < 0)
    ffscMsg("console MSC=<undef>\n");
  else
    ffscMsg("console MSC=%d\n", (*info)->consoleModule);
}



static int compare_modnum_func(const void* item1, const void* item2)
{
  int a = *((int*)item1);
  int b = *((int*)item2);
  if(a < b)
    return -1;
  if(a == b)
    return 0;
  else
    return 1;
}


/*
 * Returns a character string which represents our partition information.
 * This routine is used by the MMSC command cmdPARTLST()
 */
char* get_part_info(part_info_t** info)
{
  char tmp[25];
  char* buf = (char*)malloc(1024);
  int i = 0;

  /*
   * First sort the modules so they look the same on each rack...
   */
  qsort((void*)(*info)->modules,
	(*info)->moduleCount,sizeof(int),compare_modnum_func);

  memset(buf,0x0,1024);
  sprintf(buf,"Partition %d Name=%s%d Modules=[" ,
	  (*info)->partId,
	  (*info)->name ? (*info)->name : "partition-",
	  (*info)->partId);

  for(i = 0; i < (*info)->moduleCount; i++){
    if(i != (*info)->moduleCount -1){
      memset(tmp,0x0,25);
      sprintf(tmp,"%d+", (*info)->modules[i]);
      strcat(buf,tmp);
    }
    else{
      memset(tmp,0x0,25);
      sprintf(tmp,"%d", (*info)->modules[i]);
      strcat(buf,tmp);
    }
  }
  memset(tmp,0x0,25);
  if((*info)->consoleModule < 0){
    sprintf(tmp,"] Console=none\r\n");
  }
  else{
    sprintf(tmp,"] Console=%d\r\n", (*info)->consoleModule);
  }
  strcat(buf,tmp);
  return buf;
}


/*
 * Remove a partition ID from our list.
 */
static void remove_node(part_info_t** node, part_info_t* s)
{
  part_info_t* p = *node;
  part_info_t* q = *node;

  /* First element in list */
  if(p == s){
    *node = p->next;
    free(s);
    return;
  } 
  else{
    while(p->next != s && p->next !=NULL){
      q = p->next;
      p = p->next;
    }
    q->next = s->next;
    free(s);
  }
}

/*
 * Remove a module from a partition in our list.
 * NOTE: All module numbers are assumed to be unique (they better be :-)
 */
static int remove_module(part_info_t** info, int modid)
{
  int i, rindex = 0, done = 0;
  part_info_t *s = NULL, *p = *info;

  if(ffscDebug.Flags & FDBF_PARTINFO) 
    ffscMsg("NOTICE: Removing module %d from map.\r\n", modid);

  while(p!= NULL && !done){
    for(i = 0; i < p->moduleCount; i++){
      if(p->modules[i] == modid){
	done++;
	rindex = i; 
	s = p; 
	break;
      }
    }
    if(!done)
      p = p->next;
  }
  if(!done)
    return 0;

  if(s && (rindex >= 0)){
    for(i = rindex; i < rindex+1; i++){
      s->modules[i] = s->modules[i+1];
    } 
    if(--(s->moduleCount) == 0)
      remove_node(info,s);
    return 0;
  }
  else
    return -1;
}




/*
 * Given a part_info_t,  update the partition information structure
 * with the partid, moduleid, and the console module (if console > 0).
 * We make sure not to add a module to a partition twice and avoid
 * duplicate entries. We do this by scanning the list of modules stored
 * in a partition.
 */
int update_part_info(part_info_t** info, 
		     int partid, 
		     int modid, 
		     int console)
{
  int i ;
  if(ffscDebug.Flags & FDBF_PARTINFO) 
    ffscMsg("NOTICE: update_part_info called: P=%d, M=%d,C=%d.\r\n",
	    partid,modid,console);

  /* Check for NULL, list, we could have been rebooted */
  if(info == NULL)
    return 4;

  /* Check for No entries, it is possible another thread is updating us right now */
  if(*info == NULL)
    return 5;

  if(partid < 0)
    return 2; /* Bad partition */

  /* There is no such thing as a moduleid of "0" */
  if(modid == 0)
    return 3; /* Bad module */

  /*
   * Sanity Check (for duplicates), return value < 0 if found, update 
   * with current values if DUP
   */
  if((*info)->moduleCount) /* Make common case fast */
    for(i = 0; i < (*info)->moduleCount; i++){
      if(modid == (*info)->modules[i]){
	if(ffscDebug.Flags & FDBF_PARTINFO) 
	  ffscMsg("DUP entry found: part=%d, modnum = %d\n", partid, modid);
	/* 
	 * The correct way to add DUP's is to reinit them, since console
	 * assignment may not happen until we see the same message a few
	 * times.
	 */
	(*info)->partId = partid;
	(*info)->modules[i] = modid;
	if(console)
	  (*info)->consoleModule = modid;

	return 1; /* Dup found */
      }
    }
  /* No duplicates found, we can safely add the new data 
   * here now.
   */
  (*info)->partId = partid;
  (*info)->modules[(*info)->moduleCount++] = modid;
  if(console)
    (*info)->consoleModule = modid;

  return 0; /* No error */
}


/*
 * Allocates a new part_info_t* and sets up some default values
 * which the rest of the code depends upon.
 */

int alloc_part_info(part_info_t** info)
{
  if(ffscDebug.Flags & FDBF_PARTINFO) 
    ffscMsg("NOTICE: allocating partition node.\r\n");
  if ((*info = (part_info_t*) calloc(1,sizeof(part_info_t))) == NULL) {
    ffscMsg("Failed to allocate memory for new partition information");
    return ERROR;
  }
    
  (*info)->next          = NULL;
  (*info)->moduleCount   =  0;
  (*info)->partId        = -1; /* TBD */
  (*info)->consoleModule = -1;
  (*info)->name          = '\0';

  return OK; /* everything is cool */

}


/* parse_part_info_line:
 * Parse a line of output from the IP27/IO6 prom. This routine will 
 * act as a DFA (pattern analyzer) to detect strings of the form:
 * 
 * P [0-9]+ M [0-9]+ {C}
 * 
 * "P 7 M 1", "P 7 M 2C", "P7M2C", "P 7 M 2 C" are all valid examples.
 * (Note that spaces do not matter).
 * NOTE: user MUST free memory allocated by this puppy.
 * Currently used for global partition information update.
 */

mod_part_info_t* parse_part_info_line(char* str)
{
  int i=0, partNo=0, modNo=0,isConsoleModule=0;
  char tmp[10];
  char c, *ptr = NULL, *p1 = NULL;
  mod_part_info_t* mpi = NULL;

  if(!str)
    return mpi; /* Bad input string */

  if(*str != 'P' && *str != 'p'){
    return mpi; /* Bogus line of shit */
  }

  ptr = str;
  /* info has been allocated */
  while((c = *ptr++)){
    if(c == 'P' || c == 'p'){
      /* Now at 'P' character, grab digits+space after and convert to int */
      for(i = 0, p1 = ptr; *p1 != 'M' && *p1 != 0; p1++)
	tmp[i++] = *p1;
      tmp[i] = 0;
      partNo = atoi(tmp);
      /* Now at 'M' character, same thing */
      for(i = 0, (void)*p1++, ptr = p1; *p1 != 'C' && *p1 != 0; p1++)
	tmp[i++] = *p1;
      tmp[i] = 0; 
      modNo = atoi(tmp);
      /* Determine whether or not this module is a console (C follows)*/
      /* @@ TODO: verify that there is no space after module number!!! */
      if(*p1 == 'C')
	isConsoleModule++;
    }
  }

  mpi = (mod_part_info_t*)malloc(sizeof(mod_part_info_t*));
  mpi->partid = partNo;
  mpi->modid = modNo;
  mpi->cons = isConsoleModule;  
  return mpi; /* No error */
}




/* update_partition_info:
 *
 * Add information to our local state regarding partitions.
 * The information is maintained internally as a linked-list of data 
 * structures defined by the structure definition above.
 * 
 * First call should pass a part_info_t** which has not been allocated.
 * Successive calls to this routine will do one of two things:
 * 
 * a) Update a partition definition with a new module member, store in 
 *    current head of linked-list.
 * b) Create a new partition definition at the end of the list.
 * NOTES:
 *   -part_info_t** info is an in/out parameter.
 *   -mod_part_info_t* mp is strictly an out parameter.
 */
int update_partition_info(part_info_t** info, char* str)
{
  int partNo=0, modNo=0,isConsoleModule=0;
  part_info_t* p = NULL, *q = NULL;

  if(ffscDebug.Flags & FDBF_PARTINFO) 
      ffscMsg("update_partition_info(0x%x,%s)\r\n",*info,str);
  
  if(!str){
    return -1; /* Bad input string */
  }



  if(*str != 'P' && *str != 'p'){
    if(ffscDebug.Flags & FDBF_PARTINFO) 
      ffscMsg("update_partition_info: str \"%s\" begins with \"%c\" -ignoring.\n",
	    str,*str);
    return -2; /* Bogus line of shit */
  }

  /* If the command comes in, it's going to look like this:
     P%2d M%2d or P%2d M%2dC  Anything that doesn't look like this should
     not concern us */

  if (sscanf(str, "P%2d M%2d", &partNo, &modNo) != 2) {
    if(ffscDebug.Flags & FDBF_PARTINFO) 
      ffscMsg("update_partition_info: str is not a partition command:\n\t%s\n",
	      str);
    return -2; /* Some other display message begginig with "P" */
  }

  if (str[7] == 'C') {
    isConsoleModule++;
  }


  /* Mutex enter  */
  lock_part_info();

  if(!*info) {
    if (alloc_part_info(info) != OK) {
      unlock_part_info();
      return -1; 
    }
  }
  

  /* Now we have module number, partition id, and whether or not the
   * module is a console device. So, we will update the list and take
   * care of partition entries that do or do not exist below.
   */

  /* First: rip it out if it's in there ... */
  remove_module(info,modNo);

  /* Now, do the update */
  if((*info)->partId == partNo)
    /* Update module list */
    update_part_info(info, partNo, modNo, isConsoleModule);
  
  else if((*info)->partId < 0)
    /* No entry yet, update all */
    update_part_info(info, partNo, modNo, isConsoleModule);

  else {
    /* Search list for entry with this partition Id.
     * If still not found, allocate a new part_info_t 
     */
    p = *info;
    while (p != NULL){
      if(p->partId == partNo){
	update_part_info(&p, partNo, modNo, isConsoleModule);
	break;
      } else{ /* Not found, save last pointer, increment next pointer.*/
	q = p;
	p = p->next;
	if(p == NULL){ /* Next pointer is dangling, alloc new and fill in */
	  if (alloc_part_info(&p) != OK) {
	    unlock_part_info();
	    return -1;
	  }
	  update_part_info(&p, partNo, modNo, isConsoleModule);
	  q->next = p;
	  break;
	}
      }
    }
  }
  /* Mutex exit */
  unlock_part_info();
  return 0; /* No error */
}


/*
 * Goes through the linked list and prints out all information
 * on partitions.
 */
void print_all_part(part_info_t** info)
{
  part_info_t* p = *info;
  while(p!= NULL){
    print_part(&p);
    p = p->next;
  }
}


/*
 * Cleanup a partition list (free all memory allocated).
 */
void dealloc_part_info(part_info_t** info)
{
  part_info_t* p = *info, *q = NULL;
  while(p!= NULL){
    q = p->next;
    free(p->name);
    free(p);
    p = q;
  }
}

/*
 * Returns information on the specified partition id. NULL if partid
 * is not found.
 */

char* get_part_info_on_partid(part_info_t** info, int partid)
{
  part_info_t* p = *info;
  char *str = NULL;
  while(p!= NULL){
    if(p->partId == partid){
      str = get_part_info(&p);
      break;
    } 
    p = p->next;
  }
  return str;
}


/*
 * Removes a partition ID from the master partition table.
 */
void remove_partition_id(part_info_t** info, int partid)
{
  part_info_t* p = *info;
  while(p != NULL){
    if(p->partId == partid){
      remove_node(info,p);
      return;
    } 
    else
      p = p->next;
  }
}


/*
 * Returns the partition which this module is assigned to or value < 0
 * if modNum does not belong to a partition.
 */
int modnum_to_partnum(part_info_t** info, int modNum)
{
  int i;
  part_info_t* p = *info;
  while(p != NULL){
    for(i = 0; i < p->moduleCount; i++){
      if(modNum == p->modules[i])
	return p->partId;
    }
    p = p->next;
  }
  return -1;
}


/*
 * Returns value of console Module # for this partition, val < 0 if error.
 */
int partition_console_module(part_info_t** info, int partid)
{
  part_info_t* p = *info;
  while(p != NULL){
    if(p->partId == partid)
      return p->consoleModule;
    p = p->next;
  }
  return -1;
}




/*
 * Returns true if we already have the module in this partition.
 * Usefule for when we broadcast data out on the net. Since we
 * will already have the info we sent, we use this to check the
 * validity of the request which could destroy precious data if
 * we already have it.
 */
int contains_module(part_info_t** info, int partid, int modid)
{
  int i;
  part_info_t* p = *info;
  while(p != NULL){
    if(p->partId == partid){
      for(i = 0; i < p->moduleCount; i++)
	if(p->modules[i] == modid)
	  return 1;
      return 0; /* No point in going on */
    }
    p = p->next;
  }
  return 0;
}


/*
 * Returns true if we already have the module in ANY partition.
 */
int contains_modulenum(part_info_t** info,int modid)
{
  int i;
  part_info_t* p = *info;
  while(p != NULL){
    for(i = 0; i < p->moduleCount; i++)
      if(p->modules[i] == modid)
	return 1;
    p = p->next;
  }
  return 0;
}



/* simply returns the number of partitions, given the head of the
 * partition list.
 */

int num_parts(part_info_t* info) {

  part_info_t* p = info;
  int total = 0;

  while(p != NULL) {
    p = p->next;
    total++;
  }

  return total;

}

  




/*
 * getPartitionMapping: 
 * returns a list of all the currently known partition mappings
 * which are in place at the time of invocation.
 * Returns: NULL delimited char array with entryCount set to the number
 * of character strings to parse.
 * NOTE: this is used by the GUI task for getting a list of partitions
 * and displaying them as a selection for the user.
 * 
 * Hacked out, but hey, it works!
 */
char* getPartitionMappings(int* entryCount)
{
  int localCount = 0;
  part_info_t* p = partInfo;
  char* buffer = (char*)malloc(1024);

  char* ptr = 0x0;

  /* zero silly buffer */
  memset(buffer,0x0,1024);

  /* Add the "all" string first */
  sprintf(buffer,"%s","all");
  ptr = buffer +4;
  localCount++;

  /*
   * People could be updating this so we need Mutex to access
   * it safely. Loop through the list and enumerate every partition.
   */
  lock_part_info();
  while(p != NULL){
    ptr += (sprintf(ptr, "%d", p->partId) + 1);
    localCount++;
    p = p->next;
  }
  /* Mutex exit */
  unlock_part_info();

  *entryCount = localCount;
  return buffer;

  /* NOTE: caller must free this up! */
}



#ifdef TEST_PARTCODE
/*
 * Test out routines with some real data
 */

int main(int argc, char *argv[])
{
  part_info_t* info = NULL;

  /* test out on 2 partitions */
  update_partition_info(&info, "P 1 M 4");
  update_partition_info(&info, "P 1 M 3 ");
  update_partition_info(&info, "P 1 M 7 ");
  update_partition_info(&info, "P 1 M 8 ");
  update_partition_info(&info, "P 1 M 9 ");
  update_partition_info(&info, "P1M10");
  update_partition_info(&info, "P 1 M 11C");
  update_partition_info(&info, "P 2 M 2");
  update_partition_info(&info, "P 2 M 12C");
  update_partition_info(&info, "P 2 M 13");
  update_partition_info(&info, "P 2 M 14");
  update_partition_info(&info, "P 2 M 15");
  update_partition_info(&info, "P 2 M 15");
  update_partition_info(&info, "P 2 M 15");
  update_partition_info(&info, "james was here");
  update_partition_info(&info, "P 1 M 4");
  update_partition_info(&info, "P 1 M 3 ");
  update_partition_info(&info, "P 1 M 4");
  update_partition_info(&info, "P 1 M 3 ");
  update_partition_info(&info, "P 1 M 4");
  update_partition_info(&info, "P 1 M 21 ");
  update_partition_info(&info, "Happy New Year!");
  update_partition_info(&info, "P 3 M 22 ");
  update_partition_info(&info, "P 3 M 24 C");
  update_partition_info(&info, "P 3 M 23C ");
  ffscMsg("\n\nFinal partition information:\n");
  print_all_part(&info);
  dealloc_part_info(&info);

}

#endif /* TEST_PARTCODE */














