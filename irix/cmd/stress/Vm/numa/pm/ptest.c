/*****************************************************************************
 * copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <procfs/procfs.h>
#include <sys/pmo.h>
#include <sys/syssgi.h>
#include <sys/sysmp.h>
#include <sys/SN/hwcntrs.h>

#define HPSIZE           (0x1000)
#define HPSIZE_MASK      (HPSIZE-1)
#define HPSIZE_SHIFT     (12)
#define OLD_DATA_POOL_SIZE   (128*1024)
#define DATA_POOL_SIZE   (128*1024)
#define CACHE_TRASH_SIZE ((4*1024*1024)/sizeof(long))

#define MAX_MLDS 64
#define NUM_PLACEMENT_POLICIES 6
#define NUM_FALLBACK_POLICIES 2
#define NUM_REPLICATION_POLICIES 2
#define NUM_MIGRATION_POLICIES 3
#define NUM_PAGING_POLICIES 1
#define NODE_NAME_LEN 256
#define MAX_NODES 64

long cache_trash_buffer[CACHE_TRASH_SIZE];
char node_names[MAX_NODES][NODE_NAME_LEN];

char placement_policies[NUM_PLACEMENT_POLICIES][25] = {
  "PlacementDefault",
  "PlacementFixed",
  "PlacementFirstTouch",
  "PlacementRoundRobin",
  "PlacementThreadLocal",
  "PlacementCacheColor"
};

char fallback_policies[NUM_FALLBACK_POLICIES][25] = {
  "FallbackDefault",
  "FallbackLocal"
};

char replication_policies[NUM_REPLICATION_POLICIES][25] = {
  "ReplicationDefault",
  "ReplicationOne"
};

char migration_policies[NUM_MIGRATION_POLICIES][25] = {
  "MigrationDefault",
  "MigrationControl",
  "MigrationRefcnt"
};

char paging_policies[NUM_PAGING_POLICIES][25] = {
  "PagingDefault"
};

typedef struct test_info_s {
  int placement_policy;
  int fallback_policy;
  int replication_policy;
  int migration_policy;
  int paging_policy;
  size_t page_size;
  int policy_flags;
  int num_mlds;
  int mld_sizes[MAX_MLDS];
  int mld_nodes[MAX_MLDS];  
  int data_size;
  int thread_node;
  rqmode_t request_mode;
} test_info_t;

void
place_data(char* vaddr, test_info_t *test_info)
{
	pmo_handle_t mldlist[MAX_MLDS];
        pmo_handle_t mldset;
        raff_info_t  rafflist[MAX_MLDS];
        pmo_handle_t pm;
        policy_set_t policy_set;
	int i;
	
	for(i = 0; i < test_info->num_mlds; i++) {
	  if ((mldlist[i] = mld_create(0, test_info->mld_sizes[i])) < 0) {
	    perror("mld_create");
	    exit(1);
	  }
	  
	  rafflist[i].resource = node_names[test_info->mld_nodes[i]];
	  rafflist[i].restype = RAFFIDT_NAME;
	  rafflist[i].reslen = (ushort)strlen(node_names[test_info->mld_nodes[i]]);
	  rafflist[i].radius = 0;
	  rafflist[i].attr = RAFFATTR_ATTRACTION;
	}

        if ((mldset = mldset_create(&mldlist[0], test_info->num_mlds)) < 0) {
                perror("mldset_create");
                exit(1);
        }

        if (mldset_place(mldset,
                         TOPOLOGY_PHYSNODES,
                         rafflist,
                         test_info->num_mlds,
                         test_info->request_mode) < 0) {
                perror("mldset_place");
                exit(1);
        }

        pm_filldefault(&policy_set);

        policy_set.placement_policy_name = placement_policies[test_info->placement_policy];
	if(test_info->placement_policy == 0)
	  policy_set.placement_policy_args = (void*)1;
	else if(test_info->placement_policy == 2)
	  policy_set.placement_policy_args = (void*)NULL;
	else if(test_info->placement_policy == 1 || test_info->placement_policy == 5)
	  policy_set.placement_policy_args = (void*)mldlist[0];
	else if(test_info->placement_policy == 3 || test_info->placement_policy == 4)
	  policy_set.placement_policy_args = (void*)mldset;
	else {
	  fprintf(stderr, "Placement policy error in place_data()\n");
	  exit(1);
	}	  
	policy_set.fallback_policy_name = fallback_policies[test_info->fallback_policy];
	policy_set.fallback_policy_args = NULL;
	policy_set.replication_policy_name = replication_policies[test_info->replication_policy];
	policy_set.replication_policy_args = NULL;
        policy_set.migration_policy_name = migration_policies[test_info->migration_policy];
        policy_set.migration_policy_args = NULL;
	policy_set.paging_policy_name = paging_policies[test_info->paging_policy];
	policy_set.paging_policy_args = NULL;
	policy_set.page_size = test_info->page_size;
	policy_set.policy_flags = test_info->policy_flags;

        if ((pm = pm_create(&policy_set)) < 0) {
                perror("pm_create");
                exit(1);
        }

        if (pm_attach(pm, vaddr, test_info->data_size) < 0) {
                perror("pm_attach");
                exit(1);
        }

}

void
place_process(char *node)
{
        pmo_handle_t mld;
        pmo_handle_t mldset;
        raff_info_t  rafflist;
        
        /*
         * The mld, radius = 0 (from one node only)
         */
        
        if ((mld = mld_create(0, 0)) < 0) {
                perror("mld_create");
                exit(1);
        }

        /*
         * The mldset
         */

        if ((mldset = mldset_create(&mld, 1)) < 0) {
                perror("mldset_create");
                exit(1);
        }

        /*
         * Placing the mldset with the one mld
         */

        rafflist.resource = node;
        rafflist.restype = RAFFIDT_NAME;
        rafflist.reslen = (ushort)strlen(node);
        rafflist.radius = 0;
        rafflist.attr = RAFFATTR_ATTRACTION;

        if (mldset_place(mldset,
                         TOPOLOGY_PHYSNODES,
                         &rafflist, 1,
                         RQMODE_ADVISORY) < 0) {
                perror("mldset_place");
                exit(1);
        }

        /*
         * Attach this process to run only on the node
         * where the mld has been placed.
         */

        if (process_mldlink(0, mld, RQMODE_MANDATORY) < 0) {
                perror("process_mldlink");
                exit(1);
        }

}


void
print_refcounters(char* vaddr, int len)
{
        pid_t pid = getpid();
        char  pfile[256];
        int fd;
        sn0_refcnt_buf_t* refcnt_buffer;
        sn0_refcnt_buf_t* direct_refcnt_buffer;        
        sn0_refcnt_args_t* refcnt_args;
        int npages;
        int gen_start;
        int numnodes;
        int page;
        int node;

        sprintf(pfile, "/proc/%05d", pid);
        if ((fd = open(pfile, O_RDONLY)) < 0) {
                fprintf(stderr,"Can't open /proc/%d", pid);
                exit(1);
        }

        vaddr = (char *)( (unsigned long)vaddr & ~HPSIZE_MASK );
        npages = (len + HPSIZE_MASK) >> (HPSIZE_SHIFT);

        if ((refcnt_buffer = malloc(sizeof(sn0_refcnt_buf_t) * npages)) == NULL) {
                perror("malloc refcnt_buffer");
                exit(1);
        }
        
        if ((direct_refcnt_buffer = malloc(sizeof(sn0_refcnt_buf_t) * npages)) == NULL) {
                perror("malloc direct_refcnt_buffer");
                exit(1);
        }
        
        if ((refcnt_args = malloc(sizeof(sn0_refcnt_args_t))) == NULL) {
                perror("malloc refcnt_args");
                exit(1);
        }

        refcnt_args->vaddr = (__uint64_t)vaddr;
        refcnt_args->len = len;
        refcnt_args->buf = refcnt_buffer;

        if ((gen_start = ioctl(fd, PIOCGETSN0EXTREFCNTRS, (void *)refcnt_args)) < 0) {
                perror("ioctl  PIOCGETSN0EXTREFCNTRS returns error");
                exit(1);
        }

        refcnt_args->vaddr = (__uint64_t)vaddr;
        refcnt_args->len = len;
        refcnt_args->buf = direct_refcnt_buffer;
        
        if ((gen_start = ioctl(fd, PIOCGETSN0REFCNTRS, (void *)refcnt_args)) < 0) {
                perror("ioctl  PIOCGETSN0REFCNTRS returns error");
                exit(1);
        }
        
        if ((numnodes = sysmp(MP_NUMNODES)) < 0) {
                perror("sysmp MP_NUMNODES");
                exit(1);
        }

#ifdef NUMA_TEST_VERBOSE       
        for (page = 0; page < npages; page++) {
                printf("page[%05d, 0x%lx, 0x%010llx (0x%07llx) %02d]:",
                       page,
                       vaddr + page*0x1000,
                       refcnt_buffer[page].paddr,
                       refcnt_buffer[page].paddr >> 14,
		       refcnt_buffer[page].cnodeid);
                for (node = 0; node < numnodes; node++) {
                        printf(" %05lld (%04lld)",
                               refcnt_buffer[page].refcnt_set.refcnt[node],
                               direct_refcnt_buffer[page].refcnt_set.refcnt[node]);
                }
                printf("\n");
        }
#endif
        close(fd);
        free(refcnt_args);
        free(refcnt_buffer);
	free(direct_refcnt_buffer);
}

void
init_buffer(void* m, size_t size)
{
        size_t i;
        char* p = (char*)m;
        
        for (i = 0; i < size; i++) {
                p[i] = (char)i;
        }
}

long
buffer_auto_dotproduct_update(void* m, size_t size)
{
        size_t i;
        size_t j;
        char* p = (char*)m;
        long sum = 0;

        
        for (i = 0, j = size - 1; i < size; i++, j--) {
                sum += (long)p[i]-- * (long)p[j]++;
        }

        return (sum);
}

long
cache_trash(long* m, size_t long_size)
{
        int i;
        long sum = 0;

        for (i = 0; i < long_size; i++) {
               m[i] = i;
        }

        for (i = 0; i < long_size; i++) {
                sum += m[i];
        }

        return (sum);
}

void
do_stuff(void* m, size_t size, int loops, char* label)
{
        long total = 0;
        int count = loops;

        while (count--) {
                total += buffer_auto_dotproduct_update(m, size);
                total += cache_trash(cache_trash_buffer, CACHE_TRASH_SIZE);
        }
}

char ucase(char c)
{
  if(c >= 'a' && c <= 'z')
    return(c - 'a' + 'A');
  return(c);
}

void usage(char *name) {
  fprintf(stderr, "Usage: %s [options]\n", name);
  fprintf(stderr, "  -h -- help\n");
  fprintf(stderr, "  -x -- Execute immediately (don't enter interactive mode)\n");
  fprintf(stderr, "  -pl<Placement Policy number>\n");
  fprintf(stderr, "  -fp<Fallback Policy number>\n");
  fprintf(stderr, "  -rp<Replication Policy number>\n");
  fprintf(stderr, "  -mp<Migration Policy number>\n");
  fprintf(stderr, "  -pa<Paging Policy number>\n");
  fprintf(stderr, "  -ps<Page Size (in KB)>\n");
  fprintf(stderr, "  -fl<Policy Flag number>\n");
  fprintf(stderr, "  -rm<Request Mode number>\n");
  fprintf(stderr, "  -d<Data Size (in KB)>\n");
  fprintf(stderr, "  -t<Thread Node number>\n");
  fprintf(stderr, "  -n<Number of MLDs>\n");
  fprintf(stderr, "  -m<MLD number>n<Node number>s<MLD size (in KB)>\n");
  fprintf(stderr, "      This is used to specify the node and/or size of a particular MLD.  The \n");
  fprintf(stderr, "      n<Node number> and s<MLD size (in KB)> are optional.\n");  
  fprintf(stderr, "      All of the following are legal:\n");
  fprintf(stderr, "        -m<MLD number>n<Node number>\n");
  fprintf(stderr, "        -m<MLD number>s<MLD size (in KB)>\n");
  fprintf(stderr, "        -m<MLD number>n<Node number>s<MLD size (in KB)>\n");
  fprintf(stderr, "        -m<MLD number>s<MLD size (in KB)>n<Node number>\n");
  exit(1);
}

int get_parameter(char *t)
{
  char sbuff[10];

  printf("%s\n", t);
  return(atoi(fgets(sbuff, 10, stdin)));
}

int get_node_names()
{
  FILE *node_stream;
  int total_nodes = 0;

  /* use some UNIX stuff to read get the node names */
  if((node_stream = popen("find /hw -name node -print | sort", "r")) == NULL) {
    perror("popen()");
    return(-1);
  }
  
  /* read in the node names and figure out how many nodes there are */
  while(fgets(node_names[total_nodes], NODE_NAME_LEN, node_stream) != NULL) {
          char* p = strchr(node_names[total_nodes], '\n');
           if (p != NULL) {
                   *p = '\0';
           }
           total_nodes++;
  }

  pclose(node_stream);

  /* return total number of nodes in system */
  return(total_nodes);
}

void initialize_parameters(test_info_t *test_info)
{
  int mld;

  test_info->placement_policy = 0;
  test_info->fallback_policy = 0;
  test_info->replication_policy = 0;
  test_info->migration_policy = 0;
  test_info->paging_policy = 0;
  test_info->page_size = PM_PAGESZ_DEFAULT;
  test_info->policy_flags = 0;
  test_info->num_mlds = 1;
  
  for(mld = 0; mld < MAX_MLDS; mld++) {
    test_info->mld_sizes[mld] = 0;
    test_info->mld_nodes[mld] = 0;
  }

  test_info->data_size = DATA_POOL_SIZE;
  test_info->mld_sizes[0] = DATA_POOL_SIZE;
  test_info->thread_node = 0;
  test_info->request_mode = RQMODE_ADVISORY;
}

void display_info(int total_nodes)
{
  int i;

  printf("\n");

  /* Display node path names */
  for(i = 0; i < total_nodes; i++)
    printf("Node %02d: %s\n", i, node_names[i]);    
  printf("\n");

  /* Display placement policies */
  for(i = 0; i < NUM_PLACEMENT_POLICIES; i++)
    printf("Placement Policy %d: %s\n", i, placement_policies[i]);
  printf("\n");

  /* Display fallback policies */
  for(i = 0; i < NUM_FALLBACK_POLICIES; i++)
    printf("Fallback Policy %d: %s\n", i, fallback_policies[i]);
  printf("\n");

  /* Display replication policies */
  for(i = 0; i < NUM_REPLICATION_POLICIES; i++)
    printf("Replication Policy %d: %s\n", i, replication_policies[i]);
  printf("\n");

  /* Display migration policies */
  for(i = 0; i < NUM_MIGRATION_POLICIES; i++)
    printf("Migration Policy %d: %s\n", i, migration_policies[i]);
  printf("\n");

  /* Display paging policies */
  for(i = 0; i < NUM_PAGING_POLICIES; i++)
    printf("Paging Policy %d: %s\n", i, paging_policies[i]);
  printf("\n");

  /* Display policy flag numbers */
  printf("Policy Flag 0: none\n");
  printf("Policy Flag 1: cache color first\n\n");

  /* Display request mode numbers */
  printf("Request Mode 0: advisory\n");
  printf("Request Mode 1: mandatory\n\n");
}

int check_parameters(test_info_t *test_info, int total_nodes)
{
  int i;
  
  /* Check for valid placement policy */
  if(test_info->placement_policy >= NUM_PLACEMENT_POLICIES) {
    fprintf(stderr, "Invalid placement policy\n");
    return(-1);
  }
  /* Check for valid fallback policy */
  if(test_info->fallback_policy >= NUM_FALLBACK_POLICIES) {
    fprintf(stderr, "Invalid fallback policy\n");
    return(-1);
  }
  /* Check for valid replication policy */
  if(test_info->replication_policy >= NUM_REPLICATION_POLICIES) {
    fprintf(stderr, "Invalid replication policy\n");
    return(-1);
  }
  /* Check for valid migration policy */
  if(test_info->migration_policy >= NUM_MIGRATION_POLICIES) {
    fprintf(stderr, "Invalid migration policy\n");
    return(-1);
  }
  /* Check for valid paging policy */
  if(test_info->paging_policy >= NUM_PAGING_POLICIES) {
    fprintf(stderr, "Invalid paging policy\n");
    return(-1);
  }
  /* Check if thread node exists */
  if(test_info->thread_node >= total_nodes) {
    fprintf(stderr, "Invalid thread node\n");
    return(-1);
  }
  /* Check to see if number of MLDs is less than the maximum allowed */
  if(test_info->num_mlds > MAX_MLDS) {
    fprintf(stderr, "Invalid number of MLDs\n");
    return(-1);
  }
  /* Check to see if each node assoicated with each MLD exists */
  for(i = 0; i < MAX_MLDS; i++) {
    if(test_info->mld_nodes[i] >= total_nodes) {
      fprintf(stderr, "Invalid MLD %d node\n", i);
      return(-1);
    }
  }
  return(0);
}

void display_parameters(test_info_t *test_info)
{
  int i;

  printf("\n*** Current settings ***\n\n");

  /* Display policies */
  printf("a) Placement Policy:   %s\n", placement_policies[test_info->placement_policy]);
  printf("b) Fallback Policy:    %s\n", fallback_policies[test_info->fallback_policy]);
  printf("c) Replication Policy: %s\n", replication_policies[test_info->replication_policy]);
  printf("d) Migration Policy:   %s\n", migration_policies[test_info->migration_policy]);
  printf("e) Paging Policy:      %s\n", paging_policies[test_info->paging_policy]);
  if(test_info->page_size != PM_PAGESZ_DEFAULT)
    printf("f) Page Size:          %dK\n", test_info->page_size/1024);
  else
    printf("f) Page Size:          Default\n");
  printf("g) Policy Flags        %s\n", test_info->policy_flags == POLICY_CACHE_COLOR_FIRST ? "Cache color first" : "None");
  printf("h) Data Size:          %dK\n", test_info->data_size/1024);
  printf("i) Thread Node:        %02d\n", test_info->thread_node);
  printf("j) Request Mode        %s\n", test_info->request_mode == RQMODE_ADVISORY ? "Advisory" : "Mandatory");
  printf("k) Number of MLDs:     %d\n", test_info->num_mlds);
  for(i = 0; i < test_info->num_mlds; i++) {
    printf("%c) MLD %02d Node:        %02d\n", 'l'+i*2, i, test_info->mld_nodes[i]);
    printf("%c)        Size:        %07dK\n", 'm'+i*2, test_info->mld_sizes[i]/1024);
  }
  printf("\n");
  printf("X) execute  I) information  P) parameters  Q) quit\n");
}

main(int argc, char** argv)
{
	int arg_num;
	int total_nodes;
	test_info_t test_info, ti_mirror;
	int key = 0;
	int mld;
	int go_flag = 0;
	char *t;
	char sbuff[80];
	int i;
	char *data_pool;

#if 0
	key = (int)strtol("12", &t, 10);
	printf("%d %s\n", key, t);
	if(*t == NULL)
	  printf("hi\n");
	exit(1);
#endif

	if((total_nodes = get_node_names()) < 0) {
	  printf("Error getting node names\n");
	  exit(1);
	}

	initialize_parameters(&test_info);
	/* Turn on reference counters by default */
	test_info.migration_policy = 2;

	for(arg_num = 1; arg_num < argc; arg_num++) {
	  if(argv[arg_num][0] != '-')
	    usage(argv[0]);
	  switch(argv[arg_num][1]) {
	  case 'p':
	    switch(argv[arg_num][2]) {
	    case 'l':
	      test_info.placement_policy = atoi(&argv[arg_num][3]);
	      break;
	    case 'a':
	      test_info.paging_policy = atoi(&argv[arg_num][3]);
	      break;
	    case 's':
	      if((test_info.page_size = atoi(&argv[arg_num][3]) * 1024) == 0)
		test_info.page_size = PM_PAGESZ_DEFAULT;
	      break;
	    default:
	      usage(argv[0]);
	      break;
	    }
	    break;
	  case 'f':
	    switch(argv[arg_num][2]) {
	    case 'p':
	      test_info.fallback_policy = atoi(&argv[arg_num][3]);
	      break;
	    case 'l':
	      test_info.policy_flags = atoi(&argv[arg_num][3]) == 1 ? POLICY_CACHE_COLOR_FIRST : 0;
	      break;
	    default:
	      usage(argv[0]);
	      break;
	    }
	    break;
	  case 'r':
	    switch(argv[arg_num][2]) {
	    case 'p':
	      test_info.replication_policy = atoi(&argv[arg_num][3]);
	      break;
	    case 'm':
	      test_info.request_mode = atoi(&argv[arg_num][3]) == 1 ? RQMODE_MANDATORY : RQMODE_ADVISORY;
	      break;
	    default:
	      usage(argv[0]);
	      break;
	    }
	    break;
	  case 'm':	    
	    if(argv[arg_num][2] == 'p')
	      test_info.migration_policy = atoi(&argv[arg_num][3]);
	    else if(argv[arg_num][2] >= '0' && argv[arg_num][2] <= '9') {
	      mld = (int)strtol(&argv[arg_num][2], &t, 10);
	      if(mld >= MAX_MLDS) {
		fprintf(stderr, "MLD %d out of range\n", mld);
		exit(1);
	      }
	      while(t[0] != NULL) {
		if(t[1] < '0' || t[1] > '9')
		  usage(argv[0]);
		switch(t[0]) {
		case 'n':
		  test_info.mld_nodes[mld] = (int)strtol(t+1, &t, 10);
		  break;
		case 's':
		  test_info.mld_sizes[mld] = (int)strtol(t+1, &t, 10) * 1024;
		  break;
		default:
		  usage(argv[0]);
		  break;
		}
	      }
	    }
	    else
	      usage(argv[0]);
	    break;
	  case 'd':
	    test_info.data_size = atoi(&argv[arg_num][2]) * 1024;
	    break;
	  case 'n':
	    test_info.num_mlds = atoi(&argv[arg_num][2]);
	    break;
	  case 'h':
	    usage(argv[0]);
	    break;
	  case 't':
	    test_info.thread_node = atoi(&argv[arg_num][2]);
	    break;
	  case 'x':
	    go_flag = 1;
	    break;
	  default:
	    usage(argv[0]);
	    break;
	  }
	}
	
	if(check_parameters(&test_info, total_nodes) < 0)
	  exit(1);
	bcopy(&test_info, &ti_mirror, sizeof(test_info_t));

#ifdef NUMA_TEST_VERBOSE        
	display_parameters(&test_info);
#endif        

	while(!go_flag) {
	  fgets(sbuff, 10, stdin);
	  key = sbuff[0];

	  if((key >= 'l') && (key < 'l'+MAX_MLDS*2)) {
	    mld = (int)key - 'l';
	    if(mld % 2 == 0) {
	      sprintf(sbuff, "Enter MLD %d node:", mld/2);
	      ti_mirror.mld_nodes[mld/2] = get_parameter(sbuff);
	    }
	    else {
	      sprintf(sbuff, "Enter MLD %d size (in KB):", mld/2);
	      ti_mirror.mld_sizes[mld/2] = get_parameter(sbuff) * 1024;
	    }
	  }
	  else
	    switch(key) {
	    case 'a':
	      for(i = 0; i < NUM_PLACEMENT_POLICIES; i++)
		printf("Placement Policy %d: %s\n", i, placement_policies[i]);
	      ti_mirror.placement_policy = get_parameter("Enter placement policy number:");
	      break;
	    case 'b':
	      for(i = 0; i < NUM_FALLBACK_POLICIES; i++)
		printf("Fallback Policy %d: %s\n", i, fallback_policies[i]);
	      ti_mirror.fallback_policy = get_parameter("Enter fallback policy number:");
	      break;
	    case 'c':
	      for(i = 0; i < NUM_REPLICATION_POLICIES; i++)
		printf("Replication Policy %d: %s\n", i, replication_policies[i]);
	      ti_mirror.replication_policy = get_parameter("Enter replication policy number:");
	      break;
	    case 'd':
	      for(i = 0; i < NUM_MIGRATION_POLICIES; i++)
		printf("Migration Policy %d: %s\n", i, migration_policies[i]);
	      ti_mirror.migration_policy = get_parameter("Enter migration policy number:");
	      break;
	    case 'e':
	      for(i = 0; i < NUM_PAGING_POLICIES; i++)
		printf("Paging Policy %d: %s\n", i, paging_policies[i]);
	      ti_mirror.paging_policy = get_parameter("Enter paging policy number:");
	      break;
	    case 'f':
	      ti_mirror.page_size = get_parameter("Enter page size (in KB) (0 for default):") * 1024;
	      if(ti_mirror.page_size == 0)
		ti_mirror.page_size = PM_PAGESZ_DEFAULT;
	      break;
	    case 'g':
	      ti_mirror.policy_flags = 
		get_parameter("Enter flag number (0 - none, 1 - cache color first):") == 1 ? POLICY_CACHE_COLOR_FIRST : 0;
	      break;
	    case 'h':
	      ti_mirror.data_size = get_parameter("Enter data size (in KB):") * 1024;
	      break;
	    case 'i':
	      ti_mirror.thread_node = get_parameter("Enter thread node:");
	      break;
	    case 'j':
	      ti_mirror.request_mode =
		get_parameter("Enter request mode (0 - advisory, 1 - mandatory):") == 1 ? RQMODE_MANDATORY : RQMODE_ADVISORY;
	      break;
	    case 'k':
	      ti_mirror.num_mlds = get_parameter("Enter number of MLDs:");
	      break;
	    case 'Q':
	      exit(1);
	      break;
	    case 'I':
	      display_info(total_nodes);
	      break;
	    case 'P':
	      display_parameters(&test_info);
	      break;
	    case 'X':
	      go_flag = 1;
	      break;
	    case '\n':
	      break;
	    default:
	      fprintf(stderr, "Keystroke %c not recognized\n", key);
	      break;
	    }
	  
	  if(check_parameters(&ti_mirror, total_nodes) < 0)
	    bcopy(&test_info, &ti_mirror, sizeof(test_info_t));
	  else
	    bcopy(&ti_mirror, &test_info, sizeof(test_info_t));
	}

	data_pool = (char *)malloc(test_info.data_size);
	if(data_pool == NULL) {
	  fprintf(stderr, "malloc() error\n");
	  exit(1);
	}

        place_data(data_pool, &test_info);
        init_buffer(data_pool, test_info.data_size);
        
        /*
         * Place process
         */
        place_process(node_names[test_info.thread_node]);


        /*
         * Reference pages & print refcnt
         */


	print_refcounters(data_pool, DATA_POOL_SIZE);
        
	do_stuff(data_pool, DATA_POOL_SIZE, 100, "BUFFER");
        print_refcounters(data_pool, DATA_POOL_SIZE);

	free(data_pool);
}  
