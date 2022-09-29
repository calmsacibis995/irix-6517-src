#include "stest.H"
void getConfig() {
    FILE		*fd;
   char		*s;
   char		buf[BUFSIZ];
   char		*indir;
   int		len;
   int		line = 0;
   int		msec;

	if (strcmp(config, "-") == 0)
		fd = stdin;
	else if ((fd = fopen(config, "r")) == NULL) {
		vperror("fopen(%s,\"r\"): ", config);
		exit(1);
	}
	while (fgets(buf, BUFSIZ, fd) != NULL) {
		line++;
		indir = buf;
		while (*indir == ' ' || *indir == '\t') indir++;
		len = strlen(indir);
		if (*indir == 's')
			usleep = strtol(&indir[1], 0, 0);
		else if (*indir == 'p') {
			if (nseen >= NCLD) {
				fprintf(stderr,
					"Too many processes specified\n");
				exit(1);
			}
			s = strtok(&indir[1], " \t\n");
			while (s != NULL) {
				if (strcmp(s, "ndpri") == 0) {
					if ((s = strtok(0, " \t\n")) == NULL) {
						fprintf(stderr,
						     "Parse error on line %d\n",							line);
						exit(1);
					}
					undpri[nseen] = strtol(s, 0, 0);
				}
				else if (strcmp(s, "nice") == 0) {
					if ((s = strtok(0, " \t\n")) == NULL) {
						fprintf(stderr,
						     "Parse error on line %d\n",							line);
						exit(1);
					}
					unice[nseen] = strtol(s, 0, 0);
				}
				else if (strcmp(s, "thrash") == 0)
					thrash[nseen]++;
				else if (strcmp(s, "dlint") == 0) {
					if ((s = strtok(0, " \t\n")) == NULL) {
						fprintf(stderr,
						     "Parse error on line %d\n",							line);
						exit(1);
					}
					msec = strtol(s, 0, 0);
					udlinfo[nseen].dl_valid = 1;
					udlinfo[nseen].dl_interval.tv_sec =
						msec / 1000;
					udlinfo[nseen].dl_interval.tv_nsec =
						(msec % 1000) * (NSEC_PER_SEC /
						1000);

				}
				else if (strcmp(s, "dlalloc") == 0) {
					if ((s = strtok(0, " \t\n")) == NULL) {
						fprintf(stderr,
						     "Parse error on line %d\n",							line);
						exit(1);
					}
					msec = strtol(s, 0, 0);
					udlinfo[nseen].dl_valid = 1;
					udlinfo[nseen].dl_alloc.tv_sec =
						msec / 1000;
					udlinfo[nseen].dl_alloc.tv_nsec =
						(msec % 1000) * (NSEC_PER_SEC /
						1000);
				}
				else if (strcmp(s, "prog") == 0) {
				   char	*t;

					s += strlen(s) + 1;
					if (s - indir < len) {
						for (t = s; *t != '\0'; t++)
							if (*t == '\n') {
								*t = '\0';
								break;
							}
						prog[nseen] = strdup(s);
					}
					s = NULL;
				}
				else if (strcmp(s, "block") == 0) {
					block[nseen] = 1;
				}
				else if (strcmp(s, "release") == 0) {
					release[nseen] = 1;
				}
				else if (strcmp(s, "only") == 0) {
					only[nseen] = 1;
				}
				else {
					fprintf(stderr,
					     "Parse error on line %d\n",							line);
					exit(1);
				}
				if (s != NULL)
					s = strtok(0, " \t\n");
			}
			nseen++;
		}
	}
}



void makeCommArea() {
   int		msize;
   int		fd,poffmask;
   __psunsigned_t raddr,phys_addr;
   msize = (sizeof(struct shinfo) < SHMMIN ? SHMMIN :
		 sizeof(struct shinfo));
   if ((shmid = shmget(IPC_PRIVATE, msize, 0777)) == -1) {
     vperror("shmget(IPC_PRIVATE, %d, 0777): ", msize);
     exit(1);
   }
   if ((sharea = (struct shinfo *) shmat(shmid, 0, 0)) == (void *) -1) {
	  vperror("shmat(%d,0,0): ", shmid);
	  exit(1);
	}
   shmctl(shmid, IPC_RMID, 0);
   
   iotimer_addr = (iotimer_t *) -1;
   poffmask = getpagesize() - 1;
   phys_addr = syssgi(SGI_QUERY_CYCLECNTR,&cycleval);
   raddr = phys_addr& ~poffmask;
   fd = open("/dev/mmem",O_RDONLY);
   iotimer_addr = (volatile iotimer_t *) mmap(0,poffmask,PROT_READ,
					      MAP_PRIVATE,fd,(__psint_t)raddr);
   iotimer_addr = (iotimer_t *)((__psunsigned_t)iotimer_addr + (phys_addr & poffmask));
   
}


double
getabstime() {
   static unsigned long		lbolt = 0;
   static iotimer_t		curval = 0;
   static unsigned long	        twrap = 0;
   struct tms	tb;
   unsigned long nbolt;
   iotimer_t	nval;
   double	res;
   
   if (iotimer_addr == (iotimer_t *) -1)
      return -1;
   if (lbolt == 0) {
      lbolt = times(&tb);
      curval = *iotimer_addr;
      res = 0;
      return(res);
   }
   nbolt = times(&tb);
   nval = *iotimer_addr;
   if (nbolt - lbolt > twrap)
      res = (double)(nbolt - lbolt) / HZ;
   else {
      res = ( (nval - curval) * (double) cycleval);
      res /= 10000000000000.0;
   }
   lbolt = nbolt;
   curval = nval;
   return(res);
}

void sumabstime() { localt += getabstime(); }

void
catcher(int x) {
	sharea->cease = 1;
	exit(1);
}

void trapSignals() {
	signal(SIGHUP, (void (*) (...))catcher);
	signal(SIGQUIT, (void (*) (...))catcher);
	signal(SIGTERM, (void (*) (...))catcher);
}

char		*pname;
extern int	errno;

int main(int argc, char** argv)
{
   extern int	optind;
   extern char	*optarg;
   int		c;
   int		err = 0;
   int		i;

	pname = argv[0];
	while ((c = getopt(argc, argv, "gtf:")) != EOF) {
		switch (c) {
		case 'g':
			chkgtime = 1;
			break;
		case 't':
			chktime = 1;
			break;
		case 'f':
			config = optarg;
			break;
		case '?':
			err++;
			break;
		}
	}
	if (err) {
		fprintf(stderr, "usage: %s\n", pname);
		exit(1);
	}
	for (i = optind; i < argc; i++) {
		fprintf(stderr, "%s: extra argument %s ignored\n", pname,
			argv[i]);
	}
	getConfig();
	if (nseen <= 0)
		exit(1);
	makeCommArea();
	setbuf(stdout, NULL);
	trapSignals();
	basicTest1(nseen, unice, undpri, udlinfo, usleep);
	exit(0);
}

void chkblock(int action, timespec_t alloc, volatile int *stop, int *acount,
	int *lcount) {
   struct timeval	convert;
   struct timeval	then;
   struct timeval	now;
   int			snap;
   int			nsnap;

	if (alloc.tv_sec)
		alloc.tv_sec /= 2;
	if (alloc.tv_nsec)
		alloc.tv_nsec /= 2;
	timespec_to_timeval(&alloc, &convert);

	if (prda)
		snap = prda->t_dlactseq;

	while (!*stop) {
		sumabstime();
		gettimeofday(&then, 0);
		then.tv_sec += convert.tv_sec;
		then.tv_usec += convert.tv_usec;
		if (then.tv_usec >= 1000000) {
			then.tv_sec++;
			then.tv_usec = then.tv_usec % 1000000;
		}
		while (!*stop) {
			gettimeofday(&now, 0);
			if (now.tv_sec > then.tv_sec ||
			    (now.tv_sec == then.tv_sec &&
			     now.tv_usec >= then.tv_usec))
				break;
		}

		if (action) {
			if (schedctl(DEADLINE,0,DL_BLOCK) == -1)
				vperror("schedctl(DEADLINE,0,DL_BLOCK): ");
			(*acount)++;
		}
		else {
			if (schedctl(DEADLINE, 0,DL_RELEASE) == -1)
				vperror("schedctl(DEADLINE,0,DL_RELEASE): ");
			(*acount)++;
		}

	}



	if (prda) {
	  

		nsnap = prda->t_dlactseq;
		*lcount += nsnap - snap;
		snap = nsnap;
	}

}

int	kiddied;
void
deadkid()
{
	kiddied = 1;
}

void spin(volatile int *start, volatile int *stop, volatile int *done,
	volatile clock_t *elapsed, volatile struct tms *tmp, char *pr,
	int place) {
   clock_t	selapse;
   clock_t	eelapse;
   struct timeval	st;
   struct timeval	et;
   struct tms	stm;
   struct tms	etm;
   while (!*start){
      sginap(1);
   }
   gettimeofday(&st, 0);
   sumabstime();

   
     selapse = times(&stm);
   sharea->lcount[place] = 0;
   if (!pr) {
      unsigned	snap;
      unsigned	nsnap;
      unsigned	act = 0;
      
      
      if (prda)
         snap = prda->t_dlactseq;

      while (!*stop) {
         if (prda) {
            nsnap = prda->t_dlactseq;
            if (nsnap != snap) {
               if (nsnap > snap)
                  sharea->lcount[place] +=
                     (nsnap - snap);
               else
                  sharea->lcount[place] +=
                     (0xffffffff - snap) + nsnap;
               snap = nsnap;
            }
         }
         
         if (thrash[place]) {
            sginap(0);
            sharea->spthrash[place]++;
         }
      }
   }
   else if (pr == (char *) 1)
      chkblock(0, udlinfo[place].dl_alloc, stop,
               &sharea->acount[place], &sharea->lcount[place]);
   else if (pr == (char *) 2)
      chkblock(1, udlinfo[place].dl_alloc, stop,
                    &sharea->acount[place], &sharea->lcount[place]);
   else {
      char		*args[20];
      int		i;
      int		pid;
      char		*s;
      int		stat;
      
      s = strtok(pr, " \t\n");
      i = 0;
      while (s != NULL && i < 19) {
         args[i++] = s;
         s = strtok(0, " \t\n");
      }
      args[i] = 0;
      kiddied = 0;
      signal(SIGCLD, (void (*) (...))deadkid);
      if ((pid = fork()) == 0) {
         close(1);
         open("/dev/null", O_WRONLY);
         execvp(args[0], args);
         vperror("execv(%s): ", args[0]);
         exit(1);
      } else if (pid == -1) {
         vperror("fork: ");
         while (!*stop) { }
		} else {
                   if (schedctl(DEADLINE, 0, 0) == -1)
                      vperror("schedctl(DEADLINE,0,0): ");
                   while (!*stop && !kiddied)
                      sleep(1);
                   if (!kiddied) {
                      printf("spin[%d]: kiling %d\n", getpid(), pid);
                      if (kill(pid, SIGKILL) == -1)
                         vperror("kill(%d,KILL): ", pid);
                   }
                   wait(&stat);
		}
   }
   
   gettimeofday(&et, 0);
   sumabstime();
   sharea->realtime[place] = localt;
   eelapse = times(&etm);
   sharea->grealtime[place] = (et.tv_sec - st.tv_sec);
   if (et.tv_usec > st.tv_usec)
      sharea->grealtime[place] +=
         (et.tv_usec - st.tv_usec) / 1000000.0;
   else
      sharea->grealtime[place] -=
         (st.tv_usec - et.tv_usec) / 1000000.0;
   if ( pr > (char*) 10) {
      tmp->tms_utime = etm.tms_cutime - stm.tms_cutime;
      tmp->tms_stime = etm.tms_cstime - stm.tms_cstime;
   }
   else {
      tmp->tms_utime = etm.tms_utime - stm.tms_utime;
      tmp->tms_stime = etm.tms_stime - stm.tms_stime;
   }
   *elapsed = eelapse - selapse;
   *done = 1;
   exit(0);
}

int preemies = 0;

void crisis(int sig) {
   int pid;
   int status;
   int i;

	pid = wait(&status);
	printf("Child %d died of unatural causes, status = 0x%x\n", pid,
		status);
	for (i = 0; i < nseen; i++) {
		if (cpid[i] == pid)
			sharea->done[i] = 1;
	}
	preemies++;
	signal(SIGCLD, (void (*) (...))crisis);
}

void basicTest1(int nprocs,
	   int *nice,
	   int *ndpri,
	   dlinfo_t *deadline,
	   int slp) {
   float	tticks;
   int		i;
   int		norm;
   int		nl = 1;
   int		stat;
   int		dcnt;

	if (schedctl(NDPRI, 0, NDPHIMAX) == -1)
		vperror("schedctl(NDPRI,0,NDPHIMAX): ");
	signal(SIGCLD, (void (*) (...))crisis);
	for (i = 0; i < nprocs; i++) {
	  sharea->begin =0;

		if ((cpid[i] = fork()) == -1) {
			perror("fork: ");
			sharea->cease = 1;
			sharea->begin = 1;
			exit(1);
		}
		if (cpid[i] == 0) {
			printf("Proc %d (%d): \n", i, getpid());
			norm = 0;

			if (deadline) {
			   struct sched_deadline	dlinfo;
			   pid_t p_id = getpid();
				if (deadline[i].dl_valid) {
					dlinfo.dl_period =
						deadline[i].dl_interval;
					dlinfo.dl_alloc =
						deadline[i].dl_alloc;
					if (schedctl(DEADLINE,0,&dlinfo)==-1) {
						vperror(
				"schedctl(DEADLINE,0,{[%d,%d],[%d,%d]}): ",
						deadline[i].dl_interval.tv_sec,
						deadline[i].dl_interval.tv_nsec,
						deadline[i].dl_alloc.tv_sec,
						deadline[i].dl_alloc.tv_nsec);
						nl = 0;
					}
					else
						printf(
						"deadline([%d,%d],[%d,%d]) ",
						deadline[i].dl_interval.tv_sec,
						deadline[i].dl_interval.tv_nsec,
						deadline[i].dl_alloc.tv_sec,
						deadline[i].dl_alloc.tv_nsec);
					norm++;
					if ((prda = (struct prda_sys *)
				schedctl(SETHINTS)) == (struct prda_sys *) -1)
						vperror("schedctl(SETHINTS): ");
				}
			}
			if (only[i]) {
				if (schedctl(DEADLINE,0,DL_ONLY) == -1)
					vperror(
					"schedctl(DEADLINE,0,DL_ONLY): ");
				else
					printf("only ");
			}

			if (nice) {
				if (nice[i]) {
					if (schedctl(RENICE, 0, nice[i]) == -1){
						vperror(
						      "schedctl(RENICE,0,%d): ",
						      nice[i]);
						nl = 0;
  					}
					else
						printf("nice(%d) ", nice[i]);
					norm++;
				}
			}
			if (block[i])
				printf("block ");
			if (prog[i])
				printf("x(%s) ", prog[i]);
			if (release[i])
				printf("release ");
			if (ndpri) {
				if (ndpri[i]) {
					if (schedctl(NDPRI, 0, ndpri[i])==-1) {
						vperror(
						       "schedctl(NDPRI,0,%d): ",
							ndpri[i]);
						nl = 0;
					}
					else
						printf("ndpri(%d) ", ndpri[i]);
					norm++;
				}
				else if (schedctl(NDPRI, 0, 0) == -1)
					vperror("schedctl(NDPRI,0,0): ");
			}
			else if (schedctl(NDPRI, 0, 0) == -1)
				vperror("schedctl(NDPRI,0,0): ");
			if (nl)
				printf("\n");
			/*
			if (plock(PROCLOCK) == -1)
				vperror("plock(PROCLOCK): ");
			*/
			if (release[i])
				spin(&sharea->begin,
				     &sharea->cease,
				     &sharea->done[i],
				     &sharea->elapsed[i],
				     &sharea->tms[i],
				     (char *) 1,
				     i);
			else if (block[i])
				spin(&sharea->begin,
				     &sharea->cease,
				     &sharea->done[i],
				     &sharea->elapsed[i],
				     &sharea->tms[i],
				     (char *) 2,
				     i);
			else {
				spin(&sharea->begin,
				     &sharea->cease,
				     &sharea->done[i],
				     &sharea->elapsed[i],
				     &sharea->tms[i],
				     prog[i],
				     i);
			}
		}
	}

	sharea->begin = 1;

	sleep(slp);
	signal(SIGCLD, SIG_DFL);
	sharea->cease = 1;
	if (schedctl(NDPRI, 0, 0) == -1)
		vperror("schedctl(NDPRI,0,0): ");

	dcnt = 0;
	while (dcnt < nprocs) {
		sleep(1);
		for (dcnt = 0, i = 0; i < nprocs; i++)
			if (sharea->done[i])
				dcnt++;
	}
	for (i = preemies; i < nprocs; i++)
		wait(&stat);

	for (i = 0; i < nprocs; i++) {
		tticks = sharea->tms[i].tms_utime + sharea->tms[i].tms_stime;
		printf(
	    "%d (mS): %9.2f elapsed, %9.2f user, %9.2f sys, ratio %.2f%%\n",
			i,
			mS(sharea->elapsed[i]),
			mS(sharea->tms[i].tms_utime),
			mS(sharea->tms[i].tms_stime),
			(sharea->elapsed[i] ? (tticks / sharea->elapsed[i])
				: 0) * 100);
		if (chkgtime)
			printf(
    "       :%9.2f elapsed gettimeofday (%6.2f secs) [error = %6.2f%%]\n",
			    sharea->grealtime[i] * 1000,
			    sharea->grealtime[i],
			    (1.0 - (mS(sharea->elapsed[i])/
				(sharea->grealtime[i]*1000)))*100);
		if (chktime)
			printf(
    "       :%9.2f elapsed counter      (%6.2f secs) [error = %6.2f%%]\n",
			    sharea->realtime[i] * 1000,
			    sharea->realtime[i],
			    (1.0 - (mS(sharea->elapsed[i])/
				(sharea->realtime[i]*1000)))*100);
 		if (deadline[i].dl_valid) {
			printf(
			    "       : %d blk/rls, %d events (%d expected)\n",
			    sharea->acount[i],
			    sharea->lcount[i],
			    (int) (mS(sharea->elapsed[i]) /
			    tmS(&deadline[i].dl_interval)));
		}
		if (thrash[i]) {
			printf(
		    "       : total thrashes %6d index %6d scheds/sec\n",
			    sharea->spthrash[i],
			    (sharea->elapsed[i] ?
			        (sharea->spthrash[i]/sharea->elapsed[i])*HZ
				: 0));
		}
		if (chkgtime || chktime)
			printf("\n");
	}
}

void
vperror(char* fmt,...)

{
   va_list	args;
   char		vpbuf[BUFSIZ];

	va_start(args,fmt);

	vsprintf(vpbuf, fmt, args);
	perror(vpbuf);

	va_end(args);
}




