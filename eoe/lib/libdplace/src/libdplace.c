/*
 *   libdplace.c
 *   jlr@sgi.com
 */

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mutex.h>
#include <sys/pmo.h>
#include <strings.h>
#include <fmtmsg.h>
#include <ulocks.h>
#include <sys/syssgi.h>
#include <abi_mutex.h>
#include <errno.h>
#include "place_defs.h"
#include <sgidefs.h>
#include <sys/resource.h>
#include <invent.h>

#define MAP_FILE "/dev/zero"

extern int   _fdata[], _edata[];
extern void  *_SBRK(ssize_t);
extern FILE  *yyin;
extern void  __ateachexit(void());
extern void  migr_policy_args_init(migr_policy_uparms_t *);
extern dev_t __mld_to_node(pmo_handle_t);

static int  yyparse(void);
static void *__dplace_arena;
int         __dplace_fileio;
char        *__dplace_command;

/* Structure which holds options, setting them while configuration from
 * environment variables and the placement file takes place.
 */
struct dplace_info
{
    unsigned long       tcount;
    abilock_t           is_running;
    char                *inputfile;
    int                 migration_enabled;
    __uint64_t          dpagesize, spagesize, tpagesize;
    int                 dpagewait, spagewait, tpagewait;
    __uint64_t          migration;
    __uint64_t          stack_min, stack_max, data_min, data_max;
    char                stack_policy[PM_NAME_SIZE];
    char                data_policy[PM_NAME_SIZE];
    char                text_policy[PM_NAME_SIZE];
    int                 verbose, inherit, disable;
    rqmode_t            rq_mode;
    int                 mustrun;
    int                 line_count;
    int                 *thread_pid, thread_max;
    int                 nthreads, nnodes;
    int                 nodes_set, threads_set;
    int                 affinity_set, topology_set;
    int                 dpagesize_set, spagesize_set, tpagesize_set;
    int                 dpagewait_set, spagewait_set, tpagewait_set;
    int                 migration_set, initial_policies;
    char                *device[128];
    enum topology_type  topology;
    enum affinity_type  affinity;
    struct
    {
        int             number;
        node_mapping_t  *maps;
    }                   node_maps;
    struct
    {
        int     count;
        link_t  *links;
    }                   thread_link;
    pmo_handle_t        mldset_handle;
    pmo_handle_t        *mldlist;
    int                 mld_offset;
    int                 memory_fd;
} *d;

/* Called on errors. Sends SIGKILL to everyone in this process group. */
static
void
die(void)
{
    kill(-getpid(), SIGKILL);
    exit(1);
}

/* Do an arena realloc, and die if it fails */
static
void*
__dplace_realloc_check(void *p, size_t n)
{
    void *np = arealloc(p, n, __dplace_arena);

    if (0 == np)
    {
        fprintf(stderr, "No more space to arealloc().\n");
        die();
    }
    return np;
}

/* Do an arena malloc, and die if it fails */
void*
__dplace_malloc_check(size_t n)
{
    void *np = amalloc(n, __dplace_arena);

    if (0 == np)
    {
        fprintf(stderr, "No more space to amalloc().\n");
        die();
    }
    return np;
}

/* Do an arena calloc, and die if it fails */
static
void*
__dplace_calloc_check(size_t n, size_t m)
{
    void *np = acalloc(n, m, __dplace_arena);

    if (0 == np)
    {
        fprintf(stderr, "No more space to acalloc().\n");
        die();
    }
    return np;
}

/* Free an arena */
static
void
free_check(void *addr)
{
    afree(addr, __dplace_arena);
}

/* Setup default values of "d" structure */
static
void
initialize_values(void)
{
    d->tcount = 1;
    d->inputfile = NULL;
    d->dpagesize = getpagesize();
    d->spagesize = d->dpagesize;
    d->tpagesize = d->dpagesize;
    d->dpagewait = 0;
    d->spagewait = 0;
    d->tpagewait = 0;
    d->migration_enabled = 0;
    d->migration = 0;
    /*d->stack_min, d->stack_max, d->data_min, d->data_max; */
    d->verbose = 0;
    d->inherit = 0;
    d->disable = 0;
    d->rq_mode = RQMODE_ADVISORY;
    d->mustrun = 0;
    d->line_count = 0;
    __dplace_fileio = 0;
    /*__dplace_command; */
    d->thread_max = 128;
    d->thread_pid = (int *) __dplace_malloc_check(d->thread_max * sizeof (int));
    d->nthreads = 0;
    d->nnodes = 0;
    d->nodes_set = 0;
    d->threads_set = 0;
    d->affinity_set = 0;
    d->topology_set = 0;
    d->dpagesize_set = 0;
    d->spagesize_set = 0;
    d->tpagesize_set = 0;
    d->dpagewait_set = 0;
    d->spagewait_set = 0;
    d->tpagewait_set = 0;
    d->migration_set = 0;
    d->initial_policies = 0;
    /*char *d->device[128]; */
    d->topology = cluster;
    d->affinity = None;
    /*struct { int number; node_mapping_t *maps; } d->node_maps; */
    d->thread_link.count = 0;
    d->thread_link.links = (link_t *) 0;
    d->mldset_handle = 0;
    /*pmo_handle_t *d->mldlist; */
    d->mld_offset = 0;
    init_lock(&d->is_running);
    strcpy(d->stack_policy, "PlacementThreadLocal");
    strcpy(d->data_policy, "PlacementThreadLocal");
    strcpy(d->text_policy, "PlacementDefault");
}

/* Set stack and data minimum and maximum values */
static
void
do_limits(void)
{
    struct rlimit rlp;

    if (getrlimit(RLIMIT_STACK, &rlp) < 0)
    {
        perror("getrlimit(RLIMIT_STACK, &rlp) failed\n");
        die();
    }

    d->stack_max = TOPOFSTACK;
    d->stack_min = d->stack_max - (long long) rlp.rlim_cur;

    /* this was a bad idea because it caused enormous core files */
    /* d->data_min = *(long long *)d->stack_min; touch the bottom of stack */
#if 0
    if (d->verbose)
        fprintf(stderr, "stack: 0x%010llx to 0x%010llx\n", d->stack_min,
                        d->stack_max);
#endif /* 0 */

    if (getrlimit(RLIMIT_DATA, &rlp) < 0)
    {
        perror("getrlimit(RLIMIT_DATA, &rlp) failed\n");
        die();
    }
    d->data_min = (long long) _fdata;
    d->data_max = d->data_min + rlp.rlim_cur;

#if 0
    if (d->verbose)
        fprintf(stderr, "data:  0x%010llx to 0x%010llx\n", d->data_min,
                        d->data_max);
#endif /* 0 */
}

/* Get the dplace command-line arguments passed through environment
 * variables.
 */
static
void
do_args(void)
{
    char        *c;
    static char rld_list[BUFSIZ], buffer[BUFSIZ];

    /* Specify the placement file */
    c = getenv("__DPLACE_PLACEFILE_");
    if (NULL == c)
    {
        /* library initializaion ... leave quietly */
        return;
    }
    putenv("__DPLACE_PLACEFILE_=/dev/null");    /* for future generations */

    if (!strcmp(c, "/dev/null"))    /* Special case /dev/null */
    {
        d->inputfile = NULL;
        __dplace_fileio = 0;
    } else {
        d->inputfile = c;
        __dplace_fileio = 1;
    }

    /* Specify the data page size */
    c = getenv("__DPLACE_DPAGESIZE_");
    if (NULL == c)
    {
        fprintf(stderr, "dplace data_pagesize parameter error.\n");
        die();
    }
    d->dpagesize = strtoull(c, (char **) NULL, 10);

    /* Specify the stack page size */
    c = getenv("__DPLACE_SPAGESIZE_");
    if (NULL == c)
    {
        fprintf(stderr, "dplace stack_pagesize parameter error.\n");
        die();
    }
    d->spagesize = strtoull(c, (char **) NULL, 10);

    /* Specify the text page size */
    c = getenv("__DPLACE_TPAGESIZE_");
    if (NULL == c)
    {
        fprintf(stderr, "dplace text_pagesize parameter error.\n");
        die();
    }
    d->tpagesize = strtoull(c, (char **) NULL, 10);

    /* Specify that we should wait for data segment large pages */
    c = getenv("__DPLACE_DPAGEWAIT_");
    if (NULL == c)
    {
        fprintf(stderr, "dplace data_lpage_wait parameter error.\n");
        die();
    }
    if (!strcmp("on", c))
        d->dpagewait = 1;
    else if (!strcmp("off", c))
        d->dpagewait = 0;
    else {
        int n;

        if (1 != sscanf(c, "%d", &n))
        {
            fprintf(stderr, "dplace data_lpage_wait parameter error.\n");
            die();
        }
        d->dpagewait = n ? 1 : 0;
    }

    /* Specify that we should wait for stack segment large pages */
    c = getenv("__DPLACE_SPAGEWAIT_");
    if (NULL == c)
    {
        fprintf(stderr, "dplace stack_lpage_wait parameter error.\n");
        die();
    }
    if (!strcmp("on", c))
        d->spagewait = 1;
    else if (!strcmp("off", c))
        d->spagewait = 0;
    else {
        int n;

        if (1 != sscanf(c, "%d", &n))
        {
            fprintf(stderr, "dplace stack_lpage_wait parameter error.\n");
            die();
        }
        d->spagewait = n ? 1 : 0;
    }

    /* Specify that we should wait for text segment large pages */
    c = getenv("__DPLACE_TPAGEWAIT_");
    if (NULL == c)
    {
        fprintf(stderr, "dplace text_lpage_wait parameter error.\n");
        die();
    }
    if (!strcmp("on", c))
        d->tpagewait = 1;
    else if (!strcmp("off", c))
        d->tpagewait = 0;
    else {
        int n;

        if (1 != sscanf(c, "%d", &n))
        {
            fprintf(stderr, "dplace text_lpage_wait parameter error.\n");
            die();
        }
        d->tpagewait = n ? 1 : 0;
    }

    /* Turn on migration. If specified as a number also set the migration
     * level as indicated.
     */
    c = getenv("__DPLACE_MIGRATION_");
    if (NULL == c)
    {
        fprintf(stderr, "dplace migration parameter error.\n");
        die();
    }
    if (!strcmp("on", c))
        d->migration_enabled = 1;
    else if (!strcmp("off", c))
        d->migration_enabled = 0;
    else {
        int n;

        if (1 != sscanf(c, "%d", &n))
        {
            fprintf(stderr, "dplace migration parameter error.\n");
            die();
        }
        d->migration_enabled = n ? 1 : 0;
        d->migration = n;
    }

    /* Specify the migration level */
    c = getenv("__DPLACE_MIGRATION_LEVEL_");
    if (c)
        d->migration = 100 - strtoull(c, (char **) NULL, 10);

    /* Turn on verbose messages */
    c = getenv("__DPLACE_VERBOSE_");
    if (!c)
    {
        fprintf(stderr, "dplace verbose parameter error.\n");
        die();
    }
    if (!strcmp(c, "on"))
        d->verbose = 1;

    /* Specify whether child processes should inherit placement attributes */
    c = getenv("__DPLACE_INHERIT_");
    if (c)
        if (!strcmp(c, "on"))
            d->inherit = 1;

    /* Indicates the parent process, basically the initial thread which
     * utilized this library.
     */
    c = getenv("__DPLACE_PARENT_");
    if (!c)
    {
        fprintf(stderr, "dplace parent pid parameter error.\n");
        die();
    } else {
        pid_t dplace_pid = (pid_t) atol(c);

#if 0
        fprintf(stderr, "dplace_pid = %d, parent = %d\n", dplace_pid,
                        getppid());
#endif /* 0 */
        if ((d->inherit == 0) && (dplace_pid != getppid()))
            d->disable = 1;
        if (d->inherit)
        {
            sprintf(buffer, "__DPLACE_PARENT_=%d\n", getpid());
            putenv(buffer);
        }
    }

    /* remove libdplace.so from _RLD_LIST in case exec's are used
     * only if d->inherit == 0
     */
    if (d->disable && (NULL != (c = getenv("_RLD_LIST"))))
    {
        char *newrld = (char *) strchr(c, ':');

        if (newrld)
        {
            sprintf(rld_list, "_RLD_LIST=%s", newrld+1);
            putenv(rld_list);
        }
    }

    /* Turn on mandatory placement */
    if (getenv("__DPLACE_MANDATORY_"))
    {
        if (d->verbose)
            fprintf(stderr, "Mandatory placement.\n");
        d->rq_mode = RQMODE_MANDATORY;
    }

    /* Do a mustrun on threads during placement */
    if (getenv("__DPLACE_MUSTRUN_"))
    {
        if (d->verbose)
            fprintf(stderr, "Mustrunning threads.\n");
        d->mustrun = 1;
    }
}

/* Convenience for repeatedly checking return codes */
static
int
checkerr(int code)
{
    if (code < 0)
    {
        /* This message seems misleading since it's not just misconfiguration
         * which could cause bad return codes. Oh well.
         */
        perror("Check placement file");
        die();
    }
    return 0;
}

/* Attach a thread to a MLD. If we are to mustrun also attach to a
 * particular CPU.
 */
static
void
attach_pid_to_mld(int pid, pmo_handle_t mld, int thread)
{
    if (!d->mustrun)
        checkerr(process_mldlink(pid, mld, d->rq_mode));
    else {
        int     i, cpu = 0;
        dev_t   node = __mld_to_node(mld);

        for (i = 0; i < thread; i++)
            if (node == __mld_to_node(d->mldlist[d->thread_link.links[i].node + d->mld_offset]))
                cpu ^= 1;
        /* make sure the cpu exists */
        {
            char        devname[128], path[128];
            int         length = sizeof (devname);
            struct stat sinfo;

            sprintf(path, "%s/cpu/%c",
                          dev_to_devname(node, devname, &length),
                          "ab"[cpu]);
            if (stat(path, &sinfo))
                cpu ^= 1;
            if (d->verbose)
            {
                length = sizeof (devname);
                fprintf(stderr, "Attaching thread %d to %s/cpu/%c\n",
                                thread,
                                dev_to_devname(node, devname, &length),
                                "ab"[cpu]);
            }
        }
        checkerr(process_cpulink(pid, mld, cpu, d->rq_mode));
    }
}

/* Attach a thread to a particular MLD and CPU. */
static
void
attach_pid_to_mld_and_cpu(int pid, pmo_handle_t mld, int cpu)
{
    checkerr(process_cpulink(pid, mld, cpu, d->rq_mode));
}

/* This is called every time a child thread sproc's */
void
__dplace_sproc_child(void)
{
    int i;

    i = add_then_test(&d->tcount, 1);
    if (d->verbose)
    {
        spin_lock(&d->is_running);
        fprintf(stderr, "There are now %d threads\n", i);
    }
    if (d->disable)
        return;
    if (i >= d->thread_max)
    {
        d->thread_max *= 2;
        d->thread_pid = (int *) __dplace_realloc_check(d->thread_pid, d->thread_max * sizeof (int));
        bzero((void *) &d->thread_pid[d->thread_max/2],
              (d->thread_max/2) * sizeof (int));
    }
    d->thread_pid[i-1] = (int) getpid();
    if ((i <= d->nthreads) && (d->thread_link.links[i-1].valid))
    {
        int pid = d->thread_pid[i-1];

        if (d->thread_link.links[i-1].cpu_link)
        {
            if (d->verbose)
                fprintf(stderr, "Attaching process [%d] to memory [%d] using cpu [%d]\n",
                                pid, d->thread_link.links[i-1].node,
                                d->thread_link.links[i-1].cpu);
            attach_pid_to_mld_and_cpu(pid,
                                      d->mldlist[d->thread_link.links[i-1].node + d->mld_offset],
                                      d->thread_link.links[i-1].cpu);
        } else {
            if (d->verbose)
                fprintf(stderr, "Attaching process [%d] to memory [%d]\n", pid,
                                d->thread_link.links[i-1].node);
            attach_pid_to_mld(pid,
                              d->mldlist[d->thread_link.links[i-1].node + d->mld_offset],
                              i-1);
        }
    }
    if (d->verbose)
        release_lock(&d->is_running);
}

/* set up arena */
static
void
do_memory(void)
{
    int          fd, npages = 512;
    unsigned int *mapped;
    void         *ac;

    if (__dplace_arena == (void *) 0)
    {
        if ((fd = open(MAP_FILE, O_RDWR)) < 0)
        {
            perror("Cannot open /dev/zero");
            die();
        }
        mapped = (unsigned int *) mmap(NULL, npages * getpagesize(),
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED | MAP_AUTORESRV, fd, 0);
        if (!mapped)
        {
            perror("mmap of /dev/zero failed");
            die();
        }
        __dplace_arena = (void *) mapped;
        ac = acreate(__dplace_arena, npages * getpagesize(), MEM_NOAUTOGROW,
                     0, 0);
        if (!ac)
        {
            perror("Arena creation failed.\n");
            die();
        }
        d = (struct dplace_info *)
                            __dplace_malloc_check(sizeof (struct dplace_info));

        d->memory_fd = fd;
    } else {
        fd = d->memory_fd;
        mapped = (unsigned int *) mmap(__dplace_arena, npages * getpagesize(),
                                       PROT_READ | PROT_WRITE,
                                       MAP_SHARED | MAP_AUTORESRV, fd, 0);
        if (NULL == mapped)
        {
            perror("mmap of /dev/zero failed");
            die();
        }
    }
}

/* Yacc placement file processing error message output. */
static
void
yyerror(char *s)
{
    fprintf(stderr, "%s on line %d.\n", s, d->line_count+1);
    fprintf(stderr, "Aborting.\n");
    die();
}

/* Turn on migration from placement file. */
static
void
set_migration(int n)
{
    if (0 == d->migration_set)
    {
        if ((n > 100) || (n < 0))
        {
            fprintf(stderr, "Placement error on line %d: ", d->line_count+1);
            fprintf(stderr, "Migration level must be in the range [0,100]\n");
            fprintf(stderr, "Aborting.\n");
            die();

        } else {
            d->migration_set = 1;
            d->migration = 100 - n;
        }
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr, "Migration level has already been set to %d %%.\n",
                (int) d->migration);
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Set data segment page size from placmeent file. */
static
void
set_dpagesize(int n)
{
    if (0 == d->dpagesize_set)
    {
        d->dpagesize_set = 1;
        d->dpagesize = n;
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr,
                "Global data_pagesize has already been set to %lldk.\n",
                d->dpagesize >> 10);
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Set stack segment page size from placmeent file. */
static void
set_spagesize(int n)
{
    if (0 == d->spagesize_set)
    {
        d->spagesize_set = 1;
        d->spagesize = n;
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr,
                "Global stack_pagesize has already been set to %lldk.\n",
                d->spagesize >> 10);
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Set text segment page size from placmeent file. */
static
void
set_tpagesize(int n)
{
    if (0 == d->tpagesize_set)
    {
        d->tpagesize_set = 1;
        d->tpagesize = n;
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count + 1);
        fprintf(stderr,
                "Global text_pagesize has already been set to %lldk.\n",
                d->tpagesize >> 10);
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Turn on or off data segement large page wait option from placement file. */
static
void
set_dpagewait(int n)
{
    if (0 == d->dpagewait_set)
    {
        d->dpagewait_set = 1;
        d->dpagewait = n;
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr,
                "Global data_lpage_wait has already been turned %s.\n",
                d->dpagewait ? "ON" : "OFF");
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Turn on or off stack segement large page wait option from placement file. */
static
void
set_spagewait(int n)
{
    if (0 == d->spagewait_set)
    {
        d->spagewait_set = 1;
        d->spagewait = n;
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr,
                "Global stack_lpage_wait has already been turned %s.\n",
                d->spagewait ? "ON" : "OFF");
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Turn on or off text segement large page wait option from placement file. */
static
void
set_tpagewait(int n)
{
    if (0 == d->tpagewait_set)
    {
        d->tpagewait_set = 1;
        d->tpagewait = n;
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr,
                "Global stack_lpage_wait has already been turned %s.\n",
                d->tpagewait ? "ON" : "OFF");
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Set up the affinity list from the placement file. */
static
void
set_affinity(struct list f_list)
{
    struct list *p = &f_list;

    if (0 == d->affinity_set)
    {
        d->affinity = Close;

        while (p)
        {
            if (d->verbose)
                fprintf(stderr, "raff[%d] = %s\n", d->affinity_set, p->name);
            d->device[d->affinity_set] = p->name;
            p = p->next;
            d->affinity_set++;
        }

    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr, "Affinity has already been set.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Set the mldset topology from the placment file. */
static
void
set_topology(int n)
{
    if (0 == d->topology_set)
    {
        d->topology_set = 1;
        d->topology = (enum topology_type) n;
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr, "Topology has already been set.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
}

/* Not sure what this is for, but it's set from the placement file. */
static
void
set_nodes(int n)
{
    if (n == 0)
        return;
    if (0 == d->nodes_set)
    {
        d->nodes_set = 1;
        d->nnodes = n;
        d->node_maps.number = n;
        d->node_maps.maps = (node_mapping_t *)
                            __dplace_calloc_check(n, sizeof (node_mapping_t));
        if (!d->node_maps.maps)
        {
            fprintf(stderr, "No space for %d memories...\n", n);
            die();
        }
    } else {
        fprintf(stderr, "Placement error on line %d:", d->line_count+1);
        fprintf(stderr, "Memories have already been set to %d.\n", d->nnodes);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (d->verbose)
        fprintf(stderr, "Setting memories = %d\n", n);
}

/* Not sure what this is for, but it's set from the placement file. */
static
void
set_threads(int n)
{
    if (n == 0)
        return;
    if (d->nthreads != 0)
    {
        if (n != d->nthreads)
        {
            fprintf(stderr, "Placement error on line %d:", d->line_count+1);
            fprintf(stderr, "Threads has already been set to %d.\n",
                            d->nthreads);
            fprintf(stderr, "Aborting.\n");
            die();
        }
    } else {
        d->nthreads = n;
        d->threads_set = 1;
        d->thread_link.count = n;
        d->thread_link.links = (link_t *)
                                    __dplace_calloc_check(n, sizeof (link_t));
        if (!d->thread_link.links)
        {
            fprintf(stderr, "No space for %d threads...\n", n);
            die();
        }
    }
    if (n >= d->thread_max)
    {
        d->thread_pid = (int *)
                    __dplace_realloc_check(d->thread_pid, n * sizeof (int));
        bzero((void *) &d->thread_pid[d->thread_max],
              (n - d->thread_max) * sizeof (int));
        d->thread_max = n;
    }
    if (d->verbose)
        fprintf(stderr, "Setting threads = %d\n", n);
}

/* Not sure what this is for, but it's set from the placement file. */
static
void
migrate_range(__uint64_t min, __uint64_t max, int node)
{
    if (d->verbose)
    {
        fprintf(stderr,
                "Migrate range [%010llx,%010llx] to memory %d.\n",
                min, max, node);
    }
    checkerr(migr_range_migrate((void *) min, (size_t) (max-min),
                                (pmo_handle_t) d->mldlist[node+d->mld_offset]));
}

/* Get the first thread's placement policies set up correctly */
static
void
setup_first_thread_and_policy(void)
{
    policy_set_t            policy_set;
    pmo_handle_t            pm;
    migr_policy_uparms_t    migr;
    int                     pgsz = getpagesize();

    if (d->verbose)
        fprintf(stderr, "Setting up policies and initial thread.\n");
    /* First attach thread to particular MLD or cpu */
    if (d->threads_set && (1 == d->thread_link.links[0].valid))
    {
        int pid = d->thread_pid[0];

        if (d->thread_link.links[0].cpu_link)
        {
            if (d->verbose)
                fprintf(stderr, "Attaching process [%d] to memory [%d] using cpu [%d]\n",
                                 pid, d->thread_link.links[0].node,
                                 d->thread_link.links[0].cpu);
            attach_pid_to_mld_and_cpu(pid,
                                      d->mldlist[d->thread_link.links[0].node + d->mld_offset],
                                      d->thread_link.links[0].cpu);
        } else {
            if (d->verbose)
                fprintf(stderr, "Attaching process [%d] to memory [%d]\n", pid,
                                d->thread_link.links[0].node);
            attach_pid_to_mld(pid,
                              d->mldlist[d->thread_link.links[0].node + d->mld_offset],
                              0);
        }
    }

    /* Create a default policy set */
    pm_filldefault(&policy_set);

    /* Set migration parameters. */
    if (d->verbose)
    {
        if (d->migration_enabled)
            fprintf(stderr, "Migration is on and level is %d%%.\n",
                            100 - (int) d->migration);
        else
            fprintf(stderr, "Migration is off.\n");
    }
    migr_policy_args_init(&migr);
    migr.migr_base_enabled = d->migration_enabled ? SETON : SETOFF;
    migr.migr_base_threshold = d->migration;
    policy_set.migration_policy_name = "MigrationControl";
    policy_set.migration_policy_args = (void *) &migr;

    /* Set up data segment placement and large page policy. */
    if (d->verbose)
        fprintf(stderr, "Creating data PM.\n");
    if (d->mldset_handle)
    {
        policy_set.placement_policy_name = d->data_policy;
        policy_set.placement_policy_args = (void *) d->mldset_handle;
    } else
        policy_set.placement_policy_args = (void *) 1;
    if (d->verbose)
        fprintf(stderr, "Data placement policy is %s.\n",
                        policy_set.placement_policy_name);
    policy_set.page_size = d->dpagesize;
    if (d->verbose)
        fprintf(stderr, "Data pagesize is %lldk.\n", d->dpagesize >> 10);
    policy_set.policy_flags |= d->dpagewait ? POLICY_PAGE_ALLOC_WAIT : 0;
    if (d->verbose && d->dpagewait)
        fprintf(stderr, "Will wait for large data pages.\n");
    pm = pm_create(&policy_set);
    checkerr(pm);
    if (d->verbose)
        fprintf(stderr, "Setting data PM.\n");
    checkerr(pm_setdefault(pm, MEM_DATA));

    /* Set up stack segment placement and large page policy. */
    if (d->verbose)
        fprintf(stderr, "Creating stack PM.\n");
    if (d->mldset_handle)
    {
        policy_set.placement_policy_name = d->stack_policy;
        policy_set.placement_policy_args = (void *) d->mldset_handle;
    } else {
        policy_set.placement_policy_args = (void *) 1;
    }
    if (d->verbose)
        fprintf(stderr, "Stack placement policy is %s.\n",
                        policy_set.placement_policy_name);
    policy_set.page_size = d->spagesize;
    if (d->verbose)
        fprintf(stderr, "Stack pagesize is %lldk.\n", d->spagesize >> 10);
    /* Reset pagewait since the data placement policy may have set it */
    policy_set.policy_flags &= ~POLICY_PAGE_ALLOC_WAIT;
    policy_set.policy_flags |= d->spagewait ? POLICY_PAGE_ALLOC_WAIT : 0;
    if (d->verbose && d->spagewait)
        fprintf(stderr, "Will wait for large stack pages.\n");
    pm = pm_create(&policy_set);
    checkerr(pm);
    if (d->verbose)
        fprintf(stderr, "Setting stack PM.\n");
    checkerr(pm_setdefault(pm, MEM_STACK));

    /* Set up text segment placement and large page policy. */
    if ((pgsz != d->tpagesize)
        || strcmp(d->text_policy, "PlacementDefault")
        || d->tpagewait)
    {
        if (d->verbose)
            fprintf(stderr, "Creating text PM.\n");
        if (d->mldset_handle)
        {
            policy_set.placement_policy_name = d->text_policy;
            policy_set.placement_policy_args = (void *) d->mldset_handle;
        } else {
            policy_set.placement_policy_args = (void *) 1;
        }
        if (d->verbose)
            fprintf(stderr, "Text placement policy is %s.\n",
                            policy_set.placement_policy_name);
        policy_set.page_size = d->tpagesize;
        if (d->verbose)
            fprintf(stderr, "Text pagesize is %lldk.\n", d->tpagesize >> 10);
        /* Reset pagewait since the stack placement policy may have set it */
        policy_set.policy_flags &= ~POLICY_PAGE_ALLOC_WAIT;
        policy_set.policy_flags |= d->tpagewait ? POLICY_PAGE_ALLOC_WAIT : 0;
        if (d->verbose && d->tpagewait)
            fprintf(stderr, "Will wait for large text pages.\n");
        pm = pm_create(&policy_set);
        checkerr(pm);
        if (d->verbose)
            fprintf(stderr, "Setting text PM.\n");
        checkerr(pm_setdefault(pm, MEM_TEXT));
    }
    d->initial_policies = 1;
}

static
void
add_range(__uint64_t min, __uint64_t max, int node, int size)
{
    int             count;
    policy_set_t    policy_set;
    pmo_handle_t    pm;
    long long       edge;

    if (d->verbose)
        fprintf(stderr,
                "Place range [0x%010llx,0x%010llx] on memory %d with pagesize %dkbytes.\n",
                min, max, node, size >> 10);

    if (!d->nodes_set)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count + 1);
        fprintf(stderr, "Ranges cannot be placed until memories are declared.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (node >= d->nnodes)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Memory number (%d) is >= %d.\n", node, d->nnodes);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (min >= max)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Address %010llx is >= %010llx.\n", min, max);
        fprintf(stderr, "Aborting.\n");
        die();
    }

    /* attach range to mld here */
    /* first we must do initial policies */
    if (!d->initial_policies)
        setup_first_thread_and_policy();

    /* we can't attach the range unless the boundary of the heap (or stack)
     * overlaps the address range, so we will queue stuff up and do it by
     * spying on brk() and sbrk() We can do the stack (or anonymous) now since
     * we already touched the first (lowest address) page of the stack
     */
    edge = (long long) _SBRK(0);
    if ((min > d->stack_min) || (max <= edge))
    {
        /* We have an address that is not in the unallocated heap so do it
         * now!
         */
        pm_filldefault(&policy_set);
        policy_set.placement_policy_name = "PlacementFixed";
        policy_set.placement_policy_args = (void *) d->mldlist[node+d->mld_offset];
        policy_set.page_size = size;
        if (d->verbose)
            fprintf(stderr, "Creating PM with PlacementFixed.\n");
        pm = pm_create(&policy_set);
        checkerr(pm);
        if (d->verbose)
            fprintf(stderr, "Attaching PM with %s.\n",
                            policy_set.placement_policy_name);
        checkerr(pm_attach(pm, (void *) min, (size_t) (max-min)));

        /* if this is data, it has already been touched so we need to migrate */
        if (max <= edge && min >= d->data_min)
            migrate_range(min, (max <= edge) ? max : edge, node);
        else if (min >= d->stack_min)
        {
            /* if this is stack, we need to touch, then migrate */
            volatile int phoney = *(int *) min;

            migrate_range(min, max, node);
        }
    } else {
        /* here we queue it up */
        if (d->verbose)
            fprintf(stderr,
                    "Queue range [0x%010llx,0x%010llx] on memory %d with pagesize %dkbytes.\n",
                    min, max, node, size >> 10);
        count = d->node_maps.maps[node].count;
        d->node_maps.maps[node].ranges =
            (range_t *) __dplace_realloc_check(d->node_maps.maps[node].ranges,
                                               (count + 1) * sizeof (range_t));
        d->node_maps.maps[node].ranges[count].start = min;
        d->node_maps.maps[node].ranges[count].end = max;
        d->node_maps.maps[node].ranges[count].node = node;
        d->node_maps.maps[node].ranges[count].pagesize = size;
        d->node_maps.maps[node].ranges[count].placed = 0;
        d->node_maps.maps[node].count++;
    }
}

/* Link up a thread with the MLD it will be placed on. From placement file. */
static
void
add_link(int thread, int node)
{
    if (!d->threads_set)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread linkage cannot be done until threads are declared.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (!d->nodes_set)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread linkage cannot be done until memories are declared.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (node >= d->nnodes)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Memory number (%d) is >= %d.\n", node, d->nnodes);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (thread >= d->nthreads)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread number (%d) is >= %d.\n", thread, d->nthreads);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (d->thread_link.links[thread].valid)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread %d is already linked to node %d\n",
                        thread, d->thread_link.links[thread].node);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    d->thread_link.links[thread].valid = 1;
    d->thread_link.links[thread].thread = thread;
    d->thread_link.links[thread].node = node;
    d->thread_link.links[thread].cpu_link = 0;

    if (d->thread_pid[thread])  /* thread already exists */
    {
        int pid = d->thread_pid[thread];

        if (d->verbose)
            fprintf(stderr, "Attaching process [%d] to memory [%d]\n", pid,
                            node);
        attach_pid_to_mld(pid, d->mldlist[node+d->mld_offset], thread);
        d->thread_link.links[thread].valid++;
    }
}

/* Link up a thread with the CPU it will be placed on. From placement file. */
static
void
add_cpulink(int thread, int node, int cpu)
{
    if (!d->threads_set)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread linkage cannot be done until threads are declared.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (!d->nodes_set)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread linkage cannot be done until memories are declared.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (node >= d->nnodes)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Memory number (%d) is >= %d.\n", node, d->nnodes);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (thread >= d->nthreads)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread number (%d) is >= %d.\n", thread, d->nthreads);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (d->thread_link.links[thread].valid)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread %d is already linked to node %d\n",
                thread, d->thread_link.links[thread].node);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (cpu > 1)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "cpu number must be 0 or 1 on this system\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    d->thread_link.links[thread].valid = 1;
    d->thread_link.links[thread].thread = thread;
    d->thread_link.links[thread].node = node;
    d->thread_link.links[thread].cpu = cpu;
    d->thread_link.links[thread].cpu_link = 1;
    if (d->thread_pid[thread])
    {                            /* thread already exists */
        int pid = d->thread_pid[thread];

        if (d->verbose)
            fprintf(stderr,
                    "Attaching process [%d] to memory [%d] using cpu [%d]\n",
                    pid, node, cpu);
        attach_pid_to_mld_and_cpu(pid, d->mldlist[node + d->mld_offset], cpu);
        d->thread_link.links[thread].valid++;
    }
}

/* Distribute threads in a manner according to placement file. */
static
void
distribute_threads(int distribution, int value, int t0, int t1, int dt,
                   int tmode, int m0, int m1, int dm, int mmode)
{
    int nthreads, nnodes;

    if (!d->threads_set)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread distribution cannot be done until threads are declared.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (!d->nodes_set)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread distribution cannot be done until memories are declared.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (0 == d->nnodes)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Cannot distribute across 0 memories.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (0 == d->nthreads)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "There are n threads to distribute.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (tmode == 0)
    {
        t0 = 0;
        t1 = d->nthreads - 1;
        dt = 1;
    }
    if ((t0 < 0) || (t0 > d->nthreads))
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "The lower thread limit is out of range.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (mmode == 0)
    {
        m0 = 0;
        m1 = d->nnodes - 1;
        dm = 1;
    }
    if ((m0 < 0) || (m0 > d->nnodes))
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "The lower memory limit is out of range.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (dt > 0)
        nthreads = (t1+1-t0) / dt;
    else
        nthreads = (t0+1-t1) / (-dt);
    if (dm > 0)
        nnodes = (m1+1-m0) / dm;
    else
        nnodes = (m0+1-m1) / (-dm);
    if (distribution == dist_default)
    {
        value = (nthreads+nnodes-1) / nnodes;
        distribution = block;
    }
    if (distribution == block)
    {
        int i;

        if (value < (nthreads/nnodes))
        {
            fprintf(stderr, "Error on line %d.\n", d->line_count+1);
            fprintf(stderr, "Blocking factor is too small.\n");
            fprintf(stderr, "Aborting.\n");
            die();
        }
        if (d->verbose)
            fprintf(stderr, "Block distribution: %d threads per memory\n",
                            value);
        for (i = 0; i < nthreads; i++)
        {
            int thread, mem;

            thread = t0 + i*dt;
            mem = m0 + (i/value)*dm;
            if (((dt >= 0) && (thread > t1)) || ((dm >= 0) && (mem > m1)) ||
                ((dt < 0) && (thread < t1)) || ((dm < 0) && (mem < m1)))
            {
                fprintf(stderr, "Error on line %d.\n", d->line_count+1);
                fprintf(stderr, "Illegal distribution range.\n");
                fprintf(stderr, "Aborting.\n");
                die();
            } else {
                add_link(thread, mem);
                if (d->verbose)
                    fprintf(stderr, "Thread %d will run on memory %d\n",
                            thread, mem);
            }
        }
    } else if (distribution == cyclic) {
        int i;

        if (value < 1)
        {
            fprintf(stderr, "Error on line %d.\n", d->line_count+1);
            fprintf(stderr, "Cyclic blocking factor must be larger than 1.\n");
            fprintf(stderr, "Aborting.\n");
            die();
        }
        if (d->verbose)
            fprintf(stderr, "Cyclic distribution by %d\n", value);

        for (i = 0; i < nthreads; i++)
        {
            int thread, mem;

            thread = t0 + i*dt;
            mem = m0 + ((i/value)%nnodes)*dm;
            if (((dt >= 0) && (thread > t1)) || ((dm >= 0) && (mem > m1)) ||
                ((dt < 0) && (thread < t1)) || ((dm < 0) && (mem < m1)))
            {
                fprintf(stderr, "Error on line %d.\n", d->line_count+1);
                fprintf(stderr, "Illegal distribution range.\n");
                fprintf(stderr, "Aborting.\n");
                die();
            } else {
                add_link(thread, mem);
                if (d->verbose)
                    fprintf(stderr, "Thread %d will run on memory %d\n",
                                    thread, mem);
            }
        }
    }
}

/* Change the linkage between a thread and a MLD. */
static
void
move_link(int thread, int node)
{
    if (!d->threads_set)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread linkage cannot be done until threads are declared.\n");
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (node >= d->nnodes)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Memory number (%d) is >= %d.\n", node, d->nnodes);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (thread >= d->nthreads)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread number (%d) is >= %d.\n", thread, d->nthreads);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (!d->thread_pid[thread])
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Thread %d doesn't yet exist, so can't be moved.\n",
                        thread);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    d->thread_link.links[thread].valid = 1;
    d->thread_link.links[thread].thread = thread;
    d->thread_link.links[thread].node = node;

    if (d->verbose)
        fprintf(stderr, "Attaching process [%d] to memory [%d]\n",
                        d->thread_pid[thread], node);
    attach_pid_to_mld(d->thread_pid[thread], d->mldlist[node+d->mld_offset],
                      thread);
}

/* Change the linkage between a process and a MLD. */
static
void
move_pid(int pid, int node)
{
    int i;

    if (node >= d->nnodes)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "Memory number (%d) is >= %d.\n", node, d->nnodes);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    for (i = 0; i < d->nthreads; i++)
        if (pid == d->thread_pid[i])
            break;
    if (d->nthreads == i)
    {
        fprintf(stderr, "pid [%d] does not exist.\n", pid);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if (d->verbose)
        fprintf(stderr, "Attaching process [%d] to memory [%d]\n", pid, node);

    attach_pid_to_mld(pid, d->mldlist[node + d->mld_offset], i);
}

/* Spill our guts about how everything is placed. */
static
void
show_layout(void)
{
    int i, j;

    fprintf(stderr, "Placement info summary:\n");
    fprintf(stderr, "Default data_pagesize %lldkbytes.\n", d->dpagesize >> 10);
    if (d->dpagewait)
        fprintf(stderr, "Will wait for large data pages.\n");
    fprintf(stderr, "Default stack_pagesize %lldkbytes.\n", d->spagesize >> 10);
    if (d->spagewait)
        fprintf(stderr, "Will wait for large stack pages.\n");
    fprintf(stderr, "Default text_pagesize %lldkbytes.\n", d->tpagesize >> 10);
    if (d->tpagewait)
        fprintf(stderr, "Will wait for large text pages.\n");
    if (d->migration_enabled)
        fprintf(stderr, "Migration is on and level is %d%%.\n",
                100 - (int) d->migration);
    else
        fprintf(stderr, "Migration is off.\n");
    fprintf(stderr, "Migration level is %d%%.\n", (int) d->migration);
    fprintf(stderr, "%d memories in topology", d->nnodes);
    if (d->topology == cube)
        fprintf(stderr, " cube");
    else if (d->topology == cluster)
        fprintf(stderr, " cluster");
    else if (d->topology == none)
        fprintf(stderr, " none");
    else if (d->topology == physical)
        fprintf(stderr, " physical");
    if (d->affinity == Close)
    {
        fprintf(stderr, " with d->affinity to ");
        for (i = 0; i < d->affinity_set; i++)
            fprintf(stderr, "%s ", d->device[i]);
    } else if(d->affinity == None)
        fprintf(stderr, " with no d->affinity");
    fprintf(stderr, "\n");
    fprintf(stderr, "%d threads.\n", d->nthreads);
    for (i = 0; i < d->node_maps.number; i++)
    {
        for (j = 0; j < d->node_maps.maps[i].count; j++)
        {
            fprintf(stderr, "Place range [%010llx,%010llx] on memory %d with pagesize %dkbytes.\n",
                            d->node_maps.maps[i].ranges[j].start,
                            d->node_maps.maps[i].ranges[j].end,
                            d->node_maps.maps[i].ranges[j].node,
                            d->node_maps.maps[i].ranges[j].pagesize >> 10);
        }
    }
    for (i = 0; i < d->thread_link.count; i++)
        if (d->thread_link.links[i].valid)
            fprintf (stderr, "Linking thread %d to memory %d\n",
                             d->thread_link.links[i].thread,
                             d->thread_link.links[i].node);
}

/* Efficiently compute ceil(log(n)/log(2)) */
int
ceil_log2(int n)
{
    int count = 0;

    n--;
    while (n > 0)
    {
        count++;
        n = n >> 1;
    }
    return count;
}

/* Do the actual MLD placement */
static
void
place_nodes(void)
{
    mld_info_t              mld_info;
    mldset_placement_info_t mldset_placement_info;
    raff_info_t             *rafflist;

    int i, n_mld, min_mld = d->nnodes;

    if (d->topology == cube)
    {
        n_mld = 1 << ceil_log2(min_mld);
        d->mld_offset = (n_mld-min_mld+1) / 2;
    } else {
        n_mld = d->nnodes;
        d->mld_offset = 0;
    }
    d->mldlist = (pmo_handle_t *) __dplace_malloc_check(n_mld * sizeof (pmo_handle_t));

    if (d->verbose)
        fprintf(stderr, "Creating %d MLDS.\n", n_mld);

    for (i = 0; i < n_mld; i++)
    {
        mld_info.radius = 1;    /* local to one node */
        if ((i >= d->mld_offset) && (i < (d->mld_offset + min_mld)))
            mld_info.size = 1;
        else
            mld_info.size = 0;
        d->mldlist[i] = mld_create(mld_info.radius, mld_info.size);
        if (d->verbose)
            fprintf(stderr, "Created MLD [%d]\n", d->mldlist[i]);
        checkerr(d->mldlist[i]);
    }

    if (d->verbose)
        fprintf(stderr, "Setting up mldset\n");

    if (d->verbose)
        fprintf(stderr, "Creating the mldset\n");

    d->mldset_handle = mldset_create(d->mldlist, n_mld);
    checkerr(d->mldset_handle);

    /*
     * Setup mldset_placement_info
     */
    mldset_placement_info.mldset_handle = d->mldset_handle;
    if (d->topology == cube)
    {
        mldset_placement_info.topology_type = TOPOLOGY_CUBE;
    } else if (d->topology == cluster)
    {
        mldset_placement_info.topology_type = TOPOLOGY_FREE;
    } else if (d->topology == none)
    {
        mldset_placement_info.topology_type = TOPOLOGY_FREE;
    } else if (d->topology == physical)
    {
        mldset_placement_info.topology_type = TOPOLOGY_PHYSNODES;
    }
    if (d->affinity_set)
    {
        rafflist = (raff_info_t *)
            __dplace_malloc_check(d->affinity_set * sizeof (raff_info_t));
        for (i = 0; i < d->affinity_set; i++)
        {
            rafflist[i].resource = (void *) d->device[i];
            rafflist[i].reslen = (unsigned short) strlen (d->device[i]);
            rafflist[i].restype = RAFFIDT_NAME;
            rafflist[i].radius = 1;
            rafflist[i].attr = RAFFATTR_ATTRACTION;
        }
        mldset_placement_info.rafflist = rafflist;
        mldset_placement_info.rafflist_len =
            (d->nnodes < d->affinity_set) ? d->nnodes : d->affinity_set;

    } else {
        mldset_placement_info.rafflist = 0;
        mldset_placement_info.rafflist_len = 0;
    }

    /*
     * Request mode 
     */
    mldset_placement_info.rqmode = d->rq_mode;

    if (d->topology == physical && d->affinity_set < d->nnodes)
    {
        fprintf(stderr, "Error on line %d.\n", d->line_count+1);
        fprintf(stderr, "The affinity count (%d) must be at least",
                        d->affinity_set);
        fprintf(stderr, " as large as the number of memories (%d).\n",
                        d->nnodes);
        fprintf(stderr, "Aborting.\n");
        die();
    }
    if(d->verbose)
       fprintf (stderr, "Placing the mldset\n");
    mldset_place(mldset_placement_info.mldset_handle,
                 mldset_placement_info.topology_type,
                 mldset_placement_info.rafflist,
                 mldset_placement_info.rafflist_len,
                 mldset_placement_info.rqmode);
    if (d->verbose)
    {
        dev_t   node_dev;
        char    devname[128];
        int     length;

        for (i = 0; i < min_mld; i++)
        {
            length = sizeof (devname);
            node_dev = __mld_to_node(d->mldlist[i + d->mld_offset]);
            printf("Memory %d is placed on %s\n", i,
                   dev_to_devname (node_dev, devname, &length));
        }
    }
}

/* Parses from a single line (command line?) instead of a file.
 * Not used by the dplace command, but might be useful for other
 * programs that want to use this library. A bit odd, however.
 */
void
dplace_line(char *line)
{
    unsigned long   n;
    char            buffer[BUFSIZ];

    spin_lock(&d->is_running);
    n = strlen(line);
    if ('\n' == line[n-1])
        sprintf(buffer, "%s", line);
    else
        sprintf(buffer, "%s\n", line);
    __dplace_fileio = 0;
    __dplace_command = buffer;
    yyparse();
    if (!d->initial_policies)
        setup_first_thread_and_policy();
    release_lock(&d->is_running);
}

/* Opens and starts parsing the placement file. */
void
dplace_file(char *filename)
{
    char buffer[BUFSIZ];
    FILE *fp;

    spin_lock(&d->is_running);
    /* Open the placement file */
    fp = fopen(filename, "r");
    if (NULL == fp)
    {
        fprintf(stderr, "Cannot open %s.\n", filename);
        die();
    }
    setbuf(fp, buffer);
    yyin = fp;              /* Point yacc at the right file */
    __dplace_fileio = 1;
    d->line_count = 0;
    yyparse();              /* Process the file */
    d->line_count = 0;
    /* If we haven't already done first thread setup, do it now*/
    if (!d->initial_policies)
        setup_first_thread_and_policy();
    release_lock(&d->is_running);
}

/* Use this during library debugging to show the final layout at
 * process exit. dlook can now do this for us, mostly.
 */
static void
__hasta_la_vista (void)
{
#if 0
    if (d->verbose && (d->thread_pid[0] == (int)getpid()))
    {
        fprintf(stderr,"dplace info...\n");
        show_layout();
    }
#endif /* 0 */
}

/* dplace library initialization */
void
__init_dplace(void)
{
    setbuf(stderr, 0);
    do_memory();
    initialize_values();
    do_args();
    if (d->disable)
        return;
    do_limits();
    if (atsproc_child(__dplace_sproc_child) < 0)
    {
        perror("atsproc_child");
        die();
    }
    if (atfork_child(__dplace_sproc_child) < 0)
    {
        perror("atfork_child");
        die();
    }
    d->thread_pid[0] = (int) getpid();
    if (d->verbose)
        fprintf(stderr, "There are now %d threads\n", (int) d->tcount);
    d->line_count = 0;
    if (d->inputfile)
        dplace_file(d->inputfile);
    d->line_count = 0;
    if (!d->initial_policies)
        setup_first_thread_and_policy();
    __ateachexit(__hasta_la_vista);
}

/* Don't know what this does. I suspect it is dead code. */
void
dplace_line_(char *line, int count)
{
    char buffer[BUFSIZ];

    spin_lock(&d->is_running);
    strncpy(buffer, line, count);
    buffer[count] = '\n';
    buffer[count + 1] = '\0';
    __dplace_fileio = 0;
    __dplace_command = buffer;
    yyparse();
    if (!d->initial_policies)
        setup_first_thread_and_policy();
    release_lock(&d->is_running);
}

/* Don't know what this does. I suspect it is dead code. */
void
dplace_file_(char *filename, int count)
{
    FILE *fp;
    char buffer[BUFSIZ];

    spin_lock(&d->is_running);
    strncpy(buffer, filename, count);
    buffer[count] = '\0';
    fp = fopen (filename, "r");
    if (NULL == fp)
    {
        fprintf(stderr, "Cannot open %s.\n", filename);
        die();
    }
    setbuf(fp, buffer);
    yyin = fp;
    __dplace_fileio = 1;
    d->line_count = 0;
    yyparse();
    d->line_count = 0;
    if (!d->initial_policies)
        setup_first_thread_and_policy();
    release_lock(&d->is_running);
}

/* Updates placement policy attachments when brk/sbrk is performed. */
static
void
place_queue(long long addr)
{
    int             i, j, k;
    __uint64_t      min, max;
    int             node, size;
    policy_set_t    policy_set;
    pmo_handle_t    pm;

#if 0
    if (errno)
        return;
#endif /* 0 */
    for (i = 0; i < d->nnodes; i++)
    {
        for (j = 0; j < d->node_maps.maps[i].count; j++)
        {
            if (d->node_maps.maps[i].ranges[j].end <= addr)
            {
                node = d->node_maps.maps[i].ranges[j].node;
                min = d->node_maps.maps[i].ranges[j].start;
                max = d->node_maps.maps[i].ranges[j].end;
                size = d->node_maps.maps[i].ranges[j].pagesize;

                pm_filldefault(&policy_set);
                policy_set.placement_policy_name = "PlacementFixed";
                policy_set.placement_policy_args = (void *) d->mldlist[node+d->mld_offset];
                policy_set.page_size = size;
                if (d->verbose)
                    fprintf(stderr, "Creating PM with PlacementFixed.\n");
                pm = pm_create(&policy_set);
                checkerr(pm);
                if (d->verbose)
                    fprintf(stderr, "Attaching PM with %s.\n",
                                    policy_set.placement_policy_name);
                checkerr(pm_attach(pm, (void *) min, (size_t) (max-min)));
                if (d->verbose)
                    fprintf(stderr,
                            "sbrk place range [0x%010llx,0x%010llx] on memory %d with pagesize %dkbytes.\n",
                            min, max, node, size >> 10);

                migrate_range(min, max, node);
                d->node_maps.maps[i].count--;
                /* eliminate entry */
                for (k = j; k < d->node_maps.maps[i].count; k++)
                    d->node_maps.maps[i].ranges[k] = d->node_maps.maps[i].ranges[k+1];
            }
        }
    }
}

/* Replaces standard brk with arena brk */
int 
brk(void *addr)
{
    extern int _brk(void *);
    int rv = _brk(addr);

    if (__dplace_arena && ((long long) addr >= (long long) _edata))
        place_queue((long long) addr);
    return rv;
}

/* Replaces standard sbrk with arena sbrk */
void*
SBRK(ssize_t size)
{
    void        *addr;
    long long   newaddr;

    addr = _SBRK(size);
    newaddr = (long long) addr + size;
    if (__dplace_arena && ((long long) newaddr >= (long long) _edata))
        place_queue((long long) newaddr);
    return addr;
}
