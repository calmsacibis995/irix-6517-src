/* 
** File:  shmtest.c
** ----------------
** program to share memory between various partitions
*/


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "clshm_barrier.h"
#include "xp_shm_wrap.h"

/* text if no file is specified for sending data */
#define TEST_TEXT "This is the text to print out :).\n"

/* defaults for parameters into share_mem */
#define DEFAULT_THREADS		1
#define DEFAULT_PART_SIZE	getpagesize()
#define DEFAULT_ITERATIONS	1
#define DEFAULT_WRITE		0
#define DEFAULT_CONTINUOUS	0
#define DEFAULT_LIMITED_AREA	1
#define DEFAULT_FILE_NAME	NULL
#define DEFAULT_FILE_ID		-1
#define DEFAULT_AUTORMID	0
#define DEFAULT_MEM_PLACE	NULL

/* shared_t -- structure that associates a file descriptor with the
** pointer into addr space.
*/
typedef struct {
    int shmid;
    char *addr;
} shared_t;
   

/* usage string */ 
static const char *usage = "%s [-s<size>] [-t<threads>] [-m] [-i<iters>] "
    "[-w] [-c] \n\t\t [-f<file>] [-a] [-p<hwgraph/path>] [-v<mode>] [-h]\n"
    "meaning:\n"
    "\t -s:  # of bytes each partition is given on each shared segment\n"
    "\t -t:  # of threads on each partition\n"
    "\t -m:  make partitions write non-contiguous areas\n"
    "\t -i:  # of times to go through each shared/mapped page and w/r\n"
    "\t -w:  write only, don't check contents\n"
    "\t -c:  run forever\n"
    "\t -f:  specify a file is written and read from shared memory\n"
    "\t -a:  use IPC_AUTORMID\n"
    "\t -p:  place memory on particular node (/hw/nodenum/X)\n"
    "\t -v:  0 = nothing, 1 = minimal, 5 = debug, 10 = to many messages\n"
    "\t -h:  print out this message\n"
    "defaults:\n"
    "\t -s = system page size: %d\n"
    "\t -t = 1\n"
    "\t -m = partitions write to contiguous blocks on each shared segment\n"
    "\t -i = 1\n"
    "\t -w = not write only (check contents)\n"
    "\t -c = not continuous\n"
    "\t -f = no file, just an internal text string\n"
    "\t -a = don't use IPC_AUTORMID\n"
    "\t -p = memory can be place anywhere\n"
    "\t -v = 0\n";


/* prototypes */
void parse_args(int argc, char *argv[], int *num_threads, 
		int *each_part_size, int *iterations, int *continuous, 
		int *write_my_area, int *write_only, char **file_name,
		int *auto_cleanup, char **place_memory);
void stupid_sort(int num_parts, partid_t *part_list);
void print_hostnames(int num_parts, partid_t *part_list);
int fork_threads(int num_threads);
void place_memory_if_needed(char *place_memory, key_t key);
int setup_mapped_areas(int num_parts, shared_t *share, key_t *keys, 
		       int my_index, int each_part_size, int auto_cleanup);
void print_placed_memory(char *place_memory, int shmid);
int share_memory(int num_parts, shared_t *share, int my_index, 
		 int each_part_size, int iters, int continuous,
		 int write_my_area, int num_threads, int thread_num,
		 int write_only, int data_file_fd);
int write_to_memory(int num_parts, shared_t *share, int my_index,
		    int each_part_size, int write_my_area,
		    int num_threads, int thread_num, int data_file_fd,
		    off_t len, off_t file_offset);
int read_from_memory(int num_parts, shared_t *share, int each_part_size, 
		     int write_my_area, int num_threads, int data_file_fd, 
		     off_t len, off_t file_offset);
int remove_shared_segments(int num_parts, int thread_num, int my_index, 
			   shared_t *share, int auto_cleanup);


/* parse_args
** ----------
** parse arguments and make sure most arguments make sense.
*/
void parse_args(int argc, char *argv[], int *num_threads, 
		int *each_part_size, int *iterations, int *continuous, 
		int *write_my_area, int *write_only, char **file_name,
		int *auto_cleanup, char **place_memory)
{
    extern char *optarg;
    extern int optind;
    int c;
    int error = 0;

    *num_threads = DEFAULT_THREADS;
    *each_part_size = DEFAULT_PART_SIZE;
    *iterations = DEFAULT_ITERATIONS;
    *write_only = DEFAULT_WRITE;
    *continuous = DEFAULT_CONTINUOUS;
    *write_my_area = DEFAULT_LIMITED_AREA;
    *file_name = DEFAULT_FILE_NAME;
    *auto_cleanup = DEFAULT_AUTORMID;
    *place_memory = DEFAULT_MEM_PLACE;
    debug_flag = DEFAULT_DEBUG_FLAG;

    if(argc > 1)   {
	while((c = getopt(argc, argv, "s:t:i:wchmf:ap:v:")) != EOF) {
	    switch(c)  {
	    case 't':	{
		*num_threads = atoi(optarg);
		if(*num_threads <= 0) error = 1;
		break;
	    }
	    case 's':	{
		*each_part_size = atoi(optarg);
		if(*each_part_size <= 0) error = 1;
		break;
	    }
	    case 'i': {
		*iterations = atoi(optarg);
		if(*iterations <= 0) error = 1;
		break;
	    }
	    case 'w': {
		*write_only = 1;
		break;
	    }
	    case 'c': {
		*continuous = 1;
		break;
	    }
	    case 'm': {
		*write_my_area = 0;
		break;
	    }
	    case 'f': {
		*file_name = (char *) malloc(strlen(optarg) + 1);
		if(!*file_name) {
		    dprintf(0, ("Out of memory\n"));
		    exit(EXIT_FAILURE);
		}
		strcpy(*file_name, optarg);
		break;
	    }
	    case 'a': {
		*auto_cleanup = 1;
		break;
	    }
	    case 'p': {
		*place_memory = (char *) malloc(strlen(optarg) + 1);
		if(!*place_memory) {
		    dprintf(0, ("Out of memory\n"));
		    exit(EXIT_FAILURE);
		}
		strcpy(*place_memory, optarg);
		break;
	    }
	    case 'v': {
		debug_flag = atoi(optarg);
		if(debug_flag < 0) error = 1;
		break;
	    }
	    case 'h':
	    default:  {
		dprintf(0, (usage, argv[0], DEFAULT_PART_SIZE));
		if(c == 'h') 
		    exit(EXIT_SUCCESS);
		else exit(EXIT_FAILURE);
	    }
	    }
	}
    }

    if(*num_threads > *each_part_size) {
	dprintf(0, ("%s:  # threads must be <= # bytes per partition\n", 
		    argv[0]));
	dprintf(0, (usage, argv[0], DEFAULT_PART_SIZE));
	exit(EXIT_FAILURE);
    }
    if(error) {
	dprintf(0, ("%s:  all input numbers must be positive\n", argv[0]));
	dprintf(0, (usage, argv[0], DEFAULT_PART_SIZE));
	exit(EXIT_FAILURE);
    }
}


/* main */
int main(int argc, char *argv[])
{
    int num_parts, each_part_size, my_index;
    partid_t part_list[MAX_PARTITIONS];
    shared_t *share;
    partid_t my_partid;
    int tests[8];
    int num_tests = 8;
    int iterations, continuous, write_my_area, write_only;
    int data_file_fd;
    char *file_name;
    int num_threads, thread_num;
    int failed, status, i;
    key_t keys[MAX_PARTITIONS];
    int auto_cleanup;
    char *place_memory;

    parse_args(argc, argv, &num_threads, &each_part_size, &iterations, 
	       &continuous, &write_my_area, &write_only, &file_name,
	       &auto_cleanup, &place_memory);

    set_debug_level(debug_flag);

    /* get partition info and set up mapping between partitions */
    if((num_parts = part_getlist(part_list, MAX_PARTITIONS)) < 1) {
	dprintf(0, ("failed in part_getlist\n"));
	exit(EXIT_FAILURE);
    }
    stupid_sort(num_parts, part_list);

    print_hostnames(num_parts, part_list);
        
    share = (shared_t *) malloc (sizeof(shared_t) * num_parts);
    if(!share) {
	dprintf(0, ("out of memory\n"));
	exit(EXIT_FAILURE);
    }
    my_partid = part_getid();
    if(my_partid < 0) {
	dprintf(0, ("bad partid returned from part_getid\n"));
	exit(EXIT_FAILURE);
    }
    dprintf(5, ("I am part %d\n", my_partid));
    dprintf(5, ("sizeof each part (page size) %d\n", each_part_size));

    my_index = -1;
    for(i = 0; i < num_parts; i++) {
	if(my_partid == part_list[i]) {
	    my_index = i;
	    break;
	}
    }
    if(my_index < 0) {
	exit(EXIT_FAILURE);
    }

    /* get a key on this partition */
    keys[my_index] = xp_ftok(NULL, 1);
    if(keys[my_index] < 0) {
	exit(EXIT_FAILURE);
    }
    place_memory_if_needed(place_memory, keys[my_index]);

    /* create an initial barrier and spend parameters around to make
    ** sure all partitions agree on them */
    tests[0] = each_part_size;
    tests[1] = num_threads;
    tests[2] = write_my_area;
    tests[3] = iterations;
    tests[4] = continuous;
    tests[5] = write_only;
    tests[6] = file_name ? 1 : 0;
    tests[7] = auto_cleanup;
    if(barrier_init(num_parts, part_list, my_index, num_tests, tests, keys)) {
	dprintf(0, ("can't init barrier\n"));
	exit(EXIT_FAILURE);
    }

    /* fork threads and open data-file for text to pass over */
    thread_num = fork_threads(num_threads);

    if(file_name) {
	data_file_fd = open(file_name, O_RDONLY);
	if(data_file_fd < 3) {
	    dprintf(0, ("Can't open data file %s\n", file_name));
	    exit(EXIT_FAILURE);
	}
    } else {
	data_file_fd = DEFAULT_FILE_ID;
    }

    if(setup_mapped_areas(num_parts, share, keys, my_index, each_part_size,
			  auto_cleanup)) {
	dprintf(0, ("ERROR: in setting up maps\n"));
	exit(EXIT_FAILURE);
    }
    
    /* make sure that even if we aren't doing any reads, that all the
     * threads have made it through the attach code, so that an AUTORMID
     * won't cause a segment to be removed -- because one thread can
     * finish all it's work before another can attach -- then it looks like
     * there are failures.
     */
    if(barrier_to_all_parts(num_parts, my_index, num_threads,
			    thread_num)) {
	dprintf(0, ("ERROR: in barrier_to_all_parts before main sharing!"));
	exit(EXIT_FAILURE);
    }


    if(thread_num == 0) {
	print_placed_memory(place_memory, share[my_index].shmid);
    }

    /* pass info back and forth on shared memory and then clean up */
    failed = share_memory(num_parts, share, my_index, each_part_size, 
			  iterations, continuous, write_my_area, 
			  num_threads, thread_num, write_only, 
			  data_file_fd);

    if(remove_shared_segments(num_parts, thread_num, my_index, share,
			      auto_cleanup)) {
	dprintf(0, ("ERROR: in removing mapped areas\n"));
	return(EXIT_FAILURE);
    }
    if(thread_num == 0) {
	if(!failed) dprintf(1, ("Success!\n"));
	for(i = 1; i < num_threads; i++) wait(&status);
	barrier_close(my_index, thread_num);
    }
    return(EXIT_SUCCESS);
}


/* stupid_sort
** -----------
** sort the partition list.
*/
void stupid_sort(int num_parts, partid_t *part_list)
{
    int i, j, least;
    partid_t temp;

    for(i = 0; i < num_parts; i++) {
	least = i;
	for(j = i + 1; j < num_parts; j++) {
	    if(part_list[j] < part_list[least]) {
		least = j;
	    }
	}
	temp = part_list[i];
	part_list[i] = part_list[least];
	part_list[least] = temp;
    }
}

/* print_hostnames
** ---------------
** print out all hostsnames
*/
void print_hostnames(int num_parts, partid_t *part_list)
{
    char name[256];
    int i;

    for(i = 0; i < num_parts; i++) {
	if(part_gethostname(part_list[i], name, 256) < 0) {
	    dprintf(0, ("ERROR: Couldn't get host name for part %d\n", 
			part_list[i]));
	    exit(EXIT_FAILURE);
	}
	dprintf(1, ("partition #%d: %s\n", part_list[i], name));
    }
}


/* place_memory_if_needed
** ----------------------
** If the place_memory flag is set, then set up the given key to place
** memory close together.  
*/
void place_memory_if_needed(char *place_memory, key_t key)
{
    pmo_handle_t mld_handle, mldset, pm;
    policy_set_t policy_set;
    raff_info_t rafflist;

    /* nothing to place, so just ignor this part */
    if(!place_memory) {
	return;
    }

    if((mld_handle = mld_create(0, 0)) < 0) {
	dprintf(0, ("couldn't mld_create!!!\n"));
	exit(EXIT_FAILURE);
    }

    if((mldset = mldset_create(&mld_handle, 1)) < 0) {
        dprintf(0, ("couldn't mldset_create\n"));
        exit(EXIT_FAILURE);
    } 
    
    rafflist.resource = place_memory;
    rafflist.restype = RAFFIDT_NAME;
    rafflist.reslen = (ushort)strlen(place_memory);
    rafflist.radius = 0;
    rafflist.attr = RAFFATTR_ATTRACTION;

    if(mldset_place(mldset, TOPOLOGY_PHYSNODES, &rafflist, 1,
                    RQMODE_ADVISORY) < 0) {
        dprintf(0, ("couldn't mldset_place\n"));
        exit(EXIT_FAILURE);
    }

    if(part_setpmo(key, mld_handle, __PMO_MLD) < 0) {
	dprintf(0, ("couldn't place key with mld interface\n"));
	exit(EXIT_FAILURE);
    }
}


/* setup_mapped_areas
** ------------------
** call the appropriate calls to get all shared areas opened up.
*/
int setup_mapped_areas(int num_parts, shared_t *share, key_t *keys, 
		       int my_index, int each_part_size, int auto_cleanup)
{
    int i;

    for(i = 0; i < num_parts; i++) {
	share[i].shmid = xp_shmget(keys[i], num_parts * each_part_size, 0);
	if(share[i].shmid < 0) {
	    dprintf(0, ("ERROR: can't xp_shmget key 0x%x\n", keys[i]));
	    return(EXIT_FAILURE);
	}
	if(auto_cleanup) {
	    if(xp_shmctl(share[i].shmid, IPC_AUTORMID) < 0) {
		dprintf(0, ("ERROR: can't xp_shmctl shmid 0x%x AUTORMID "
			    "errno = %d\n", share[i].shmid, errno));
		return(EXIT_FAILURE);
	    }
	}
	share[i].addr = xp_shmat(share[i].shmid, NULL, 0);
	if(!share[i].addr) {
	    dprintf(0, ("ERROR: can't xp_shmat shmid 0x%x, key 0x%x\n",
			share[i].shmid, keys[i]));
	    return(EXIT_FAILURE);
	}
    }
    return(EXIT_SUCCESS);
}

/* print_placed_memory
** -------------------
** go through and print out the hwgraph placement of the shmid if this 
** memory was place (even if it was not, but don't expect anything).
*/
void print_placed_memory(char *place_memory, int shmid)
{
    char hwpath[256];
    int error;

    error = part_getnode(shmid, hwpath, 256);
    if(place_memory && error < 0) {
	dprintf(1, ("shmid %x hwgraph path with errno = %d\n", shmid, errno));
    } else if(place_memory) {
	dprintf(1, ("shmid %x hwgraph path with path = %s\n", shmid, hwpath));
    } else if(error < 0) {
	dprintf(1, ("shmid %x hwgraph didn't expect path and got none "
		    "errno = %d\n",
		    shmid, errno));
    } else {
	dprintf(1, ("shmid %x hwgraph didn't expect path but got %s\n",
		    shmid, hwpath));
    }
}

/* die_now_parent
** --------------
** Catch the SIGCHLD signal and die
*/
void die_now_parent(void)
{
    dprintf(0, ("ERROR: One of my children has died, killing"
		" myself in sorrow\n"));
    exit(EXIT_FAILURE);
}


/* die_now_child
** -------------
** catch the SIGHUP from our parent dying and die now
*/
void die_now_child(void)
{
    dprintf(0, ("ERROR: My parent has died, killing myself in sorrow\n"));
    exit(EXIT_FAILURE);
}


/* fork_threads
** ------------
** fork all the thread and set up signal handlers
*/
int fork_threads(int num_threads)
{
    int i;
    pid_t pid;
    int ret = 0;
    void (*orig)();

    for(i = 1; i < num_threads; i++) {
	pid = fork();
	if(pid > 0) {
	    ret = i;
	    break;
	}
	if(pid < 0) {
	    dprintf(0, ("ERROR: Couldn't create child number %d\n", i));
	    perror("fork_threads");
	    return pid;
	}
    }
    if(pid == 0) {
	orig = signal(SIGCHLD, die_now_parent);
    } else {
	orig = signal(SIGHUP, die_now_child);
    }
    if(orig == SIG_ERR) {
	dprintf(5, ("thread #%d bad return from signal, errno = %d\n", 
		    ret, errno));
    }
    return(ret);
}


/* share_memory
** ------------
** loop through and write and read from shared memory as many times
** as we are told to
*/
int share_memory(int num_parts, shared_t *share, int my_index, 
		 int each_part_size, int iters, int continuous,
		 int write_my_area, int num_threads, int thread_num,
		 int write_only, int data_file_fd)
{
    struct stat buf;
    off_t len, offset, i;

    /* find out the size of the file that we are going to share */
    if(data_file_fd != DEFAULT_FILE_ID) {
	if(fstat(data_file_fd, &buf)) {
	    dprintf(0, ("ERROR: can't get file stats\n"));
	    return(EXIT_FAILURE);
	}
	len = buf.st_size;
    } else {
	len = strlen(TEST_TEXT);
    }

    /* read and write to shared memory */
    for(i = 0; i < iters || continuous; i++) {
	offset = i % len;
	if(thread_num == 0) dprintf(1, ("."));
	if(write_to_memory(num_parts, share, my_index, each_part_size, 
			   write_my_area, num_threads, thread_num, 
			   data_file_fd, len, offset))
	    return(EXIT_FAILURE);
	if(!write_only) {
	    if(barrier_to_all_parts(num_parts, my_index, num_threads,
				    thread_num) ||
	       read_from_memory(num_parts, share, each_part_size, 
				write_my_area, num_threads, 
				data_file_fd, len, offset) ||
	       barrier_to_all_parts(num_parts, my_index, num_threads,
				    thread_num)) {
		return(EXIT_FAILURE);
	    }
	}
    }
    if(thread_num == 0) dprintf(1, ("\n"));
    return(EXIT_SUCCESS);
}


/* write_to_memory 
** --------------- 
** write to our section (our section is specified by our partition
** number and our thread number).  If we are doing a block section
** writing, then each partition number and thread number writes to a
** contiguous block of memory.  Otherwise, we write to an intertwined
** section of memory.
*/
int write_to_memory(int num_parts, shared_t *share, int my_index,
		    int each_part_size, int write_my_area, 
		    int num_threads, int thread_num, int data_file_fd,
		    off_t len, off_t file_offset)
{
    int sharei, index, texti, offset, extra, before, bytes_read;
    char *text = TEST_TEXT;
    char ch;

    /* figure out the extra bits and pieces of addition we need to do if
    ** our number of threads does not nicely divide into the section of 
    ** shared memory that we are writing to. */
    extra = thread_num < (each_part_size % num_threads) ? 1 : 0; 
    before = thread_num < (each_part_size % num_threads) ? thread_num :
	(each_part_size % num_threads);
    dprintf(10, ("extra = %d, thread_num = %d, each_part = %d\n",
		extra, thread_num, each_part_size));

    /* for each shared section (one per partition), write out info */
    for(sharei = 0; sharei < num_parts; sharei++) {
	if(data_file_fd == DEFAULT_FILE_ID) {
	    texti = file_offset;
	} else {
	    if(lseek(data_file_fd, file_offset, SEEK_SET) < 0) {
		dprintf(0, ("ERROR: can't lseek into file\n"));
		return(EXIT_FAILURE);
	    }
	}
	for(index = 0; index < ((each_part_size / num_threads) + extra); 
	    index++) {
	    if(write_my_area) {
		offset = (my_index * each_part_size) + index + before;
		offset += (each_part_size / num_threads) * thread_num;
	    } else {
		offset = my_index + (index * num_threads * num_parts);
		offset += thread_num * num_parts;
	    }

	    /* either write from our text string or the file */
	    if(data_file_fd == DEFAULT_FILE_ID) {
		share[sharei].addr[offset] = text[texti];
		texti++;
		if(texti == len) texti = 0;
	    } else {
		bytes_read = read(data_file_fd, &ch, 1);
		if(bytes_read < 0) {
		    dprintf(0, ("ERROR: reading data file\n"));
		    return(EXIT_FAILURE);
		}
		if(bytes_read == 0) {
		    if(lseek(data_file_fd, 0, SEEK_SET) < 0) {
			dprintf(0, ("ERROR: can't lseek into file\n"));
			return(EXIT_FAILURE);
		    }
		    index--;
		    continue;
		}
		dprintf(10, ("VERB W: sharei %d: partid %d: thread %d: "
			     "index %d: offset %d: char %c\n", sharei, 
			     my_index, thread_num, 
			     index, offset, ch));
		share[sharei].addr[offset] = ch;
	    }
	}
    }
    return(EXIT_SUCCESS);
}


/* read_from_memory 
** ----------------
** Go through and check that we get the writing that we expect for all
** partitions and all threads.
*/
int read_from_memory(int num_parts, shared_t *share, int each_part_size, 
		     int write_my_area, int num_threads, int data_file_fd,
		     off_t len, off_t file_offset)
{
    int sharei, index, texti, part_index;
    int thread_index, offset, extra, before, bytes_read;
    char *text = TEST_TEXT;
    char ch;

    /* for each shared memory segment (one per partition) */
    for(sharei = 0; sharei < num_parts; sharei++) {

	/* for each partition, each has a write section in each shared
	** memory segment */
	for(part_index = 0; part_index < num_parts; part_index++) {

	    /* for each thread */
	    for(thread_index = 0; thread_index < num_threads; thread_index++) {

		/* find the inital placement of data */
		extra = thread_index < (each_part_size % num_threads) ? 1 : 0; 
		before = thread_index < (each_part_size % num_threads) ? 
		    thread_index : (each_part_size % num_threads);
		if(data_file_fd == DEFAULT_FILE_ID) {
		    texti = file_offset;
		} else {
		    if(lseek(data_file_fd, file_offset, SEEK_SET) < 0) {
			dprintf(0, ("ERROR: can't lseek into file\n"));
			return(EXIT_FAILURE);
		    }
		}

		/* index through the number of writes for this partition
		** and thread that we are checking the writes of */
		for(index = 0; index < ((each_part_size/num_threads)+extra); 
		    index++) {
		    if(write_my_area) {
			offset = index + before;
			offset += (part_index * each_part_size);
			offset += (each_part_size/num_threads) * thread_index;
		    } else {
			offset = part_index;
			offset += index * num_threads * num_parts;
			offset += thread_index * num_parts;
		    }

		    /* get the data we expect to find */
		    if(data_file_fd == DEFAULT_FILE_ID) {
			ch = text[texti];
			texti++;
			if(texti == len) texti = 0;
		    } else {
			bytes_read = read(data_file_fd, &ch, 1);
			if(bytes_read < 0) {
			    dprintf(0, ("ERROR: reading data file\n"));
			    return(EXIT_FAILURE);
			}
			if(bytes_read == 0) {
			    if(lseek(data_file_fd, 0, SEEK_SET) < 0) {
				dprintf(0, ("ERROR: can't lseek into file\n"));
				return(EXIT_FAILURE);
			    }
			    index--;
			    continue;
			} 
		    }	

		    /* check it against what is there */
		    if(share[sharei].addr[offset] != ch) {
			dprintf(0, ("ERROR: Found bad text: expecting a "
				    "%c and got a %c\n",
				    share[sharei].addr[offset], ch));
			dprintf(5, ("sharei %d: partid %d: thread %d: "
				    "index %d: offset %d\n", sharei, 
				    part_index, thread_index, 
				    index, offset));
			return(EXIT_FAILURE);
		    }
		    dprintf(10, ("VERB R: sharei %d: partid %d: thread %d: "
				"index %d: offset %d: map %c: file %c\n", 
				 sharei, part_index, thread_index, index, 
				 offset, share[sharei].addr[offset], ch));
		}
	    }
	}
    }
    return(EXIT_SUCCESS);
}

/* remove_shared_segments
** ----------------------
** Dettach all areas and then if we are the first thread and it is our
** index remove the shared memory segment.
*/
int remove_shared_segments(int num_parts, int thread_num, int my_index, 
			   shared_t *share, int auto_cleanup)
{
    int i;

    for(i = 0; i < num_parts; i++) {
	if(xp_shmdt(share[i].addr) < 0) {
#if (_MIPS_SIM == _ABI64)
	    dprintf(0, ("ERROR: can't xp_shmdt address: 0x%llx for "
#else 
	    dprintf(0, ("ERROR: can't xp_shmdt address: 0x%x for "
#endif
			"shmid: 0x%x\n", share[i].addr, share[i].shmid));
	    return(EXIT_FAILURE);
	}
	if(i == my_index && thread_num == 0 && !auto_cleanup) {
	    if(xp_shmctl(share[i].shmid, IPC_RMID) < 0) {
		dprintf(0, ("ERROR: can't xp_shmctl to remove shmid 0x%x\n",
			    share[i].shmid));
		return(EXIT_FAILURE);
	    }
	}
    }
    return(EXIT_SUCCESS);
}
