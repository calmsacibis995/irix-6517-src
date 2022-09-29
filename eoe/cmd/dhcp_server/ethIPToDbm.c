#include <stdio.h>
#include <unistd.h>
#include <ndbm.h>
#include <strings.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>

#include "dhcp_common.h"
#include "dhcp.h"

void create_dbm_files(char *);
void create_ethers_file(char *);
void create_hosts_file(char *);
void usage(char *);

int create_new_entry(char *, int, EtherAddr *, u_long, char *, time_t, DBM *);
int parse_ether_entry(char *, char **, char **, char **, char **); 
FILE *log_fopen(char *, char *);
extern struct ether_addr *ether_aton(char *);

char *vartmpEtherIpFile = "/var/tmp/etherToIP";
char *vardhcpEtherIpFile = "/var/dhcp/etherToIP";

int dblock = 0;

/*
 * this is the same as loc0_create_new_entry_l from loc0.c
 * without the LDAP update portion
 */
int
create_new_entry(char *cid_ptr, int cid_length, EtherAddr *eth,
			u_long ipaddr, char *hname, time_t lease_end,
			DBM *db)
{
  char *ebuf;
  char *ipchar;
  char tmp1[256];
  char tmp2[256];
  struct in_addr ipn;
  
  ebuf = ether_ntoa(eth);
  ipn.s_addr = ipaddr;
  ipchar = inet_ntoa(ipn);

  if (sprintf(tmp1,"%s\t%s\t%s\t%d\0", ebuf, ipchar, hname, lease_end)<0){
    return 1;
  }
  strcpy(tmp2, tmp1);

  LOCKDHCPDB(dblock);
  putRecByCid(cid_ptr, cid_length, db, tmp2);
  putRecByEtherAddr(eth, db, cid_ptr, cid_length);
  putRecByHostName(hname, db, cid_ptr, cid_length);
  putRecByIpAddr(ipaddr, db, cid_ptr, cid_length);
  UNLOCKDHCPDB(dblock);
  return 0;
}

/*
 * from loc0.c - to avoid linking in that file
 */
int
parse_ether_entry(char *buf, char **beh, char **bipc, char **bhna,char **blease)
{
    char *p;

    p = buf;
    *beh = strtok(p, " \t");
    if (! *beh) {
	return 1;
    }
    p = (char *)0;
    *bipc = strtok(p, " \t");
    if (! *bipc) {
	return 1;
    }
    *bhna = strtok(p, " \t");
    if (! *bhna) {
	return 1;
    }
    *blease = strtok(p, " \t");
    if (! *blease) {
	return 1;
    }

    return 0;
}

FILE *
log_fopen(char *file, char *options)
{
	FILE *fp;

	fp = fopen(file, options);
	if (! fp) {
		syslog(LOG_ERR, "fopen failed for file %s: %m", file);
	}
	return fp;
}

/*===========================NOTE==================================
  The foll. variables have been defined as a workaound for a linking problem 
  ================================================================*/
/* int ProclaimServer = 0;
int ifcount = 0;            
struct netaddr* np_recv;
iaddr_t myhostaddr;
int always_return_netmask = 0;
int debug = 0;
int alt_naming_flag = 0;*/
/*===============================================================*/

void
main(int argc, char *argv[]){
  int ch=0, fflag=0, eflag=0, hflag=0;
  int st1,st2,st3;
  char input1[2],input2[2],input3[2];
  struct stat buf1, buf2, buf3;
  FILE *fp;
  DBM *db;

  while ((ch = getopt(argc, argv, "fhe")) != -1) {
    switch(ch) {
    case 'f':
      fflag = 1;
      break;
      
    case 'h':
      hflag = 1;
      break;
      
    case 'e':
      eflag = 1;
      break;
      
    default:
      usage(argv[0]);
      exit(1);
    }
  }
  
  openlog("ethIPToDbm", LOG_PID|LOG_CONS, LOG_USER);
  if (fflag){
    st1 = stat("/var/tmp/etherToIP.dir", &buf1);
    if (st1 == 0){
      printf("The files etherToIP.dir/etherToIP.pag exist in /var/tmp.Do you wish to overwrite them[y/n]:");
      scanf("%s", input1);
      if ((*input1 == 'y') ||(*input1 == 'Y')){
	create_dbm_files(vartmpEtherIpFile);	
      }
      else{
	printf("No changes being made\n");
      }
    }
    else{
      create_dbm_files(vartmpEtherIpFile);
    }
  }
  else if((!hflag) && (!eflag)){
    fp = fopen("/var/dhcp/etherToIP", "r");
    if (fp)
      fclose(fp);
    
    db = dbm_open("/var/dhcp/etherToIP", O_RDONLY, S_IRUSR);
    if (db)
      dbm_close(db);

    if ((fp) && (!db))
      create_dbm_files(vardhcpEtherIpFile);
  }
  
  if (eflag){
    st2 = stat("/var/tmp/ethers", &buf2);
    if (st2 == 0){  
      printf("The ethers file exists in /var/tmp.Do you wish to overwrite it[y/n]:");
      scanf("%s", input2);
      if ((*input2 == 'y') ||(*input2 == 'Y')){
	create_ethers_file(vartmpEtherIpFile);	
      }
      else{
	printf("No changes being made\n");
      }
    }
    else{
      create_ethers_file(vartmpEtherIpFile);	
    } 
  }
  
  
  
  if (hflag){
    st3 = stat("/var/tmp/hosts", &buf3);
    if (st3 == 0){  
      printf("The hosts file exists in /var/tmp.Do you wish to overwrite it[y/n]:");
      scanf("%s", input3);
      if ((*input3 == 'y') ||(*input3 == 'Y')){
	create_hosts_file(vartmpEtherIpFile);	
      }
      else{
	printf("No changes being made\n");
      }
    }
    else{
      create_hosts_file(vartmpEtherIpFile);	
    }
  }
  closelog();
  exit(0);  
}
  

void
usage(char *invocation) {
  printf("Usage: %s [-f] [-h] [-e]\n",invocation);
} 


void
create_dbm_files(char *etherIpFilePath){
  
  char buf[256];
  char *eh, *ipc, *hna, *lend;
  EtherAddr *ehs;
  u_long ipcl;
  time_t lease_end;
  char *cid_ptr;
  int cid_length;
  DBM *db;
  FILE *fp;

  fp = fopen(etherIpFilePath, "r");
  if (!fp)
    exit(1);

  db = dbm_open(etherIpFilePath, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR|S_IROTH);
  if (db==NULL){
    printf("ERROR: creating etherToIP.dir/etherToIP.pag files\n");
    fclose(fp);
    exit (1);
  }	
  sprintf(buf, "%s.lock", etherIpFilePath);
  dblock = open(buf, O_RDONLY|O_CREAT, 0604);
  if (dblock < 0) {
      syslog(LOG_ERR, "Could not open %s (%m)", buf);
      exit(1);
  }
  
  bzero(buf, 256);
  while(fgets(buf, sizeof(buf), fp)) {
    if( (buf[0] == '\n') || (buf[0] == '#') || (buf[0] == ' ') )
      continue;
    if(parse_ether_entry(buf, &eh, &ipc, &hna, &lend)) {
      fclose(fp);
      printf("ERROR: parsing etherToIP file");
      fclose(fp);
      dbm_close(db);
      exit (1);
    }
    
    ehs = ether_aton(eh);
    if(!ehs) {
      fclose(fp);
      printf("ERROR: ether_aton conversion.");
      fclose(fp);
      dbm_close(db);     
      exit(1);
    }
    ipcl = inet_addr(ipc);
    lease_end = atoi(lend);
    cid_ptr = (char*)ehs->ea_addr;
    cid_length = sizeof(EtherAddr);
    
    create_new_entry(cid_ptr, cid_length, ehs, ipcl, hna, lease_end, db);
  }

  printf("Created ndbm files %s (.dir/.pag). Correct path is /var/dhcp.\n",etherIpFilePath);

  close(dblock);
  fclose(fp);
  dbm_close(db);
}

void
create_ethers_file(char *etherIpFilePath){

  FILE *ethers_fp;
  FILE *etherIpFile_fp;
  char buf[256];
  char *eh, *ipc, *hna, *lend;
  char host[100];

  etherIpFile_fp = log_fopen(etherIpFilePath, "r");
  if(!etherIpFile_fp){
    printf("ERROR: reading/opening %s file\n",etherIpFilePath );
    exit(1);
  }
  
  ethers_fp = fopen("/var/tmp/ethers","w+");
  if(!ethers_fp){
    printf("ERROR: opening /var/tmp/ethers file");
    fclose(etherIpFile_fp);
    exit(1);
  }

  bzero(buf, 256);
  while(fgets(buf, sizeof(buf), etherIpFile_fp)) {
    if( (buf[0] == '\n') || (buf[0] == '#') || (buf[0] == ' ') )
      continue;
    if(parse_ether_entry(buf, &eh, &ipc, &hna, &lend)) {
      fclose(etherIpFile_fp);
      fclose(ethers_fp);
      printf("ERROR: parsing etherToIP file");
      exit (1);
    }

    strcpy(host, hna);
    strtok(host, ".");

    if (fprintf(ethers_fp, "%s\t%s\n", eh, host) < 0) {
        fclose(etherIpFile_fp);
	fclose(ethers_fp);
	printf("ERROR: writing to /var/tmp/ethers file");
	exit(1);
    }
  }
  printf("Created ethers file in /var/tmp. It needs to be copied over to /etc/\n ");
  fclose(etherIpFile_fp);
  fclose(ethers_fp);
}

void 
create_hosts_file(char *etherIpFilePath){

  FILE *hosts_fp;
  FILE *etherIpFile_fp;
  char buf[256];
  char *eh, *ipc, *hna, *lend;
  char host[100];

  etherIpFile_fp = log_fopen(etherIpFilePath, "r");
  if(!etherIpFile_fp){
    printf("ERROR: reading/opening %s file\n",etherIpFilePath );
    exit(1);
  }
  
  hosts_fp = fopen("/var/tmp/hosts","w+");
  if(!hosts_fp){
    fclose(etherIpFile_fp);
    printf("ERROR: opening /var/tmp/hosts file");
    exit(1);
  }

  bzero(buf, 256);
  while(fgets(buf, sizeof(buf),etherIpFile_fp)) {
    if( (buf[0] == '\n') || (buf[0] == '#') || (buf[0] == ' ') )
      continue;
    if(parse_ether_entry(buf, &eh, &ipc, &hna, &lend)) {
      fclose(etherIpFile_fp);
      fclose(hosts_fp);
      printf("ERROR: parsing etherToIP file");
      exit (1);
    }

    strcpy(host, hna);
    strtok(host, ".");
    
    if (fprintf(hosts_fp, "%s\t%s\t%s\n", ipc, hna, host) < 0) {
      fclose(etherIpFile_fp);
      fclose(hosts_fp);
      printf("ERROR: writing to /var/tmp/hosts file");
      exit(1);
    }
  }
  printf("Created hosts file in /var/tmp. It needs to be copied over to /etc/\n ");
  fclose(etherIpFile_fp);
  fclose(hosts_fp);
}


