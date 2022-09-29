#ifndef _SSCHTMLPROCESSOR_H_
#define _SSCHTMLPROCESSOR_H_

#include "sscStreams.h"
#include "SSC.h"
#include "sssSession.h"


#ifdef __cplusplus
extern "C" {
#endif

int getSSSDataByURL(char *url,streamHandle out,sssSession *SSS);

#ifdef __cplusplus
}
#endif
#endif
