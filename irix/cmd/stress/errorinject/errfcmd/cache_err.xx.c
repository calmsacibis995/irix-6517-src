#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/fcntl.h>
#include <fs/procfs/procfs.h>
#include <syslog.h>


do_accesses(int size)
{
	char *tmp;
	int i, j;

	for (i = 0; i < size * 4; i += 4096) {
		tmp = (char *)malloc(4096);
		for (j = 0; j < 4096; j+= 128)
		    *(volatile char *)(tmp + j);
		free(tmp);	
	}

	return 0;
}

main(int argc, char *argv[])
{
	char file[32];
	unsigned long long old_cnt, old_miss;
	int fd, gen, gen1;
	hwperf_profevctrarg_t hwprof;
	hwperf_eventctrl_t *hwevent;	
	hwperf_cntr_t cnts;
	int size = strtol(argv[1], NULL, 0);

	printf("Running scache tests on size %d (%x)\n", size, size);

	hwevent = &hwprof.hwp_evctrargs;

	bzero(&hwprof, sizeof(hwperf_profevctrarg_t));

	sprintf(file, "/proc/%d", getpid());
	fd = open(file, O_RDWR);

	hwevent->hwp_evctrl[0].hwperf_creg.hwp_ev = HWPERF_C0PRFCNT0_SCDAECC;
	hwevent->hwp_evctrl[0].hwperf_creg.hwp_mode = HWPERF_CNTEN_U;
	hwevent->hwp_evctrl[HWPERF_CNT1BASE].hwperf_creg.hwp_ev = 
	    HWPERF_C0PRFCNT1_SDCMISS;
	hwevent->hwp_evctrl[HWPERF_CNT1BASE].hwperf_creg.hwp_mode = 
	    HWPERF_CNTEN_U;

	gen = ioctl(fd, PIOCENEVCTRS, (void *)&hwprof);
	if (gen < 0) {
		perror("evctrl ioctl");
		printf("Unable to enable event counting\n");
		exit(-1);
	}
	
	gen1 = ioctl(fd, PIOCGETEVCTRS, (void *)&cnts);
	while (1) {
		old_cnt = cnts.hwp_evctr[0];
		old_miss =  cnts.hwp_evctr[HWPERF_CNT1BASE];
		do_accesses(size);
		gen1 = ioctl(fd, PIOCGETEVCTRS, (void *)&cnts);
		if (gen1 != gen) {
			printf("Generation number mismatch. %d %d\n", 
			       gen, gen1);	
			ioctl(fd, PIOCRELEVCTRS);
			exit(-1);
		}

		if (cnts.hwp_evctr[0] - old_cnt) {
			char msg[256];

			sprintf(msg, "%d SCACHE SBE's during scache test",
				cnts.hwp_evctr[0] - old_cnt);
			openlog("cache_err", LOG_PID, LOG_DAEMON);	    
			syslog(LOG_ALERT|LOG_CONS, msg);
			syslog(LOG_EMERG, msg);
			syslog(LOG_INFO, msg);
			printf(msg);
			printf("%d SCACHE SBE during one pass through scache %d %d\n ",
			       cnts.hwp_evctr[0] - old_cnt,
			       cnts.hwp_evctr[0], old_cnt);

			printf("%d SCACHE misses\n",
			       cnts.hwp_evctr[HWPERF_CNT1BASE] - old_miss);
			old_cnt = cnts.hwp_evctr[0];
		}
		else {
			
#if 1
			char msg[256];
			sprintf(msg, "%d SCACHE SBE's during scache test",
				cnts.hwp_evctr[0] - old_cnt);
			openlog("cache_err", LOG_PID|LOG_CONS|LOG_NDELAY, LOG_USER);	    
			syslog(LOG_ALERT, msg);
			syslog(LOG_EMERG, msg);
			closelog();
			printf("Message %s \n", msg);

			printf(" %d SCACHE SBE during one pass through scache %d %d\n ",
			       cnts.hwp_evctr[0] - old_cnt,
			       cnts.hwp_evctr[0], old_cnt);

			printf(" %d SCACHE misses\n",
			       cnts.hwp_evctr[HWPERF_CNT1BASE] - old_miss);
		}
		
#endif
	}
}
