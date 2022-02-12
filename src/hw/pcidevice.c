// Code to maintain and access the pci_device cache
//
// Copyright (C) 2008-2016  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "malloc.h" // malloc_tmp
#include "output.h" // dprintf
#include "pci.h" // pci_config_writel
#include "pcidevice.h" // pci_probe_devices
#include "pci_regs.h" // PCI_VENDOR_ID
#include "romfile.h" // romfile_loadint
#include "stacks.h" // wait_preempt
#include "string.h" // memset

//所有的pci设备，在pci_probe_devices中构建
struct hlist_head PCIDevices VARVERIFY32INIT;
int MaxPCIBus VARFSEG;

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     platform_hardware_setup()
 *      qemu_platform_setup()
 *       pci_setup()
 *        pci_probe_devices()
 * 
 * 强行遍历所有的bdf,得到对应的pci device,添加到PCIDevices
 */ 
// Find all PCI devices and populate PCIDevices linked list.
void
pci_probe_devices(void)
{
    dprintf(3, "PCI probe\n");
    struct pci_device *busdevs[256];
    memset(busdevs, 0, sizeof(busdevs));
    //所有的pci设备都插入到这个链表
    struct hlist_node **pprev = &PCIDevices.first;
    int extraroots = romfile_loadint("etc/extra-pci-roots", 0);
    int bus = -1, lastbus = 0, rootbuses = 0, count=0;
    //所有的bus号
    
    while (bus < 0xff && (bus < MaxPCIBus || rootbuses < extraroots)) {
        bus++;
        int bdf;
        //bus下的设备
        olly_printf("+++++++++++++++++++++++++++++++++++++++++++++++++++ pci_probe_devices bus=0x%x\n", bus);
        foreachbdf(bdf, bus) { //qemu有这个设备，所以已经发现设备了

            olly_printf("pci_probe_devices: found device, bdf = 0x%x\n", bdf);    

            // Create new pci_device struct and add to list.
            struct pci_device *dev = malloc_tmp(sizeof(*dev));
            if (!dev) {
                warn_noalloc();
                return;
            }
            memset(dev, 0, sizeof(*dev));

            //将pci设备加入链表
            hlist_add(&dev->node, pprev);
            pprev = &dev->node.next;
            count++;

            // Find parent device.
            int rootbus;
            struct pci_device *parent = busdevs[bus];
            if (!parent) {
                if (bus != lastbus)
                    rootbuses++;
                lastbus = bus;
                rootbus = rootbuses;
                if (bus > MaxPCIBus)
                    MaxPCIBus = bus;
            } else {
                rootbus = parent->rootbus;
            }

            // Populate pci_device info.
            dev->bdf = bdf;
            dev->parent = parent;
            dev->rootbus = rootbus;
            u32 vendev = pci_config_readl(bdf, PCI_VENDOR_ID); //0x00
            dev->vendor = vendev & 0xffff;
            dev->device = vendev >> 16;
            u32 classrev = pci_config_readl(bdf, PCI_CLASS_REVISION); //0x08
            dev->class = classrev >> 16;
            dev->prog_if = classrev >> 8;
            dev->revision = classrev & 0xff;
            dev->header_type = pci_config_readb(bdf, PCI_HEADER_TYPE); // 0x0e

            u8 v = dev->header_type & 0x7f; //低7bit

            //bridge设备
            if (v == PCI_HEADER_TYPE_BRIDGE || v == PCI_HEADER_TYPE_CARDBUS) {
                u8 secbus = pci_config_readb(bdf, PCI_SECONDARY_BUS); //0x19
                dev->secondary_bus = secbus;

                if (secbus > bus && !busdevs[secbus])
                    busdevs[secbus] = dev;
                if (secbus > MaxPCIBus)
                    MaxPCIBus = secbus;
            }
            dprintf(4, "PCI device %pP (vd=%04x:%04x c=%04x)\n"
                    , dev, dev->vendor, dev->device, dev->class);
        }
    
        olly_printf("--------------------------------------------------- pci_probe_devices bus=0x%x\n", bus);
    }
    dprintf(1, "Found %d PCI devices (max PCI bus is %02x)\n", count, MaxPCIBus);

    
}

// Search for a device with the specified vendor and device ids.
struct pci_device *
pci_find_device(u16 vendid, u16 devid)
{
    struct pci_device *pci;
    foreachpci(pci) {
        if (pci->vendor == vendid && pci->device == devid)
            return pci;
    }
    return NULL;
}

// Search for a device with the specified class id.
struct pci_device *
pci_find_class(u16 classid)
{
    struct pci_device *pci;
    foreachpci(pci) {
        if (pci->class == classid)
            return pci;
    }
    return NULL;
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     platform_hardware_setup()
 *      qemu_platform_setup()
 *       pci_setup()
 *        pci_bios_init_platform()
 *         pci_init_device(ids==pci_platform_tbl)
 */ 
int pci_init_device(const struct pci_device_id *ids
                    , struct pci_device *pci, void *arg)
{
    while (ids->vendid || ids->class_mask) {

        if ((ids->vendid == PCI_ANY_ID || ids->vendid == pci->vendor) &&
            (ids->devid == PCI_ANY_ID || ids->devid == pci->device) &&
            !((ids->class ^ pci->class) & ids->class_mask)) {

            /*
            * ich9_lpc_fadt_setup，
            * ich9_smbus_setup
            * mch_mem_addr_setup,    [pci_host]
            * piix4_fadt_setup,
            * piix_isa_bridge_setup
            * mch_isa_bridge_setup   [ich9_lpc]
            * storage_ide_setup
            * piix_ide_setup
            * pic_ibm_setup
            * piix4_pm_setup
            * 
            * apple_macio_setup
            * intel_igd_setup
            * i440fx_mem_addr_setup
            * found_compatibleahci
            * 
            *  
            */
            if (ids->func){ // Q35:mch_mem_addr_setup,  [ src/fw/pciinit.c ]
                olly_printf("\n ids_func = %p  ids->devid=0x%x \n", ids->func, ids->devid);
                ids->func(pci, arg);
                
                
                //outb('5', 0x9731);
            }
            return 0;
        }
        ids++;//next
    }
    return -1;
}

struct pci_device *
pci_find_init_device(const struct pci_device_id *ids, void *arg)
{
    struct pci_device *pci;
    foreachpci(pci) {
        if (pci_init_device(ids, pci, arg) == 0)
            return pci;
    }
    return NULL;
}

// Enable PCI bus-mastering (ie, DMA) support on a pci device
void
pci_enable_busmaster(struct pci_device *pci)
{
    wait_preempt();
    pci_config_maskw(pci->bdf, PCI_COMMAND, 0, PCI_COMMAND_MASTER);
    pci->have_driver = 1;
}

// Verify an IO bar and return it to the caller
u16
pci_enable_iobar(struct pci_device *pci, u32 addr)
{
    wait_preempt();
    u32 bar = pci_config_readl(pci->bdf, addr);
    if (!(bar & PCI_BASE_ADDRESS_SPACE_IO)) {
        warn_internalerror();
        return 0;
    }
    bar &= PCI_BASE_ADDRESS_IO_MASK;
    if (bar == 0 || bar > 0xffff) {
        warn_internalerror();
        return 0;
    }
    pci_config_maskw(pci->bdf, PCI_COMMAND, 0, PCI_COMMAND_IO);
    pci->have_driver = 1;
    return bar;
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     device_hardware_setup()
 *      block_setup()
 *       ahci_setup()
 *        ahci_scan()
 *         ahci_controller_setup()
 *          pci_enable_membar()
 */ 
// Verify a memory bar and return it to the caller
void *
pci_enable_membar(struct pci_device *pci, u32 addr)
{
    wait_preempt();
    //读出pcie配置bar处的内容
    u32 bar = pci_config_readl(pci->bdf, addr);
    if (bar & PCI_BASE_ADDRESS_SPACE_IO) { //这个bar描述的是pio地址空间
        warn_internalerror();
        return NULL;
    }
    if (bar & PCI_BASE_ADDRESS_MEM_TYPE_64) { //mmio地址空间
        u32 high = pci_config_readl(pci->bdf, addr+4);
        if (high) { //高32bit只能为0
            dprintf(1, "Can not map memory bar over 4Gig\n"); 
            return NULL;
        }
    }

    //低4bit为0
    bar &= PCI_BASE_ADDRESS_MEM_MASK;
    //地址不在 [4M, 20M) 之内
    if (bar + 4*1024*1024 < 20*1024*1024) {
        // Bar doesn't look valid (it is in last 4M or first 16M)
        olly_printf("pci_enable_membar bar=0x%x\n", bar);
        warn_internalerror();
        return NULL;
    }

    //设置 PCI_COMMAND处，支持 mov指令访问pcie空间
    pci_config_maskw(pci->bdf, PCI_COMMAND, 0, PCI_COMMAND_MEMORY);

    pci->have_driver = 1;
    return (void*)bar;
}
