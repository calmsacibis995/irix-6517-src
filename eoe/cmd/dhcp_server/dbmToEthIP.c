#include <stdio.h>
#include <ndbm.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <syslog.h>
#include "dhcp_common.h"
#include "strings.h"

void
main(){
  char buf[256];
  DBM *db;
  datum key_d;
  datum data_d;
  int i;

  db = dbm_open("/var/dhcp/etherToIP",O_RDONLY, S_IROTH);
  if (db == 0) {
    syslog(LOG_ERR, "Cannot open etherToIP.dir / etherToIP.pag file");
    exit(1);
  }
  
  for (key_d = dbm_firstkey(db), i = 1; key_d.dptr != NULL; key_d = _dbm_nextkey(db), i++){

      bzero(buf, 256);
      data_d = dbm_fetch(db, key_d);
      switch (*(char*)key_d.dptr) {

      case '0':
      case '1':
      case '2':
	  continue;
	  
      case '3': 
	  strncpy(buf, data_d.dptr, data_d.dsize);
	  break;
	  
      default:
	  syslog(LOG_ERR, "Error in Rec # %d\n", i);
	  printf("Error in Rec # %d\n", i);
	  continue;
      }
      printf("%s\n", buf);
  }
  dbm_close(db);
}

