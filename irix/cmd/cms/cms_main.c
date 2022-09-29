#include "cms_base.h"
#include "cms_message.h"
#include "cms_info.h"
#include "cms_cell_status.h"
#include "cms_trace.h"
#include "sys/mman.h"
#include "cms_membership.h"

static void cms_init_cell_status(void);
static void cms_map_cell_status_file(void);
static void cms_memb_test(void);
static void cms_start_cell(int cell);

extern	void cms_monitor(int);


cell_t			local_cell;
cms_cell_status_t	*cms_cell_status;
int			cms_num_cells;
extern	int		num_test_funcs;

char	*cell_tty[MAX_CELLS];

void
main(int argc, char **argv)
{
	int	test_num = 0;
	int	i;
	int	pflag = 0;
	int	tflag = 0;
	int	c;
	int	ttyno;


	if (argc == 1) {
		fprintf(stderr, "%s -n <num cells> "
			"[-t<test_num>] [-p ttynos]\n", argv[0]);
		exit(1);
	}	

	while ((c = getopt(argc, argv, "n:p:t:")) != EOF) {
                switch (c) {
                case 'n' :
                        cms_num_cells = atoi(optarg);
                        break;

                case 't' :
			tflag++;
                        test_num = atoi(optarg);
                        break;

                case 'p' :
			pflag = 1;
			for (ttyno = 0; optind <= argc;  ttyno++) {
				if ( ttyno >= cms_num_cells)
					break;
				cell_tty[ttyno] = argv[optind -1 ];
				optind++;
			}
			printf("ttyno. %d cms_num_cells %d optind %d \n",
				ttyno, cms_num_cells, optind);
			if (ttyno < cms_num_cells) {
				fprintf(stderr,
					"Not enough ttys to be useful\n");
				pflag = 0;
			}
                        break;

                default :
			fprintf(stderr, "%s -n <num cells> "
				"[-t<test_num>] [-p ttynos]\n", argv[0]);
			exit(1);
                }
        }

	cms_map_cell_status_file();

	/*
	 * Start the membership daemons.
	 */
	for (i = 0; i < cms_num_cells; i++) {
		if (fork() == 0) {
			char	dev_tty[20];
			/*
			 * Open the given per cell ttys.
			 */
			if (pflag) {
				close(1);
				strcpy(dev_tty, "/dev/");
				strcat(dev_tty, cell_tty[i]);
				if (open(dev_tty, O_RDWR) < 0) {
					perror(cell_tty[i]);
					exit(1);
				}
				close(2);
				dup(1);
			}

			printf("Starting cell %d\n", i);
			cms_start_cell(i);
			exit(1);
		}
	}

	if (tflag) {
		if ((test_num < 0) || (test_num > num_test_funcs)) {
			printf("Invalid test number should between 1 and %d\n",
						num_test_funcs);
			exit(1);
		}
		cms_monitor(test_num - 1 );
	} else {
		printf("Running all tests\n");
		cms_monitor(-1);
	}
}


static void
cms_start_cell(int cell)
{
	local_cell = cell;


	cms_map_cell_status_file();
	cip = &cms_cell_status->cms_info[cellid()];
	cms_get_config_info();
	cms_init();
	ep_mesg_init();
	cms_init_cell_status();
	/* cms_memb_test();  */
	cms_print_cell_status();
	cms();
}

/* REFERENCED */
void
cms_memb_test(void)
{
	cell_set_t	membership;
	int	i, j;

	for (i = 0; i < cip->cms_num_cells; i++) {
		cms_set_cell_status(i, B_TRUE);
		for (j = 0; j < cip->cms_num_cells; j++)
			cms_set_link_status(i, j, B_TRUE);
	}
			
	cms_set_link_status(3,2, B_FALSE);
	for (i = 0; i < cip->cms_num_cells; i++) {
		local_cell = i;
		hb_get_recv_set(&cip->cms_recv_set[i]);
                cms_trace(CMS_TR_MEMB_RECV_SET, i, cip->cms_recv_set[i], 0);
	}

	cms_membership_compute(&membership, B_FALSE);
	printf("Membership printed is %x\n", membership);
	exit(1);
}

void
cms_get_config_info(void)
{
	FILE *fp; 
	char config_file[20];
	char config_str[80];

	sprintf(config_file, "ms_config");

	fp = fopen(config_file, "r");
	
	if (fp ==  NULL) {
		perror("fopen");
		fprintf(stderr,"Cannot open config file %s using defaults\n", 
						config_file);
		cip->cms_num_cells = cms_num_cells;
		cip->cms_follower_timeout = 20;
		cip->cms_leader_timeout = 6;
		cip->cms_nascent_timeout = 5;
		cip->cms_tie_timeout = 5;
	} else  {
		while (fgets(config_str, sizeof(config_str), fp) != NULL) {
			if (config_str[0] == '#') {
				continue;
			}
			sscanf(config_str,"%3d %3d %3d %3d %3d",
				&cip->cms_num_cells, 
				&cip->cms_follower_timeout,
				&cip->cms_leader_timeout, 
				&cip->cms_nascent_timeout,
				&cip->cms_tie_timeout);
		}
		fclose(fp);
	}

	printf("Config data for cell %d\n", cellid());
	printf("Nascent timeout: %3d\n", cip->cms_nascent_timeout);
	printf("Leader timeout: %3d\n", cip->cms_leader_timeout);
	printf("Follower timeout: %3d\n", cip->cms_follower_timeout);
	printf("Tie timeout: %3d\n", cip->cms_tie_timeout);
	printf("Num cells: %3d\n", cip->cms_num_cells);
}

void
hb_get_connectivity_set(cell_set_t *send_set, cell_set_t *recv_set)
{
	hb_get_send_set(send_set);
	hb_get_recv_set(recv_set);
}


void
hb_get_send_set(cell_set_t *send_set)
{
	cell_t cell;

	set_init(send_set);
	for (cell = 0; cell < cip->cms_num_cells; cell++) {
		if (cms_get_cell_status(cell) && 
				cms_get_link_status(cellid(), cell))
			set_add_member(send_set, cell);
	}
	for (cell = 0; cell < cip->cms_num_cells; cell++) {
		if (set_is_member(send_set, cell))
			if (!cms_get_link_status(cellid(), cell))
				abort();
	}
}

void
hb_get_recv_set(cell_set_t *recv_set)
{
	cell_t cell;

	set_init(recv_set);
	for (cell = 0; cell < cip->cms_num_cells; cell++) {
		if (cms_get_cell_status(cell) && 
				cms_get_link_status(cell, cellid()))
			set_add_member(recv_set, cell);
	}
}

void
cms_reset_cell(cell_t cell)
{
	cms_set_cell_status(cell, B_FALSE);
}

static void
cms_map_cell_status_file(void)
{
	int	fd;

	fd  = open(CELL_STATUS_FILE,O_RDWR|O_CREAT, 0644);

	if (fd == -1) {
		perror("open");
		fprintf(stderr,"Cannot open %s\n", CELL_STATUS_FILE);
		exit(1);
	}

	cms_cell_status = (cms_cell_status_t *)
				mmap(0, sizeof(cms_cell_status_t), 
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_AUTOGROW, fd, 0);

	if ((void * )cms_cell_status == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
}

void
cms_init_cell_status(void)
{
	int	j;
	/*
	 * Initialize the cells 
	 */
	cms_cell_status->cms_cell_alive[cellid()] = TRUE;
	for (j = 0; j < cip->cms_num_cells; j++)
		cms_cell_status->cms_link_status[cellid()][j] = TRUE;

}

void
cms_print_cell_status(void)
{
	int 	i, j;

	for (i = 0; i < cip->cms_num_cells; i++) {
		printf("Cell %d is %s\n", i,
				cms_get_cell_status(i) ? "alive" : "dead");
		printf("Cell %d is connected to ");
		for (j = 0; j < cip->cms_num_cells; j++)
			if (cms_get_link_status(i,j)) printf("%d ",j);
		printf("\n");
	}
}

/*
 * cms_dead:
 * Dead state. This is used only for testing.
 */
void
cms_dead(void)
{
	printf("%d died\n",cellid());
	ep_mesg_close();

	cms_set_cell_status(cellid(), B_FALSE);
	for (;;) {
		delay(DEAD_TIME);
		if (cms_get_cell_status(cellid()))
			break;
	}

	printf("%d rebooting\n",cellid());
	bzero(cip, sizeof(cms_info_t));
	cip->cms_leader = CELL_NONE;	/* No leader yet */
	cms_get_config_info();
	ep_mesg_init();
	cms_set_state(CMS_NASCENT);
	return;
}
