/************************************************************************
 * Copyright (C) 2013 by Hanna Reitz                                    *
 *                                                                      *
 * This file is part of µxoµcota.                                       *
 *                                                                      *
 * µxoµcota  is free  software:  you can  redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the  Free Software Foundation,  either version 3 of the License,  or *
 * (at your option) any later version.                                  *
 *                                                                      *
 * µxoµcota  is  distributed  in the  hope  that  it  will  be  useful, *
 * but  WITHOUT  ANY  WARRANTY;  without even the  implied warranty  of *
 * MERCHANTABILITY  or  FITNESS  FOR  A  PARTICULAR  PURPOSE.  See  the *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have  received a copy of the  GNU  General Public License *
 * along with µxoµcota.  If not, see <http://www.gnu.org/licenses/>.    *
 ************************************************************************/

#include <cdi.h>
#include <cdi/scsi.h>


// TODO


void cdi_scsi_driver_init(struct cdi_scsi_driver *driver)
{
    cdi_driver_init((struct cdi_driver *)driver);
}

void cdi_scsi_driver_destroy(struct cdi_scsi_driver* driver)
{
    cdi_driver_destroy((struct cdi_driver *)driver);
}

void cdi_scsi_device_init(struct cdi_scsi_device *device __attribute__((unused)))
{
}
