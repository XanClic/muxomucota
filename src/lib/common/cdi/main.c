#include <cdi.h>
#include <drivers.h>
#include <stdio.h>
#include <stdlib.h>
#include <cdi.h>
#include <cdi/fs.h>
#include <cdi/lists.h>
#include <cdi/net.h>


extern struct cdi_driver *__start_cdi_drivers, *__stop_cdi_drivers;


int main(void)
{
    if (&__start_cdi_drivers + 1 < &__stop_cdi_drivers)
    {
        fprintf(stderr, "Warning: Only one driver supported per binary. Ignoring the following:\n");

        for (struct cdi_driver **drvp = &__start_cdi_drivers + 1; drvp < &__stop_cdi_drivers; drvp++)
            fprintf(stderr, " - %s\n", (*drvp)->name);

        fprintf(stderr, "Running %s only.\n", __start_cdi_drivers->name);
    }

    switch ((int)__start_cdi_drivers->type)
    {
        case CDI_FILESYSTEM:
            cdi_fs_driver_register((struct cdi_fs_driver *)__start_cdi_drivers);
            break;
        default:
            fprintf(stderr, "Unknown driver type %i for %s.\n", __start_cdi_drivers->type, __start_cdi_drivers->name);
            return 1;
    }

    daemonize(__start_cdi_drivers->name);
}


void cdi_driver_init(struct cdi_driver *driver)
{
    driver->devices = cdi_list_create();
}

void cdi_driver_destroy(struct cdi_driver *driver)
{
    struct cdi_device *dev;

    while ((dev = cdi_list_pop(driver->devices)) != NULL)
    {
        if (driver->remove_device != NULL)
            driver->remove_device(dev);
        free(dev);
    }

    cdi_list_destroy(driver->devices);
}


void cdi_init(void)
{
}
