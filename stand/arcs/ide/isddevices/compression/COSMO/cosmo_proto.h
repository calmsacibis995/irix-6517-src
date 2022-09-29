/*****************************************************************************
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 *
 *****************************************************************************/

#ifndef __JPEG_PROTO_H_
#define __JPEG_PROTO_H_

int	send_slave_I2C_msg (JPEG_8584_REG *i2c, long addr, long subaddr, long *data);
int	send_slave_I2C_tbl (JPEG_8584_REG *i2c, long, long, unsigned char *, int);

#endif /* __JPEG_PROTO_H_ */
