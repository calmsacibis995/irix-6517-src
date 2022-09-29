#ifndef _INBUILT_FAKE_SSDB_H_
#define _INBUILT_FAKE_SSDB_H_
struct ssdb_event {
  int ev_class;
  int ev_type;
  int throttle_count;
  int throttle_time;
  char *Pdata;
};
  
static struct ssdb_event Assdb_event[] = 
{
  { 1, 1, 1, 10, "11", },
  { 1, 2, 1, 5,  "12", },
  { 1, 3, 4, 0,  "13", },
  { 1, 4, 1, 1,  "14", },
  { 2, 1, 0, 0,  "21", },
  { 2, 2, 0, 10, "22", },
};

const char * Assdb_rules[] =
{
  "\"localhost\", 10, 0 : UID(guest) ENV(DISPLAY=XXX.csd:0.0) ENV(SHELL=/bin/ksh) TIME(400) RETRY(0) ./notify.sh %C,%T time=%t %d",
  "\"localhost\", 20, 0 : UID(guest) ENV(DISPLAY=optimator.csd:0.0) ENV(SHELL=/bin/ksh) TIME(400) RETRY(0) ./notify.sh %C,%T time=%t %d",
  "\"localhost\", 0, 10 : UID(guest) ENV(DISPLAY=optimator.csd:0.0) ENV(SHELL=/bin/ksh) TIME(400) RETRY(0) ./notify.sh %C,%T time=%t %d",
  0,
};

const int Assdb_event_rules[] = 
{
  1, 1, 1,
  1, 2, 1,
  1, 3, 1,
  1, 4, 2,
  2, 2, 3,
  0,
};
#endif
