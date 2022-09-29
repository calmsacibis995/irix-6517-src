/* ----------------------------------------------------------------- */
/*                          SSDBIUTIL.H                              */
/* ----------------------------------------------------------------- */
#ifndef H_SSDBIUTIL_H
#define H_SSDBIUTIL_H

#include "ssdbidata.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Critical sections */
int  ssdbiu_initCritialSections(void);
int  ssdbiu_doneCritialSections(void);

/* Client */
int  ssdbiu_checkClientHandler(ssdb_CLIENT *cli,int strictFlg);
ssdb_CLIENT* ssdbiu_newClientHandler(void);
ssdb_CLIENT *ssdbiu_delClientHandler(ssdb_CLIENT *cli);
int  ssdbiu_lockClientHandler(ssdb_CLIENT *cli);
int  ssdbiu_unlockClientHandler(ssdb_CLIENT *cli);
void ssdbiu_dropFreeClientList(void);
ssdb_CLIENT* ssdbiu_removeFirstClientFromWorkList(void);
ssdb_CLIENT* ssdbiu_getFirstClientFromWorkList(void);
ssdb_CLIENT* ssdbiu_addClientHandlerToWorkList(ssdb_CLIENT *cli);
ssdb_CLIENT* ssdbiu_subClientHandlerFromWorkList(ssdb_CLIENT *cli);

/* Connection */
ssdb_CONNECTION* ssdbiu_newConnectionHandler(void);
ssdb_CONNECTION *ssdbiu_delConnectionHandler(ssdb_CONNECTION *con);
int  ssdbiu_checkConnectionHandler(ssdb_CONNECTION *con);
int  ssdbiu_lockConnectionHandler(ssdb_CONNECTION *con);
int  ssdbiu_unlockConnectionHandler(ssdb_CONNECTION *con);
void ssdbiu_dropFreeConnectionList(void);

/* Request */
ssdb_REQUEST *ssdbiu_newRequestHandler(void);
void ssdbiu_delRequestHandler(ssdb_REQUEST *req);
int  ssdbiu_checkRequestHandler(ssdb_REQUEST *req);
void ssdbiu_dropFreeRequestList(void);
int  ssdbiu_allocRequestFieldInfo(ssdb_REQUEST *req, int numfields);
int  ssdbiu_allocRequestResultData(ssdb_REQUEST *req,int size);
ssdb_REQUEST *ssdbiu_freeRequestFieldInfo(ssdb_REQUEST *req);
ssdb_REQUEST *ssdbiu_freeRequestMySqlResult(ssdb_REQUEST *req);
ssdb_REQUEST *ssdbiu_freeRequestResultData(ssdb_REQUEST *req);

/* Error (Handler) */
ssdb_LASTERR *ssdbiu_initializeErrorHandler(ssdb_LASTERR *err);
ssdb_LASTERR *ssdbiu_newErrorHandler(void);
void ssdbiu_delErrorHandler(ssdb_LASTERR *err);
int  ssdbiu_checkErrorHandler(ssdb_LASTERR *err);
void ssdbiu_dropFreeErrorList(void);
ssdb_LASTERR *ssdbiu_addErrorHandlerToWorkList(ssdb_LASTERR *err);
ssdb_LASTERR *ssdbiu_subErrorHandlerFromWorkList(ssdb_LASTERR *err);
void ssdbiu_dropWorkErrorList(void);

/* Misc: Client & Connect */
ssdb_CLIENT *ssdbiu_getClientHandleByAny(void *something);
ssdb_CONNECTION *ssdbiu_removeFirstConnectionFromClient(ssdb_CLIENT *cli);
ssdb_CLIENT *ssdbiu_addConnectionToClient(ssdb_CLIENT *cli,ssdb_CONNECTION *con);
ssdb_CLIENT *ssdbiu_subConnectionFromClient(ssdb_CLIENT *cli,ssdb_CONNECTION *con);
int  ssdbiu_checkConnectionInClient(ssdb_CLIENT *cli,ssdb_CONNECTION *con);

/* Misk: Connect & Request */
ssdb_REQUEST *ssdbiu_addRequestHandlerToConnect(ssdb_CONNECTION *con,ssdb_REQUEST *req);
ssdb_REQUEST *ssdbiu_subRequestHandlerFromConnect(ssdb_CONNECTION *con,ssdb_REQUEST *req);
ssdb_REQUEST *ssdbiu_removeFirstRequestHandlerFromConnect(ssdb_CONNECTION *con);
int  ssdbiu_checkRequestHandlerInConnect(ssdb_CONNECTION *con,ssdb_REQUEST *req);
int  ssdbiu_getFieldByteSize(int mysql_type,ssdbiu_typeconvert_request **fptr);

/* String */
char *ssdbiu_makeStringFromString(const char *srcstr,int *dstlen);
void ssdbiu_freeString(char *str);
int  ssdbiu_CopyString(const char *src,char *dst,int dst_max_size,int *dst_actual_size);
int  ssdbiu_CopyString2(const char *src,char *dst,int src_actual_size,int dst_max_size,int *dst_actual_size);
int  ssdbiu_StrCheckSymbol(const char *srcstr,const char *patterns);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef H_SSDBIUTIL_H */

