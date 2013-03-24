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
