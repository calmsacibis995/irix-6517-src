#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/conf.h>
#include <sys/hwgraph.h>
#include <sys/failover.h>

#include <sys/iograph.h>
#include <dirent.h>
#include <malloc.h>
#include <bstring.h>
#include <ftw.h>

char *dev_to_devname(dev_t, char *, int *);
char *scsi_equivalent ( char *path );
int  already_has_scsi (char *path);

void
usage(char *prog)
{
	fprintf(stderr,
	        "usage: %s [-d (dump failover structures)]\n"
		"[-s <device path name> (perform failover switch)]\n"
		"[-t <device path name> (perform failover switch/tresspass)]\n", prog);
	exit(1);
}

void
main(int argc, char **argv)
{
  int	 c;
  int    dodump=0, doswitch=0, dotresspass=0;
  char   *pathname;
  char	 *scsi_name;

  while ((c = getopt(argc, argv, "s:t:d")) != -1) {
    switch (c) {
    case 'd':			/* Dump */
      dodump = 1;
      break;

    case 't':
      dotresspass = 1;
      /* fall through */
    case 's':
      doswitch = 1;
      pathname = optarg;
      break;

    default:
      usage(argv[0]);
    }
  }

  if (optind == 1)
    usage(argv[0]);

  if (dodump) {
    int                      alloc_len, len;
    struct user_fo_instance *buf, *u_foi;
    int                      i, j;
    char                     dev_name[MAXDEVNAME];
    int                      dev_len;

    len = alloc_len = 1024 * sizeof(struct user_fo_instance);
    buf = calloc(1, alloc_len);
    for (; ;) {
      if (syssgi(SGI_FO_DUMP, &len, buf) == -1) {
	printf("syssgi(SGI_FO_DUMP) failed: %s\n", strerror(errno));
	exit(1);
      }
      if ((alloc_len - len) > sizeof(struct user_fo_instance))
	break;
      free(buf);
      len = alloc_len = alloc_len*2;
      buf = calloc(1, alloc_len);
    }

    if (len == 0)
	    exit(0);

    u_foi = buf;
    for (i = 0; ; ++i) {
      printf("Group %d:\n", i);
      for (j = 0; j < MAX_FO_PATHS; ++j) 
	if (u_foi->foi_path_vhdl[j]) {
	  dev_name[0] = '\0';
	  dev_len = MAXDEVNAME;
	  dev_to_devname(u_foi->foi_path_vhdl[j], dev_name, &dev_len);
	  if (*dev_name)
		scsi_name = scsi_equivalent (dev_name);
		if (!scsi_name)
			printf("  [%c] %s (%d)\n", 
				((u_foi->foi_primary == j) ? 'P' : ' '),
				dev_name,
				u_foi->foi_path_vhdl[j]);
		else
			printf("  [%c] %s (%d)\n", 
				((u_foi->foi_primary == j) ? 'P' : ' '),
				scsi_name,
				u_foi->foi_path_vhdl[j]);
	}
      if (((i+1)*sizeof(struct user_fo_instance)) >= len)
	break;
      ++u_foi;
    }
    exit(0);
  }
  
  if (doswitch) {
    vertex_hdl_t p_vhdl, s_vhdl;
    struct stat  st;
    char         dev_name[MAXDEVNAME];
    int          dev_len;
    int		 errno_save;

    if (stat(pathname, &st)) {
      errno_save = errno;
      /* presume it was a scsi basename e.g., sc9d4l2 */
      strcpy (dev_name,"/hw/scsi/");
      strcat (dev_name,pathname);
      if (stat(dev_name,&st)) {
        printf("stat(%s) failed: %s\n", pathname, strerror(errno));
        printf("stat(%s) failed: %s\n", dev_name, strerror(errno_save));
        exit(1);
      }
    }
    p_vhdl = st.st_rdev;
    
    if (dotresspass)
      c = syssgi(SGI_FO_TRESSPASS, p_vhdl, &s_vhdl);
    else
      c = syssgi(SGI_FO_SWITCH, p_vhdl, &s_vhdl);
    if (c == -1) {
      printf("syssgi(SGI_FO_SWITCH) failed: %s\n", strerror(errno));
      exit(1);
    }
    dev_len = MAXDEVNAME;
    dev_to_devname(s_vhdl, dev_name, &dev_len);
    scsi_name = scsi_equivalent (dev_name);
    if (scsi_name) {
	printf("New primary path %s (%d) (%s)\n", dev_name, s_vhdl, scsi_name);
    }
    else {
	printf("New primary path %s (%d)\n", dev_name, s_vhdl);
    }
  }
}

#define SCSI_EDGE	("/" EDGE_LBL_SCSI)
struct stat64	sbuf;
static char	aliaspath[MAXDEVNAME];

/*ARGSUSED*/
int
scsi_scan(const char *alias, const struct stat64 *rsbuf, int status, struct FTW *lvl)
{
	if (((rsbuf->st_mode & S_IFMT) == S_IFCHR) &&
	    (rsbuf->st_rdev == sbuf.st_rdev)) 
	{
		strcpy(aliaspath, alias);
		return 1;
	}
	return 0;
}

char *
scsi_equivalent ( char *path )
{
	char scsipath[MAXDEVNAME];
	static char *scsidir = "/hw/scsi";

	strcpy (scsipath,path);

	if (!already_has_scsi(path))
		strncat(scsipath, SCSI_EDGE, sizeof(scsipath) - (strlen(scsipath) + 1));

	if (stat64(scsipath, &sbuf) < 0) {
		return NULL;
	}
	if ((sbuf.st_mode & S_IFMT) != S_IFCHR) {
		return NULL;
	}

	aliaspath[0] = 0;
	nftw64(scsidir, scsi_scan, 3, FTW_PHYS);

	if (strlen(aliaspath))
		return aliaspath + strlen(scsidir) + 1;
	return NULL;
}

int
already_has_scsi (char *path)
{
	char *edge;

	edge = strrchr(path, '/');
	if (edge == NULL)
		return NULL;
	return !strcmp(edge, SCSI_EDGE);
}
