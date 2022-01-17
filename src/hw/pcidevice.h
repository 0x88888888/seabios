#ifndef __PCIDEVICE_H
#define __PCIDEVICE_H

#include "types.h" // u32
#include "list.h" // hlist_node

struct pci_device {
    u16 bdf;
    u8 rootbus;
    struct hlist_node node;
    struct pci_device *parent;

    // Configuration space device information
    u16 vendor, device;
    u16 class;
    u8 prog_if, revision;
    u8 header_type;
    u8 secondary_bus;

    // Local information on device.
    int have_driver;
};
extern struct hlist_head PCIDevices;
extern int MaxPCIBus;

static inline u32 pci_classprog(struct pci_device *pci) {
    return (pci->class << 8) | pci->prog_if;
}

#define foreachpci(PCI)                                 \
    hlist_for_each_entry(PCI, &PCIDevices, node)

#define PCI_ANY_ID      (~0)
struct pci_device_id {
    u32 vendid;
    u32 devid;
    u32 class;
    u32 class_mask;
    /*
     * ich9_lpc_fadt_setup，
     * ich9_smbus_setup
     * mch_mem_addr_setup,
     * piix4_fadt_setup,
     * piix_isa_bridge_setup
     * mch_isa_bridge_setup
     * storage_ide_setup
     * piix_ide_setup
     * pic_ibm_setup
     * piix4_pm_setup
     * 
     * apple_macio_setup
     * intel_igd_setup
     * i440fx_mem_addr_setup
     * found_compatibleahci
     */
    void (*func)(struct pci_device *pci, void *arg);
};

#define PCI_DEVICE(vendor_id, device_id, init_func)     \
    {                                                   \
        .vendid = (vendor_id),                          \
        .devid = (device_id),                           \
        .class = PCI_ANY_ID,                            \
        .class_mask = 0,                                \
        .func = (init_func)                             \
    }

#define PCI_DEVICE_CLASS(vendor_id, device_id, class_code, init_func)   \
    {                                                                   \
        .vendid = (vendor_id),                                          \
        .devid = (device_id),                                           \
        .class = (class_code),                                          \
        .class_mask = ~0,                                               \
        .func = (init_func)                                             \
    }

#define PCI_DEVICE_END                          \
    {                                           \
        .vendid = 0,                            \
    }

void pci_probe_devices(void);
struct pci_device *pci_find_device(u16 vendid, u16 devid);
struct pci_device *pci_find_class(u16 classid);
int pci_init_device(const struct pci_device_id *ids
                    , struct pci_device *pci, void *arg);
struct pci_device *pci_find_init_device(const struct pci_device_id *ids
                                        , void *arg);
void pci_enable_busmaster(struct pci_device *pci);
u16 pci_enable_iobar(struct pci_device *pci, u32 addr);
void *pci_enable_membar(struct pci_device *pci, u32 addr);

#endif // pcidevice.h
