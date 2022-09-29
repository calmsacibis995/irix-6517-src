#ifndef H_SSSSESSION_H
#define H_SSSSESSION_H

#include "rgAPI.h"

/* --------------------------------------------------------------------------- */
typedef struct sss_s_RGPLUGINHEAD {
   struct sss_s_RGPLUGINHEAD *link;
   char pname[128];
   srgSessionID session;   
} rgPluginHead;

/* --------------------------------------------------------------------------- */
#define MAX_RG_ARGS 512
typedef struct sss_s_SSSSESSION {
   struct sss_s_SSSSESSION *next;

   /* General data */
   streamHandle out;
   sscErrorHandle err;

   /* RG data */
   rgPluginHead *openRGSessions;
   int argc;
   char *argv[MAX_RG_ARGS];
} sssSession;

#ifdef __cplusplus
extern "C" {
#endif

sssSession *newSSSSession(sscErrorHandle hError);
void        closeSSSSession(sssSession *SSS);
void        addRGPlugin(const char *pluginName, srgSessionID session, sssSession *SSS);

#ifdef __cplusplus
}
#endif
#endif /* #ifndef H_SSSSESSION_H */

