#include <cdi.h>
#include <cdi/pci.h>

#ifdef X86
#include <drivers/ports.h>
#endif


#ifdef X86
void cdi_pci_alloc_ioports(struct cdi_pci_device *device)
{
    (void)device;

    iopl(3);
}
#endif
