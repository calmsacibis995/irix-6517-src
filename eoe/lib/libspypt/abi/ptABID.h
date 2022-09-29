#ifndef ABIDEFS_O32
#define ABIDEFS_O32
struct pt_lib_info_tInfo pt_lib_info_tInfoO32 = {
	20, 	0, 	8, 	16, };

struct pt_tInfo pt_tInfoO32 = {
	232, 	16, 	20, 	12, 	36, 	44, 	40, 	116, 	80, 	120, };

struct vp_tInfo vp_tInfoO32 = {
	40, 	8, 	28, 	12, };

struct mtx_tInfo mtx_tInfoO32 = {
	32, 	28, 	12, 	4, 	0, 	4, 	8, };

struct mtxattr_tInfo mtxattr_tInfoO32 = {
	4, 	0, 	1, 	2, };

struct prda_lib_tInfo prda_lib_tInfoO32 = {
	112, 	88, };

struct prda_sys_tInfo prda_sys_tInfoO32 = {
	120, 	64, };


#endif
#ifndef ABIDEFS_N32
#define ABIDEFS_N32
struct pt_lib_info_tInfo pt_lib_info_tInfoN32 = {
	20, 	0, 	8, 	16, };

struct pt_tInfo pt_tInfoN32 = {
	344, 	16, 	20, 	12, 	36, 	44, 	40, 	116, 	80, 	120, };

struct vp_tInfo vp_tInfoN32 = {
	40, 	8, 	28, 	12, };

struct mtx_tInfo mtx_tInfoN32 = {
	32, 	28, 	12, 	4, 	0, 	4, 	8, };

struct mtxattr_tInfo mtxattr_tInfoN32 = {
	4, 	0, 	1, 	2, };

struct prda_lib_tInfo prda_lib_tInfoN32 = {
	112, 	88, };

struct prda_sys_tInfo prda_sys_tInfoN32 = {
	120, 	64, };


#endif
#ifndef ABIDEFS_N64
#define ABIDEFS_N64
struct pt_lib_info_tInfo pt_lib_info_tInfoN64 = {
	32, 	0, 	8, 	24, };

struct pt_tInfo pt_tInfoN64 = {
	416, 	32, 	40, 	24, 	56, 	64, 	60, 	184, 	136, 	192, };

struct vp_tInfo vp_tInfoN64 = {
	64, 	16, 	48, 	24, };

struct mtx_tInfo mtx_tInfoN64 = {
	56, 	52, 	24, 	8, 	0, 	8, 	12, };

struct mtxattr_tInfo mtxattr_tInfoN64 = {
	4, 	0, 	1, 	2, };

struct prda_lib_tInfo prda_lib_tInfoN64 = {
	208, 	160, };

struct prda_sys_tInfo prda_sys_tInfoN64 = {
	120, 	64, };


#endif
struct pt_lib_info_tInfo *pt_lib_info_tInfo[] =
	{ &pt_lib_info_tInfoO32, &pt_lib_info_tInfoN32, &pt_lib_info_tInfoN64, };
struct pt_tInfo *pt_tInfo[] =
	{ &pt_tInfoO32, &pt_tInfoN32, &pt_tInfoN64, };
struct vp_tInfo *vp_tInfo[] =
	{ &vp_tInfoO32, &vp_tInfoN32, &vp_tInfoN64, };
struct mtx_tInfo *mtx_tInfo[] =
	{ &mtx_tInfoO32, &mtx_tInfoN32, &mtx_tInfoN64, };
struct mtxattr_tInfo *mtxattr_tInfo[] =
	{ &mtxattr_tInfoO32, &mtxattr_tInfoN32, &mtxattr_tInfoN64, };
struct prda_lib_tInfo *prda_lib_tInfo[] =
	{ &prda_lib_tInfoO32, &prda_lib_tInfoN32, &prda_lib_tInfoN64, };
struct prda_sys_tInfo *prda_sys_tInfo[] =
	{ &prda_sys_tInfoO32, &prda_sys_tInfoN32, &prda_sys_tInfoN64, };
