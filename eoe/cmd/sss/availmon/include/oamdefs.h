#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/param.h>

#define AMRVERSIONNUM	"2.1"
#define MAX_LINE_LEN	1024
#define MAXINPUTLINE    2048
#define MAX_TAGS        4
#define MAX_STR_LEN	80
#define SEND_TYPE_NO    7

typedef enum {
    amc_autoemail, amc_shutdownreason, amc_tickerd, amc_hinvupdate,
    amc_livenotification, amc_statusinterval, amc_autoemaillist, amc_num
} amconf_t;

typedef struct {
    char	*flagname;
    char	*filename;
} amc_t;

typedef struct {
    char        *tagname;
    char        *tagdescr;
} amc_tags;

amc_tags  tags[MAX_TAGS] = {
    {"MAINT-NEEDED", "System in deferred maintainence mode"},
    {"SYS-DEGRADED", "System is running in a degraded mode"},
    {"CONFIG-ISSUE", "Local configuration issue           "},
    {"TOOK-ACTION" , "System took action described above  "} 
};

amc_t	amcs[] = {
    {"autoemail",	"/var/adm/avail/config/autoemail"},
    {"shutdownreason",	"/var/adm/avail/config/shutdownreason"},
    {"tickerd",		"/var/adm/avail/config/tickerd"},
    {"hinvupdate",	"/var/adm/avail/config/hinvupdate"},
    {"livenotification","/var/adm/avail/config/livenotification"},
    {"statusinterval",	"/var/adm/avail/config/statusinterval"},
    {"autoemail.list",	"/var/adm/avail/config/autoemail.list"}
};

amc_t	samcs[] = {
    {"autoemail",	"/var/adm/avail/.save/autoemail"},
    {"shutdownreason",	"/var/adm/avail/.save/shutdownreason"},
    {"tickerd",		"/var/adm/avail/.save/tickerd"},
    {"hinvupdate",	"/var/adm/avail/.save/hinvupdate"},
    {"livenotification","/var/adm/avail/.save/livenotification"},
    {"statusinterval",	"/var/adm/avail/.save/statusinterval"},
    {"autoemail.list",	"/var/adm/avail/.save/autoemail.list"}
};

char	*sendtypestr[SEND_TYPE_NO] = {
    "availability(compressed,encrypted):",
    "availability(compressed):",
    "availability(text):",
    "diagnosis(compressed,encrypted):",
    "diagnosis(compressed):",
    "diagnosis(text):",
    "pager(concise text):"
};
