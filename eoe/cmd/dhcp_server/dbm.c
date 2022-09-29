#include <stdio.h>
#include <ndbm.h>
#include <sys/param.h>
#include <syslog.h>
#include <netinet/in.h>
#include <stdlib.h>
#include "dhcp_common.h"
#include <strings.h>


/* func:getRecByEtherAddr
 * return value: data.dptr
 */

char *
getRecByCid(char *sbuf, char *cid_ptr, int cid_length, DBM *db){
  datum key;
  datum data;  
  char  *coded_key;

  coded_key = malloc(1 + cid_length);

#ifdef DBM
  syslog(LOG_DEBUG,"Inside getRecByCid();cid_ptr = %s",cid_ptr);
#endif
  
  memccpy(coded_key, "3", NULL, 1);
  memcpy (&coded_key[1], cid_ptr, cid_length);

#ifdef DBM
  syslog(LOG_DEBUG,"Inside getRecByCid();coded_key = %s",coded_key);
#endif  

  key.dptr = coded_key;
  key.dsize = 1+ cid_length;
  data = dbm_fetch(db,key);

#ifdef DBM  
  syslog(LOG_DEBUG,"Inside getRecByCid();Fetched rec. is %s: %s",key.dptr,
	 data.dptr);
#endif
  
  free(coded_key);
  if (sbuf && data.dptr) {
    strncpy(sbuf, data.dptr, data.dsize);
    sbuf[data.dsize] = '\0';
    return sbuf;
  }
  else {
      return data.dptr;		/* returning NULL here */
  }
}


/*

char *
getRecByEtherAddr(EtherAddr *eth, DBM *db)
{
    
    datum key;
    datum data;  
    char  coded_key[1 + sizeof(EtherAddr)];
    char *coded_key2;
    datum key2;        
    datum actual_data; 
    
    coded_key[0] = '0';
    memcpy (&coded_key[1], eth, sizeof(EtherAddr));
    
    key.dptr = coded_key;
    key.dsize = sizeof(coded_key);
    data = dbm_fetch(db,key);
    
    coded_key2 = malloc(1+data.dsize);
    
    coded_key2[0] = '3';
    memcpy(&coded_key2[1], data.dptr, data.dsize);
    key2.dptr = coded_key2;
    key2.dsize = 1 + data.dsize;
    actual_data = dbm_fetch(db, key2);
    
    free(coded_key2);
    return actual_data.dptr;
}

*/

char *
getRecByHostName(char *sbuf, char *hname, DBM *db){

  char coded_key1[1 + MAXHOSTNAMELEN];   /*1+hname*/
  char *coded_key2;

  datum int_data;    /*ethernet address*/  
  datum key1;        /*coded key*/ 
  datum key2;        /*ethernet address*/
  datum actual_data; /*entire EtherIpFile rec*/  

  sprintf(coded_key1, "1%s", hname);
  key1.dptr = coded_key1;
  key1.dsize = strlen(coded_key1);
  int_data = dbm_fetch(db,key1);
  if (!int_data.dptr) {
      return int_data.dptr;	/*  returning NULL here  */
  }

  coded_key2 = malloc(1+int_data.dsize);

  coded_key2[0] = '3';
  memcpy(&coded_key2[1], int_data.dptr, int_data.dsize);
  key2.dptr = coded_key2;
  key2.dsize = 1 + int_data.dsize;
  actual_data = dbm_fetch(db, key2);

  free(coded_key2);
  if (sbuf && actual_data.dptr) {
    strncpy(sbuf, actual_data.dptr, actual_data.dsize);
    sbuf[actual_data.dsize] = '\0';
    return sbuf;
  }
  else
    return actual_data.dptr;
}


char *
getRecByIpAddr(char *sbuf, u_long *ipa, DBM *db){
  datum key1, key2;
  datum int_data, actual_data;  
  char coded_key1[1 + sizeof(u_long)];
  char *coded_key2;
 
  memccpy(coded_key1, "2", NULL, 1);
  memcpy (&coded_key1[1], ipa, sizeof(u_long));
  
  key1.dptr = coded_key1;
  key1.dsize = sizeof (coded_key1);
  int_data = dbm_fetch(db,key1);
  if (!int_data.dptr) {
      return int_data.dptr;	/* returning NULL here */
  }
  
  coded_key2 = (char *)malloc(1 + int_data.dsize);  
  memccpy(coded_key2, "3", NULL, 1);
  memcpy (coded_key2 + 1, int_data.dptr, int_data.dsize);

  key2.dptr = coded_key2;
  key2.dsize = 1 + int_data.dsize;
  actual_data = dbm_fetch(db, key2);

  free (coded_key2);
  if (sbuf && actual_data.dptr) {
    strncpy(sbuf, actual_data.dptr, actual_data.dsize);
    sbuf[actual_data.dsize] = '\0';
    return sbuf;
  }
  else
    return actual_data.dptr;
}

char *
getCidByIpAddr(char *cid_ptr, int *cid_len, u_long *ipa, DBM *db)
{
    datum key1;
    datum int_data;  
    char coded_key1[1 + sizeof(u_long)];
    
    memccpy(coded_key1, "2", NULL, 1);
    memcpy (&coded_key1[1], ipa, sizeof(u_long));
  
    key1.dptr = coded_key1;
    key1.dsize = sizeof (coded_key1);

    int_data = dbm_fetch(db,key1);
    if (!int_data.dptr) {
	return int_data.dptr;	/* returning NULL here */
    }
    memcpy(cid_ptr, int_data.dptr, int_data.dsize);
    *cid_len = int_data.dsize;
    return cid_ptr;
}

void putRecByCid(char *cid_ptr, int cid_length, DBM *db, char *frmted_data){
  datum key;
  datum data;
  char  *coded_key;
  int ret;

  coded_key = malloc(1 + cid_length);
#ifdef DBM
  syslog(LOG_DEBUG,"Inside putRecByCid(); cid_ptr = %s",cid_ptr);
#endif  

  memccpy(coded_key, "3", NULL, 1);
  memcpy (&coded_key[1], cid_ptr, cid_length);

#ifdef DBM
  syslog(LOG_DEBUG,"Inside putRecByCid(); coded_key = %s",coded_key);
#endif
  
  key.dptr = coded_key;
  key.dsize = 1 + cid_length;
  
  data.dptr = frmted_data;
  data.dsize = strlen(frmted_data) + 1 ;

#ifdef DBM
  syslog(LOG_DEBUG,"Inside putRecByCid(); stored rec. is %s: %s", key.dptr,
	 data.dptr);
#endif  

  ret = dbm_store(db, key, data, DBM_REPLACE);
  if (ret < 0)
      syslog(LOG_DEBUG, "Error while storing rec. by cid");
  free(coded_key);
}


void putRecByEtherAddr(EtherAddr *eth, DBM *db, char *cid_ptr, int cid_length){
  datum key;
  datum data;
  char  coded_key[7];
  int ret;

  memccpy(coded_key, "0", NULL, 1);
  memcpy (&coded_key[1], eth, sizeof(EtherAddr));

  key.dptr = coded_key;
  key.dsize = sizeof(coded_key);

  data.dptr = cid_ptr;
  data.dsize = cid_length; /*CHECK*/
  /* data.dsize = strlen(frmted_data) + 1 ;*/

  ret = dbm_store(db, key, data, DBM_REPLACE);
  if (ret < 0)
      syslog(LOG_DEBUG, "Error while storing rec. by EtherAddr");
}

void putRecByHostName(char *hname, DBM *db, char *cid_ptr, int cid_length){
  datum key;
  datum data;
  char coded_key[MAXHOSTNAMELEN];
  int ret;
  
  sprintf(coded_key, "1%s", hname);
  key.dptr = coded_key;
  key.dsize = strlen(coded_key);

  data.dptr = cid_ptr;
  data.dsize = cid_length; /*CHECK*/
  /*  data.dsize = sizeof(EtherAddr) + 1 ;*/

  ret = dbm_store(db, key, data, DBM_REPLACE);
  if (ret < 0)
      syslog(LOG_DEBUG, "Error while storing rec. by HostName");
}

void putRecByIpAddr(u_long ipaddr, DBM *db, char *cid_ptr, int cid_length){
  datum key;
  datum data;
  char coded_key[5];
  int ret;
  
  memccpy(coded_key, "2", NULL, 1);
  memcpy (&coded_key[1], &ipaddr, sizeof(u_long));

  key.dptr = coded_key;
  key.dsize = sizeof(coded_key);

  data.dptr = cid_ptr;
  data.dsize = cid_length; /*CHECK*/
  /*  data.dsize = sizeof(EtherAddr) + 1;*/

  ret = dbm_store(db, key, data, DBM_REPLACE);
  if (ret < 0)
      syslog(LOG_DEBUG, "Error while storing rec. by IpAddr");
}


void deleteRec(char *cid_ptr, int cid_length, EtherAddr *eth, u_long ipcl,
	       char *hname, DBM *db){

 datum key1;
 datum key2;
 datum key3;
 datum key4;
 char coded_key1[7];
 char coded_key2[MAXHOSTNAMELEN];
 char coded_key3[5];
 char *coded_key4;
 int ret;
 
 coded_key4 = malloc(1 + cid_length);

#ifdef DBM
 syslog(LOG_DEBUG,"Inside deleteRec; cid_ptr = %s, cid_length = %d",
	cid_ptr,cid_length);
#endif

 memccpy(coded_key1, "0", NULL, 1);
 memcpy (&coded_key1[1], eth, sizeof(EtherAddr)); 
 key1.dptr = coded_key1;
 key1.dsize = sizeof(coded_key1);
 dbm_delete(db, key1);
 
 sprintf(coded_key2, "1%s", hname);
 key2.dptr = coded_key2;
 key2.dsize = strlen(coded_key2);
 dbm_delete(db, key2);

 memccpy(coded_key3, "2", NULL, 1);
 memcpy (&coded_key3[1], &ipcl, sizeof(u_long));
 key3.dptr = coded_key3;
 key3.dsize = sizeof(coded_key3);
 dbm_delete(db, key3);

 memccpy(coded_key4, "3", NULL, 1);
 memcpy((coded_key4 + 1), cid_ptr, cid_length);
 key4.dptr = coded_key4;
 key4.dsize = 1 + cid_length;
 ret = dbm_delete(db, key4);

 if (ret < 0)
     syslog(LOG_DEBUG,"Error in dbm_delete()");

 free(coded_key4);
 
}
