// PCI config space access functions.
//
// Copyright (C) 2008  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2002  MandrakeSoft S.A.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "output.h" // dprintf
#include "pci.h" // pci_config_writel
#include "pci_regs.h" // PCI_VENDOR_ID
#include "util.h" // udelay
#include "x86.h" // outl

#define PORT_PCI_CMD           0x0cf8
#define PORT_PCI_DATA          0x0cfc

static u32 mmconfig;

static void *mmconfig_addr(u16 bdf, u32 addr)
{
    return (void*)(mmconfig + ((u32)bdf << 12) + addr);
}

static u32 ioconfig_cmd(u16 bdf, u32 addr)
{
    /*
     * 0xfc=11111100 只取addr的3-8位 
     */
    return 0x80000000 | (bdf << 8) | (addr & 0xfc);  
}

void pci_config_writel(u16 bdf, u32 addr, u32 val)
{
    if (!MODESEGMENT && mmconfig) {
        writel(mmconfig_addr(bdf, addr), val);
    } else {
        olly_printf("pci_config_writel port=0xcf8 bdf=0x%x ,addr=0x%x , ioconfig_cmd(bdf, addr)=0x%x\n", bdf, addr, ioconfig_cmd(bdf, addr));
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        olly_printf("pci_config_writel port=0xcfc val=0x%x \n", val);
        outl(val, PORT_PCI_DATA);
    }
}

void pci_config_writew(u16 bdf, u32 addr, u16 val)
{
    if (!MODESEGMENT && mmconfig) {
        writew(mmconfig_addr(bdf, addr), val);
    } else {
        //0xcf8端口记录下bdf
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        //写value到 0xcfc端口
        outw(val, PORT_PCI_DATA + (addr & 2));
    }
}

void pci_config_writeb(u16 bdf, u32 addr, u8 val)
{
    if (!MODESEGMENT && mmconfig) {
        writeb(mmconfig_addr(bdf, addr), val);
    } else {
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        outb(val, PORT_PCI_DATA + (addr & 3));
    }
}

u32 pci_config_readl(u16 bdf, u32 addr)
{
    //olly_printf("%s","0 --------------####--------------pci_config_readl ----------###---------- \n");
    if (!MODESEGMENT && mmconfig) {
        return readl(mmconfig_addr(bdf, addr));
    } else {
        //olly_printf("pci_config_readl : out ioconfig_cmd(bdf, addr)=0x%x port=0x%x \n", ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        //olly_printf("pci_config_readl : in port=0x%x \n", PORT_PCI_DATA);
        return inl(PORT_PCI_DATA);
    }
}

u16 pci_config_readw(u16 bdf, u32 addr)
{
    //olly_printf("%s","0 --####--pci_config_readw --###-- \n");
    if (!MODESEGMENT && mmconfig) {
        //olly_printf("%s","1 --####--pci_config_readw --###-- \n");
        return readw(mmconfig_addr(bdf, addr));
    } else {
        //olly_printf("2 --####--pci_config_readw --###-- ioconfig_cmd :0x%x \n", ioconfig_cmd(bdf, addr));
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        return inw(PORT_PCI_DATA + (addr & 2));
    }
}

u8 pci_config_readb(u16 bdf, u32 addr)
{
    //olly_printf("0 --####--pci_config_readb --###-- ioconfig_cmd(bdf, addr)=0x%x \n", ioconfig_cmd(bdf, addr));
    if (!MODESEGMENT && mmconfig) {
        return readb(mmconfig_addr(bdf, addr));
    } else {
        //olly_printf("0 --pci_config_readb --###-- out: ioconfig_cmd(bdf, addr)=0x%x \n", ioconfig_cmd(bdf, addr));
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD);  //0x0cf8
        //olly_printf("1 --####--pci_config_readb --###-- in: PORT_PCI_DATA+(addr & 3)=0x%x \n", PORT_PCI_DATA+(addr & 3));
        return inb(PORT_PCI_DATA + (addr & 3)); //0xcfc
    }
}

void
pci_config_maskw(u16 bdf, u32 addr, u16 off, u16 on)
{
    u16 val = pci_config_readw(bdf, addr);
    val = (val & ~off) | on;
    pci_config_writew(bdf, addr, val);
}

void
pci_enable_mmconfig(u64 addr, const char *name)
{
    if (addr >= 0x100000000ll)
        return;
    dprintf(1, "PCIe: using %s mmconfig at 0x%llx\n", name, addr);
    mmconfig = addr;
}

u8 pci_find_capability(u16 bdf, u8 cap_id, u8 cap)
{
    int i;
    u16 status = pci_config_readw(bdf, PCI_STATUS);//0x06

    if (!(status & PCI_STATUS_CAP_LIST))
        return 0;

    if (cap == 0) {
        /* find first */
        cap = pci_config_readb(bdf, PCI_CAPABILITY_LIST); //0x34
    } else {
        /* find next */
        cap = pci_config_readb(bdf, cap + PCI_CAP_LIST_NEXT);
    }
    for (i = 0; cap && i <= 0xff; i++) {
        if (pci_config_readb(bdf, cap + PCI_CAP_LIST_ID) == cap_id)
            return cap;
        cap = pci_config_readb(bdf, cap + PCI_CAP_LIST_NEXT);
    }

    return 0;
}

// Helper function for foreachbdf() macro - return next device
int
pci_next(int bdf, int bus)
{
    //olly_printf("%s","0 --####--pci_next --###-- \n");
    //0号function,并且是multi function设备
    if (pci_bdf_to_fn(bdf) == 0
        && (pci_config_readb(bdf, PCI_HEADER_TYPE) & 0x80) == 0)  //最高bit为1 ，什么意思?
        // Last found device wasn't a multi-function device - skip to
        // the next device.
        bdf += 8; 
    else
        bdf += 1;//0号fn跳过

    
    for (;;) {
        if (pci_bdf_to_bus(bdf) != bus)
            return -1; //

        //olly_printf("1 --####--pci_next --###-- bdf=0x%x bus=0x%x \n", bdf, bus);
        u16 v = pci_config_readw(bdf, PCI_VENDOR_ID);
        if (v != 0x0000 && v != 0xffff)
            // Device is present.
            return bdf;

        if (pci_bdf_to_fn(bdf) == 0)//是0号fn，并且这个fn返回的PCI_VENDOR_ID ==0,说明这个dev号下面没有fn了, 就跳到下一个去了
            bdf += 8;
        else
            bdf += 1;
    }
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     platform_hardware_setup()
 *      qemu_platform_setup()
 *       pci_setup()
 *        pci_probe_host()
 */
// Check if PCI is available at all
int
pci_probe_host(void)
{
    outl(0x80000000, PORT_PCI_CMD);
    olly_printf("----------------------------------------pci_probe_host 0\n");
    if (inl(PORT_PCI_CMD) != 0x80000000) {
        olly_printf("----------------------------------------pci_probe_host 1\n");
        dprintf(1, "Detected non-PCI system\n");
        return -1;
    }
    olly_printf("----------------------------------------pci_probe_host 2\n");

    return 0;
}

void
pci_reboot(void)
{
    u8 v = inb(PORT_PCI_REBOOT) & ~6;
    outb(v|2, PORT_PCI_REBOOT); /* Request hard reset */
    udelay(50);
    outb(v|6, PORT_PCI_REBOOT); /* Actually do the reset */
    udelay(50);
}
