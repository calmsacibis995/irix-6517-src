/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __MGRAS_IDE_TIMINGTABLES_H__

#define __MGRAS_IDE_TIMINGTABLES_H__


/* contains the following timing tables:
 * Vid_1280_1024_60_0, Vid_1280_1024_60 (from mgras_ltab_1280_1024_60_1RSS)
 * Vid_1280_1024_72_0, Vid_1280_1024_72 (from mgras_ltab_1280_1024_72_1RSS)
 * Vid_1024_768_60p_0, Vid_1024_768_60p (from mgras_ltab_1024_768_60_P_1RSS)
 */


/***************************************************************************
 * 1280x1024 @ 60 Hz (Default monitor timing)
 **************************************************************************/

ushort_t Vid_1280_1024_60_0 [] = {
    0x1713, 0x53c1, 0x3c13, 0x53c1,
    0x3d13, 0x57c1, 0x2413, 0x57c9,
    0x7fd3, 0x7fd3, 0x7fd3, 0x7fd3,
    0x7fd3, 0x05d3, 0x0e93, 0x8613,
    0xd7c1, 0x0000, 0x1713, 0x77c1,
    0x3c13, 0x73c1, 0x3d13, 0x77c1,
    0x2413, 0x77c9, 0x7fd3, 0x7fd3,
    0x7fd3, 0x7fd3, 0x7fd3, 0x05d3,
    0x0e93, 0x8613, 0xf7c1, 0x0012,
    0x1713, 0x77c1, 0x3c13, 0x73c1,
    0x3d13, 0x77c1, 0x2413, 0x77c9,
    0x7fd3, 0x7fd3, 0x7fd3, 0x7fd3,
    0x60d3, 0x2453, 0x77d9, 0x0e97,
    0x8617, 0xf7d1, 0x0024, 0x1713,
    0x7fd1, 0x2513, 0x7bd1, 0x1797,
    0x3d17, 0x7fd1, 0x2417, 0x7fd9,
    0x7fd7, 0x7fd7, 0x7fd7, 0x7fd7,
    0x7fd7, 0x05d7, 0x0e97, 0x8617,
    0xffd1, 0x0037, 0x1713, 0x7fd1,
    0x2513, 0x7bd1, 0x1797, 0x3d17,
    0x7fd1, 0x2417, 0x7fd9, 0x7fd7,
    0x7fd7, 0x7fd7, 0x7fd7, 0x7fd7,
    0x05d7, 0x0e97, 0x8617, 0xffd1,
    0x004a, 0x1713, 0x3fd1, 0x2513,
    0x3bd1, 0x1797, 0x3d17, 0x3fd1,
    0x2417, 0x3fd9, 0x7fd7, 0x7fd7,
    0x7fd7, 0x7fd7, 0x7fd7, 0x05d7,
    0x0e97, 0x8617, 0xbfd1, 0x005d,
    0x1713, 0x7fd1, 0x2513, 0x7bd1,
    0x1797, 0x3d17, 0x7fd1, 0x1017,
    0x7fd9, 0x1495, 0x7fd5, 0x7fd5,
    0x7fd5, 0x7fd5, 0x70d5, 0x14d7,
    0x0e97, 0x8617, 0xffd1, 0x0070,
    0x1713, 0x7fd1, 0x2513, 0x7bd1,
    0x1797, 0x3d17, 0x7fd1, 0x1017,
    0x7fd9, 0x1495, 0x7fd5, 0x7fd5,
    0x7fd5, 0x7fd5, 0x2dd5, 0x3555,
    0x7ed9, 0x0e55, 0x7fd9, 0x14d7,
    0x0e97, 0x8617, 0xffd1, 0x0084,
    0x1733, 0x7fd3, 0x0833, 0x7bd3,
    0x0ba3, 0x12b3, 0x17b7, 0x2f37,
    0x7fd3, 0x0ebf, 0x103f, 0x7fdb,
    0x13bd, 0x01bc, 0x60fc, 0x0aec,
    0x7ffc, 0x36fc, 0x0bec, 0x7ffc,
    0x7ffc, 0x44fc, 0x13fe, 0x01ff,
    0x0ebf, 0x863f, 0xffd3, 0x009c,
    0x053b, 0x7fd3, 0x12b3, 0x0833,
    0x7bd3, 0x0ba3, 0x12b3, 0x17b7,
    0x2f37, 0x7fd3, 0x0ebf, 0x103f,
    0x7fdb, 0x13bd, 0x01bc, 0x60fc,
    0x0aec, 0x7ffc, 0x36fc, 0x0bec,
    0x7ffc, 0x7ffc, 0x44fc, 0x13fe,
    0x01ff, 0x0ebf, 0x863f, 0xffd3,
    0x00b8, 0x053b, 0x7fd3, 0x12b3,
    0x0833, 0x7bd3, 0x0ba3, 0x12b3,
    0x17b7, 0x2f37, 0x7fd3, 0x0ebf,
    0x103f, 0x7fdb, 0x13bd, 0x01bc,
    0x60fc, 0x0aec, 0x7ffc, 0x36fc,
    0x0bec, 0x7ffc, 0x7ffc, 0x44fc,
    0x13fe, 0x01ff, 0x0ebf, 0x863f,
    0xffd3, 0x00d5, 0x053b, 0x7fd3,
    0x12b3, 0x0833, 0x7bd3, 0x0ba3,
    0x12b3, 0x17b7, 0x2f37, 0x7fd3,
    0x0ebf, 0x103f, 0x7fdb, 0x13bd,
    0x01bc, 0x5ffc, 0x0bec, 0x7ffc,
    0x36fc, 0x0bec, 0x7ffc, 0x7ffc,
    0x44fc, 0x13fe, 0x01ff, 0x0ebf,
    0x863f, 0xffd3, 0x00f2, 0x053b,
    0x7fd3, 0x12b3, 0x0833, 0x7bd3,
    0x0ba3, 0x12b3, 0x17b7, 0x2f37,
    0x7fd3, 0x0ebf, 0x103f, 0x7fdb,
    0x13bd, 0x01bc, 0x60fc, 0x0aec,
    0x7ffc, 0x36fc, 0x0bec, 0x7ffc,
    0x7ffc, 0x44fc, 0x13fe, 0x01ff,
    0x04bf, 0x0a3f, 0x7fda, 0x863f,
    0xffd2, 0x010f, 0x051b, 0x7fd1,
    0x1293, 0x2513, 0x7bd1, 0x1797,
    0x3d17, 0x7fd1, 0x1017, 0x7fd9,
    0x1495, 0x7fd5, 0x7fd5, 0x7fd5,
    0x7fd5, 0x70d5, 0x14d7, 0x0e97,
    0x8617, 0xffd1, 0x012e, 0x1713,
    0x7fd1, 0x2513, 0x7bd1, 0x1797,
    0x3d17, 0x7fd1, 0x2417, 0x7fd9,
    0x7fd7, 0x7fd7, 0x7fd7, 0x7fd7,
    0x7fd7, 0x05d7, 0x0e93, 0x8613,
    0xffc1, 0x0143,
};

ushort_t Vid_1280_1024_60 [] = {
    0x0000, 0x0001, 0x0012, 0x0001,
    0x0024, 0x0001, 0x0037, 0x0001,
    0x004a, 0x0002, 0x005d, 0x0001,
    0x0070, 0x001e, 0x0084, 0x0001,
    0x009c, 0x0001, 0x00b8, 0x0059,
    0x00d5, 0x0080, 0x00f2, 0x0100,
    0x00b8, 0x0200, 0x00d5, 0x0025,
    0x010f, 0x0001, 0x012e, 0x0001,
    0x004a, 0x0001, 0x0143, 0x0001,
    0x0000, 0x0000,
};

/***************************************************************************
 * 1280 x 1024 @ 72 Hz
 **************************************************************************/

ushort_t Vid_1280_1024_72_0 [] = {
    0x1713, 0x53c1, 0x4613, 0x53c1,
    0x3813, 0x57c1, 0x2413, 0x57c9,
    0x7fd3, 0x7fd3, 0x7fd3, 0x7fd3,
    0x7fd3, 0x05d3, 0x0e93, 0x8113,
    0xd7c1, 0x0000, 0x1713, 0x77c1,
    0x4613, 0x73c1, 0x3813, 0x77c1,
    0x2413, 0x77c9, 0x7fd3, 0x7fd3,
    0x7fd3, 0x7fd3, 0x7fd3, 0x05d3,
    0x0e93, 0x8113, 0xf7c1, 0x0012,
    0x1713, 0x77c1, 0x4613, 0x73c1,
    0x3813, 0x77c1, 0x2413, 0x77c9,
    0x7fd3, 0x7fd3, 0x7fd3, 0x7fd3,
    0x60d3, 0x2453, 0x77d9, 0x0e97,
    0x8117, 0xf7d1, 0x0024, 0x1713,
    0x7fd1, 0x2f13, 0x7bd1, 0x1797,
    0x3817, 0x7fd1, 0x2417, 0x7fd9,
    0x7fd7, 0x7fd7, 0x7fd7, 0x7fd7,
    0x7fd7, 0x05d7, 0x0e97, 0x8117,
    0xffd1, 0x0037, 0x1713, 0x7fd1,
    0x2f13, 0x7bd1, 0x1797, 0x3817,
    0x7fd1, 0x2417, 0x7fd9, 0x7fd7,
    0x7fd7, 0x7fd7, 0x7fd7, 0x7fd7,
    0x05d7, 0x0e97, 0x8117, 0xffd1,
    0x004a, 0x1713, 0x3fd1, 0x2f13,
    0x3bd1, 0x1797, 0x3817, 0x3fd1,
    0x2417, 0x3fd9, 0x7fd7, 0x7fd7,
    0x7fd7, 0x7fd7, 0x7fd7, 0x05d7,
    0x0e97, 0x8117, 0xbfd1, 0x005d,
    0x1713, 0x7fd1, 0x2f13, 0x7bd1,
    0x1797, 0x3817, 0x7fd1, 0x1017,
    0x7fd9, 0x1495, 0x7fd5, 0x7fd5,
    0x7fd5, 0x7fd5, 0x70d5, 0x14d7,
    0x0e97, 0x8117, 0xffd1, 0x0070,
    0x1713, 0x7fd1, 0x2f13, 0x7bd1,
    0x1797, 0x3817, 0x7fd1, 0x1017,
    0x7fd9, 0x1495, 0x7fd5, 0x7fd5,
    0x7fd5, 0x7fd5, 0x12d5, 0x4055,
    0x7ed9, 0x1e55, 0x7fd9, 0x14d7,
    0x0e97, 0x8117, 0xffd1, 0x0084,
    0x1733, 0x7fd3, 0x0d33, 0x7bd3,
    0x0da3, 0x15b3, 0x17b7, 0x2a37,
    0x7fd3, 0x0ebf, 0x103f, 0x7fdb,
    0x13bd, 0x01bc, 0x60fc, 0x0cec,
    0x7ffc, 0x34fc, 0x0dec, 0x7ffc,
    0x7ffc, 0x42fc, 0x13fe, 0x01ff,
    0x0ebf, 0x813f, 0xffd3, 0x009c,
    0x0a3b, 0x7fd3, 0x0db3, 0x0d33,
    0x7bd3, 0x0da3, 0x15b3, 0x17b7,
    0x2a37, 0x7fd3, 0x0ebf, 0x103f,
    0x7fdb, 0x13bd, 0x01bc, 0x60fc,
    0x0cec, 0x7ffc, 0x34fc, 0x0dec,
    0x7ffc, 0x7ffc, 0x42fc, 0x13fe,
    0x01ff, 0x0ebf, 0x813f, 0xffd3,
    0x00b8, 0x0a3b, 0x7fd3, 0x0db3,
    0x0d33, 0x7bd3, 0x0da3, 0x15b3,
    0x17b7, 0x2a37, 0x7fd3, 0x0ebf,
    0x103f, 0x7fdb, 0x13bd, 0x01bc,
    0x5ffc, 0x0dec, 0x7ffc, 0x34fc,
    0x0dec, 0x7ffc, 0x7ffc, 0x42fc,
    0x10fe, 0x037e, 0x7fda, 0x01ff,
    0x0ebf, 0x813f, 0xffd2, 0x00d5,
    0x0a1b, 0x7fd1, 0x0d93, 0x2f13,
    0x7bd1, 0x1797, 0x3817, 0x7fd1,
    0x1017, 0x7fd9, 0x1495, 0x7fd5,
    0x7fd5, 0x7fd5, 0x7fd5, 0x70d5,
    0x14d7, 0x0e97, 0x8117, 0xffd1,
    0x00f4, 0x1713, 0x7fd1, 0x2f13,
    0x7bd1, 0x1797, 0x3817, 0x7fd1,
    0x2417, 0x7fd9, 0x7fd7, 0x7fd7,
    0x7fd7, 0x7fd7, 0x7fd7, 0x05d7,
    0x0e93, 0x8113, 0xffc1, 0x0109,
};

ushort_t Vid_1280_1024_72 [] = {
    0x0000, 0x0001, 0x0012, 0x0001,
    0x0024, 0x0001, 0x0037, 0x0001,
    0x004a, 0x0002, 0x005d, 0x0001,
    0x0070, 0x001e, 0x0084, 0x0001,
    0x009c, 0x0001, 0x00b8, 0x03fe,
    0x00d5, 0x0001, 0x00f4, 0x0001,
    0x004a, 0x0001, 0x0109, 0x0001,
    0x0000, 0x0000,
};

ushort_t Vid_1024_768_60p_0 [] = {
        0x1813, 0x4be1, 0x2813, 0x43e1,
        0x2c13, 0x47e1, 0x0ad3, 0x7f53,
        0x47e9, 0x7fd3, 0x7fd3, 0x79d3,
        0x0c93, 0xa013, 0xc7e1, 0x0000,
        0x1813, 0x67e1, 0x2813, 0x63e1,
        0x2c13, 0x67e1, 0x0ad3, 0x7f53,
        0x67e9, 0x7fd3, 0x7fd3, 0x79d3,
        0x0c93, 0xa013, 0xe7e1, 0x0010,
        0x1813, 0x67e1, 0x2813, 0x63e1,
        0x2c13, 0x67e1, 0x0ad3, 0x7f53,
        0x67e9, 0x7fd3, 0x7fd3, 0x79d3,
        0x0c97, 0xa017, 0xe7e1, 0x0020,
        0x1813, 0x67e1, 0x1013, 0x6be1,
        0x1897, 0x2c17, 0x6fe1, 0x0ad7,
        0x7f57, 0x6fe9, 0x7fd7, 0x7fd7,
        0x79d7, 0x0c97, 0xa017, 0xefe1,
        0x0030, 0x1813, 0x6fe1, 0x1013,
        0x6be1, 0x1897, 0x2c17, 0x6fe1,
        0x0ad7, 0x7f57, 0x6fe9, 0x7fd7,
        0x7fd7, 0x79d7, 0x0c97, 0xa017,
        0xefe1, 0x0041, 0x1813, 0x2fe1,
        0x1013, 0x2be1, 0x1897, 0x2c17,
        0x2fe1, 0x0ad7, 0x7f57, 0x2fe9,
        0x7fd7, 0x7fd7, 0x79d7, 0x0c97,
        0xa017, 0xafe1, 0x0052, 0x1813,
        0x6fe1, 0x1013, 0x6be1, 0x1897,
        0x1817, 0x6fe1, 0x1495, 0x0ad5,
        0x7f55, 0x6fe9, 0x7fd5, 0x7fd5,
        0x65d5, 0x14d7, 0x0c97, 0xa017,
        0xefe1, 0x0063, 0x1813, 0x6fe1,
        0x1013, 0x6be1, 0x1897, 0x1817,
        0x6fe1, 0x1495, 0x0ad5, 0x7f55,
        0x6fe9, 0x7fd5, 0x7fd5, 0x65d5,
        0x1457, 0x6ee9, 0x0c97, 0xa017,
        0xefe1, 0x0076, 0x0733, 0x6fe3,
        0x03a3, 0x0323, 0x6ff3, 0x0bb3,
        0x1033, 0x6bf3, 0x12b7, 0x06bf,
        0x183f, 0x6ff3, 0x13bd, 0x01bc,
        0x0afc, 0x567c, 0x6ffb, 0x06ec,
        0x59fc, 0x07ec, 0x7ffc, 0x7ffc,
        0x28fc, 0x13fe, 0x01ff, 0x0cbf,
        0x0d3f, 0x6ff3, 0x93b7, 0x008a,
        0x0733, 0x6ff7, 0x06a3, 0x0bb3,
        0x1033, 0x6bf7, 0x12b7, 0x06bf,
        0x183f, 0x6ff7, 0x13bd, 0x01bc,
        0x0afc, 0x567c, 0x6fff, 0x06ec,
        0x59fc, 0x07ec, 0x7ffc, 0x7ffc,
        0x28fc, 0x13fe, 0x01ff, 0x0cbf,
        0x0d3f, 0x6ff7, 0x93b7, 0x00a8,
        0x0733, 0x6ff3, 0x06a3, 0x0bb3,
        0x1033, 0x6bf3, 0x12b7, 0x06bf,
        0x183f, 0x6ff3, 0x13bd, 0x01bc,
        0x0afc, 0x557c, 0x6ffb, 0x07ec,
        0x59fc, 0x07ec, 0x7ffc, 0x7ffc,
        0x28fc, 0x13fe, 0x01ff, 0x0cbf,
        0x0d3f, 0x6ff3, 0x09b7, 0x8a37,
        0xeff2, 0x00c4, 0x0a13, 0x6ff1,
        0x0e13, 0x6fe1, 0x1013, 0x6be1,
        0x1897, 0x1817, 0x6fe1, 0x1495,
        0x0ad5, 0x7f55, 0x6fe9, 0x7fd5,
        0x7fd5, 0x65d5, 0x14d7, 0x0c97,
        0xa017, 0xefe1, 0x00e2, 0x1813,
        0x6fe1, 0x1013, 0x6be1, 0x1897,
        0x2c17, 0x6fe1, 0x0ad7, 0x7f57,
        0x6fe9, 0x7fd7, 0x7fd7, 0x79d7,
        0x0c97, 0x0717, 0x6fe1, 0x9917,
        0xefc1, 0x00f7
};

ushort_t Vid_1024_768_60p [] = {
        0x0000, 0x0001, 0x0010, 0x0001,
        0x0020, 0x0001, 0x0030, 0x0001,
        0x0041, 0x0001, 0x0052, 0x0001,
        0x0063, 0x001e, 0x0076, 0x0001,
        0x008a, 0x0001, 0x00a8, 0x02fe,
        0x00c4, 0x0001, 0x00e2, 0x0001,
        0x0041, 0x0001, 0x00f7, 0x0001,
        0x0000, 0x0000,
};

#endif /* __MGRAS_IDE_TIMINGTABLES_H__ */
