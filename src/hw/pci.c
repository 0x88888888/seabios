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

//MCH(PCI HOST)的两个端口
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
     * 0xfc=11111100 只取addr的3-8位 ,低两位没有被encode进去
     */
    return 0x80000000 | (bdf << 8) | (addr & 0xfc);  
}

void pci_config_writel(u16 bdf, u32 addr, u32 val)
{

    if (!MODESEGMENT && mmconfig) { //MMIO的方式访问
        if(bdf == 0x00f8) {
            olly_printf("pci_config_writel: addr =0x%p  val=0x%x\n", mmconfig_addr(bdf, addr), val);
        }
        writel(mmconfig_addr(bdf, addr), val);
    } else { //pio的方式访问

        //olly_printf("pci_config_writel port=0xcf8 bdf=0x%x ,addr=0x%x , ioconfig_cmd(bdf, addr)=0x%x\n", bdf, addr, ioconfig_cmd(bdf, addr));
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        //olly_printf("pci_config_writel port=0xcfc val=0x%x \n", val);
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
    /*
     * mmio的方式访问pci设备的配置空间
     */
    if (!MODESEGMENT && mmconfig) {
        writeb(mmconfig_addr(bdf, addr), val);
    } else {
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        outb(val, PORT_PCI_DATA + (addr & 3));
    }
}

//pci_bios_get_bar
u32 pci_config_readl(u16 bdf, u32 addr)
{
    //olly_printf("%s","0 --------------####--------------pci_config_readl ----------###---------- \n");
    if (!MODESEGMENT && mmconfig) {
        if(bdf == 0x00f8) {
            olly_printf("pci_config_readl: addr =%p\n", mmconfig_addr(bdf, addr));
        }
        return readl(mmconfig_addr(bdf, addr));
    } else {
        //olly_printf("pci_config_readl : out ioconfig_cmd(bdf, addr)=0x%x port=0x%x \n", ioconfig_cmd(bdf, addr), PORT_PCI_CMD);
        outl(ioconfig_cmd(bdf, addr), PORT_PCI_CMD); //0x0cf8
        //olly_printf("pci_config_readl : in port=0x%x \n", PORT_PCI_DATA);
        return inl(PORT_PCI_DATA);  //0x0cfc
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
        return inw(PORT_PCI_DATA + (addr & 2)); /* 地址的低2位，encode进去 */
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
    //读出offset在addr处的值
    u16 val = pci_config_readw(bdf, addr);

    //给这个val分别设置off 和on bit
    val = (val & ~off) | on;
    //写回去
    pci_config_writew(bdf, addr, val);
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
 *          mch_mem_addr_setup()
 *           pci_enable_mmconfig(Q35_HOST_BRIDGE_PCIEXBAR_ADDR==0xb0000000, "q35");
 */ 
void
pci_enable_mmconfig(u64 addr, const char *name)
{
    if (addr >= 0x100000000ll)
        return;

    olly_printf("\n\n\n\n PCIe: using %s mmconfig at 0x%llx\n\n\n\n", name, addr);    
    //dprintf(1, "\n\n\n\n PCIe: using %s mmconfig at 0x%llx\n\n\n\n", name, addr);
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

    //最多支持256个capability
    for (i = 0; cap && i <= 0xff; i++) {
        if (pci_config_readb(bdf, cap + PCI_CAP_LIST_ID) == cap_id)
            return cap; //找到
            
        //cap为下一个cap在pcie配置空间中的偏移
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
        && (pci_config_readb(bdf, PCI_HEADER_TYPE) & 0x80) == 0)  //最高bit为1 ,multi funciton
        // Last found device wasn't a multi-function device - skip to
        // the next device.
        bdf += 8; 
    else
        bdf += 1;//bdf的下一个bdf

    
    for (;;) {
        if (pci_bdf_to_bus(bdf) != bus)
            return -1; //bdf得到的bus号不在当前讨论范围

        olly_printf("1 --####--pci_next --###-- bdf=0x%x bus=0x%x \n", bdf, bus);
        u16 v = pci_config_readw(bdf, PCI_VENDOR_ID);
        olly_printf("PCI_VENDOR_ID = 0x%x\n", v);
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
 * 
 * 查找mch(pci host) 是否启用
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
