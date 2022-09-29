#include <stdio.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syssgi.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <procfs/procfs.h>

typedef unsigned long long iotimer_t;
extern int errno;

extern        int start_counters( int e0, int e1 );
extern        int read_counters( int e0, long long *c0, int e1, long long *c1);
extern         int print_counters( int e0, long long c0, int e1, long long c1);
int fd;
hwperf_profevctrarg_t evargs;

volatile __psunsigned_t phys_addr, raddr;
volatile unsigned int cycleval;
volatile iotimer_t prev_value, cur_value, *iotimer_addr;

void
timer_init()
{
	int fd, poffmask;

	poffmask = getpagesize() - 1;
	phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval);
	raddr = phys_addr & ~poffmask;
	fd = open("/dev/mmem", O_RDONLY);
	iotimer_addr = (volatile iotimer_t *)mmap(0, poffmask, PROT_READ,
						  MAP_PRIVATE, fd, (off_t)raddr);
	iotimer_addr = (iotimer_t *)((__psunsigned_t)iotimer_addr +
				     (phys_addr & poffmask));

	return;
}

void
time_start()
{
	prev_value = *iotimer_addr;
}



double
time_end()
{
	iotimer_t x;

	x = *iotimer_addr;

	return (double)((x - prev_value) * (double)cycleval * 1e-6);
}

	

static int gen_start, gen_read;
main(int argc, char *argv[]) {
        long long c0, c1;
	int how = atoi(argv[1]);
	int x;
	double timeme;
        if((start_counters(1,17)) !=0) {
                perror("start_counters");
        }

	if (syssgi(214, 1001, 0, 0, 0, 0) == -1) {
		printf("error num %d\n", errno);
		exit(-1);
	}

	timer_init();

	/* start the counters */
	time_start();
	ioctl(fd, PIOCENEVCTRS, (void *)&evargs);

	while (how)
		how--;
	/* stop and release the counters */
	ioctl(fd, PIOCRELEVCTRS) ;

	if (syssgi(214, 1002, 0, 0, 0, 0) == -1) {
		printf("error num %d\n", errno);
		exit(-1);
	}

        if((read_counters(1,&c0,17,&c1)) !=0) {
                perror("read_counters");
        }
	timeme = time_end();

	printf("issued inst: %lld, grad inst: %lld %f time\n",c0,c1, timeme);
	
}

int 
start_counters(int event_type0, int event_type1) {
  int i,events=0;    
  int pid = getpid();
  char    pfile[32],*p;
  short mode=0x0;
    
  sprintf(pfile, "/proc/%05d", pid);
  if ((fd = open(pfile, O_RDWR)) < 0) {
    perror("Can't open /proc/pid");
    exit(1);
  }

  mode = HWPERF_CNTEN_U;
   for (i = 0; i < HWPERF_EVENTMAX; i++ ) {
    evargs.hwp_ovflw_freq[i] = 0;      /* no frequency interrupts */
    if (i == event_type0 || i == event_type1) {
      evargs.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_mode = mode;
      evargs.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_ie = 1;
      evargs.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_ev = i <
HWPERF_CNT1BASE ? i : i - HWPERF_CNT1BASE;
    } else
      evargs.hwp_evctrargs.hwp_evctrl[i].hwperf_spec = 0;
  }

/* start the counters */
  if ((gen_start = ioctl(fd, PIOCENEVCTRS, (void *)&evargs)) < 0) {
    perror("prioctl PIOCENEVCTRS returns error");
    exit(1);
  }
  return 0;
}


int 
read_counters(int event_type0, long long *mcount0,                
	int event_type1, long long *mcount1) {
  hwperf_cntr_t   cnts;
    if ((gen_read = ioctl(fd, PIOCGETEVCTRS, (void *)&cnts)) < 0) {
	perror("prioctl PIOCGETEVCTRS returns error");
        exit(1);
    }

  close(fd);
  *mcount0 = (long long) (cnts.hwp_evctr[event_type0]);
  *mcount1 = (long long) (cnts.hwp_evctr[event_type1]);
  return 0;
}
 

