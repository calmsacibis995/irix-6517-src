#ifndef __XLV_CAP_H__
#define __XLV_CAP_H__

#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/syssgi.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

__inline static int
cap_mknod (const char *path, mode_t mode, dev_t dev)
{
	cap_t ocap;
	const cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	int r;

	ocap = cap_acquire (1, &cap_device_mgt);
	r = mknod (path, mode, dev);
	cap_surrender (ocap);
	return (r);
}

__inline static int
cap_dev_syssgi (int request, void *arg1, void *arg2)
{
	cap_t ocap;
	const cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	int r;

	ocap = cap_acquire (1, &cap_device_mgt);
	r = syssgi (request, arg1, arg2);
	cap_surrender (ocap);
	return (r);
}

__inline static int
cap_vht_syssgi (int cmd, vertex_hdl_t hdl, void *hdlp)
{
	cap_t ocap;
	const cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	int r;
	
	ocap = cap_acquire (1, &cap_device_mgt);
	r = syssgi (cmd, hdl, hdlp);
	cap_surrender (ocap);
	return (r);
}

__inline static int
cap_ptr_syssgi (int cmd, void *ptr)
{
	cap_t ocap;
	const cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	int r;
	
	ocap = cap_acquire (1, &cap_device_mgt);
	r = syssgi (cmd, ptr);
	cap_surrender (ocap);
	return (r);
}

#ifdef __cplusplus
}
#endif

#endif /* __XLV_CAP_H__ */
