// 32bit code to Power On Self Test (POST) a machine.
//
// Copyright (C) 2008-2013  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2002  MandrakeSoft S.A.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // SET_BDA
#include "block.h" // block_setup
#include "bregs.h" // struct bregs
#include "config.h" // CONFIG_*
#include "e820map.h" // e820_add
#include "fw/paravirt.h" // qemu_cfg_preinit
#include "fw/xen.h" // xen_preinit
#include "hw/pic.h" // pic_setup
#include "hw/ps2port.h" // ps2port_setup
#include "hw/rtc.h" // rtc_write
#include "hw/serialio.h" // serial_debug_preinit
#include "hw/usb.h" // usb_setup
#include "malloc.h" // malloc_init
#include "memmap.h" // SYMBOL
#include "output.h" // dprintf
#include "string.h" // memset
#include "util.h" // kbd_init
#include "tcgbios.h" // tpm_*


/****************************************************************
 * BIOS initialization and hardware setup
 ****************************************************************/

static void
ivt_init(void)
{
    dprintf(3, "init ivt\n");

    // Initialize all vectors to the default handler.
    int i;
    for (i=0; i<256; i++)
        SET_IVT(i, FUNC16(entry_iret_official));

    // Initialize all hw vectors to a default hw handler.
    for (i=BIOS_HWIRQ0_VECTOR; i<BIOS_HWIRQ0_VECTOR+8; i++)
        SET_IVT(i, FUNC16(entry_hwpic1));
    for (i=BIOS_HWIRQ8_VECTOR; i<BIOS_HWIRQ8_VECTOR+8; i++)
        SET_IVT(i, FUNC16(entry_hwpic2));

    // Initialize software handlers.
    SET_IVT(0x02, FUNC16(entry_02));
    SET_IVT(0x05, FUNC16(entry_05));
    SET_IVT(0x10, FUNC16(entry_10));
    SET_IVT(0x11, FUNC16(entry_11));
    SET_IVT(0x12, FUNC16(entry_12));
    SET_IVT(0x13, FUNC16(entry_13_official));
    SET_IVT(0x14, FUNC16(entry_14));
    SET_IVT(0x15, FUNC16(entry_15_official));
    SET_IVT(0x16, FUNC16(entry_16));
    SET_IVT(0x17, FUNC16(entry_17));
    SET_IVT(0x18, FUNC16(entry_18));
    SET_IVT(0x19, FUNC16(entry_19_official));
    SET_IVT(0x1a, FUNC16(entry_1a_official));
    SET_IVT(0x40, FUNC16(entry_40));

    // INT 60h-66h reserved for user interrupt
    for (i=0x60; i<=0x66; i++)
        SET_IVT(i, SEGOFF(0, 0));

    // set vector 0x79 to zero
    // this is used by 'gardian angel' protection system
    SET_IVT(0x79, SEGOFF(0, 0));
}

static void
bda_init(void)
{
    dprintf(3, "init bda\n");

    struct bios_data_area_s *bda = MAKE_FLATPTR(SEG_BDA, 0);
    memset(bda, 0, sizeof(*bda));

    int esize = EBDA_SIZE_START;
    u16 ebda_seg = EBDA_SEGMENT_START;
    if (!CONFIG_MALLOC_UPPERMEMORY)
        ebda_seg = FLATPTR_TO_SEG(ALIGN_DOWN(SYMBOL(final_varlow_start), 1024)
                                  - EBDA_SIZE_START*1024);
    SET_BDA(ebda_seg, ebda_seg);

    SET_BDA(mem_size_kb, ebda_seg / (1024/16));

    // Init ebda
    struct extended_bios_data_area_s *ebda = get_ebda_ptr();
    memset(ebda, 0, sizeof(*ebda));
    ebda->size = esize;

    e820_add((u32)ebda, BUILD_LOWRAM_END-(u32)ebda, E820_RESERVED);

    // Init extra stack
    StackPos = &ExtraStack[BUILD_EXTRA_STACK_SIZE] - SYMBOL(zonelow_base);
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     interface_init()
 */ 
void
interface_init(void)
{
    // Running at new code address - do code relocation fixups
    olly_printf("0.........interface_init \n");
    malloc_init();
    olly_printf("1.........interface_init \n");

    // Setup romfile items.
    qemu_cfg_init();
    olly_printf("2.........interface_init \n");

    coreboot_cbfs_init();
    multiboot_init();

    // Setup ivt/bda/ebda
    ivt_init();
    bda_init();

    // Other interfaces
    boot_init();
    bios32_init();
    pmm_init();
    pnp_init();
    kbd_init();
    mouse_init();
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     device_hardware_setup()
 */ 
// Initialize hardware devices
void
device_hardware_setup(void)
{
    olly_printf("0------device_hardware_setup\n");
    usb_setup();
    olly_printf("1------device_hardware_setup\n");
    ps2port_setup();
    olly_printf("2------device_hardware_setup\n");
    block_setup();
    olly_printf("3------device_hardware_setup\n");
    lpt_setup();
    olly_printf("4------device_hardware_setup\n");
    serial_setup();
    olly_printf("5------device_hardware_setup\n");
    cbfs_payload_setup();
    olly_printf("6------device_hardware_setup\n");
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     platform_hardware_setup()
 */ 
static void
platform_hardware_setup(void)
{
    // Make sure legacy DMA isn't running.
    olly_printf("0------platform_hardware_setup\n");
    dma_setup();
    olly_printf("1------platform_hardware_setup\n");

    // Init base pc hardware.
    pic_setup();
    olly_printf("2------platform_hardware_setup\n");
    thread_setup();
    olly_printf("3------platform_hardware_setup\n");
    mathcp_setup();
    olly_printf("4------platform_hardware_setup\n");
    // Platform specific setup
    qemu_platform_setup();
    olly_printf("5------platform_hardware_setup\n");
    coreboot_platform_setup();
    olly_printf("6------platform_hardware_setup\n");

    // Setup timers and periodic clock interrupt
    timer_setup();
    olly_printf("7------platform_hardware_setup\n");
    clock_setup();
    olly_printf("8------platform_hardware_setup\n");

    // Initialize TPM
    tpm_setup();
    olly_printf("9------platform_hardware_setup\n");
}

void
prepareboot(void)
{
    // Change TPM phys. presence state befor leaving BIOS
    tpm_prepboot();

    // Run BCVs
    bcv_prepboot();

    // Finalize data structures before boot
    cdrom_prepboot();
    pmm_prepboot();
    malloc_prepboot();
    e820_prepboot();

    HaveRunPost = 2;

    // Setup bios checksum.
    BiosChecksum -= checksum((u8*)BUILD_BIOS_ADDR, BUILD_BIOS_SIZE);
}

// Begin the boot process by invoking an int0x19 in 16bit mode.
void VISIBLE32FLAT
startBoot(void)
{
    // Clear low-memory allocations (required by PMM spec).
    memset((void*)BUILD_STACK_ADDR, 0, BUILD_EBDA_MINIMUM - BUILD_STACK_ADDR);

    dprintf(3, "Jump to int19\n");
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.flags = F_IF;
    call16_int(0x19, &br);
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 */ 
// Main setup code.
static void
maininit(void)
{
    olly_printf("%s\n","0----------maininit \n");
    // Initialize internal interfaces.
    interface_init();
    olly_printf("%s\n","1----------maininit \n");

    // Setup platform devices.
    platform_hardware_setup();
    olly_printf("%s\n","2----------maininit \n");

    // Start hardware initialization (if threads allowed during optionroms)
    if (threads_during_optionroms())
        device_hardware_setup();

    olly_printf("%s\n","3----------maininit \n");
    // Run vga option rom
    vgarom_setup();
    olly_printf("%s\n","4----------maininit \n");
    sercon_setup();
    olly_printf("%s\n","5----------maininit \n");
    enable_vga_console();
    olly_printf("%s\n","6----------maininit \n");

    // Do hardware initialization (if running synchronously)
    if (!threads_during_optionroms()) {
        olly_printf("%s\n","66----------maininit \n");
        device_hardware_setup();
        olly_printf("%s\n","67----------maininit \n");
        wait_threads();
        olly_printf("%s\n","68----------maininit \n");
    }

    olly_printf("7----------maininit \n");
    // Run option roms
    optionrom_setup();
    olly_printf("8----------maininit \n");

    // Allow user to modify overall boot order.
    interactive_bootmenu();
    olly_printf("9----------maininit \n");
    wait_threads();
    olly_printf("10----------maininit \n");

    // Prepare for boot.
    prepareboot();

    // Write protect bios memory.
    make_bios_readonly();

    // Invoke int 19 to start boot process.
    startBoot();
}


/****************************************************************
 * POST entry and code relocation
 ****************************************************************/

// Update given relocs for the code at 'dest' with a given 'delta'
static void
updateRelocs(void *dest, u32 *rstart, u32 *rend, u32 delta)
{
    u32 *reloc;
    for (reloc = rstart; reloc < rend; reloc++)
        *((u32*)(dest + *reloc)) += delta;
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *  
// Relocate init code and then call a function at its new address.
// The passed function should be in the "init" section and must not
// return.
*/
void __noreturn
reloc_preinit(void *f, void *arg)
{
    void (*func)(void *) __noreturn = f;
    if (!CONFIG_RELOCATE_INIT)
        func(arg);

    // Allocate space for init code.
    u32 initsize = SYMBOL(code32init_end) - SYMBOL(code32init_start);
    u32 codealign = SYMBOL(_reloc_min_align);
    void *codedest = memalign_tmp(codealign, initsize);
    void *codesrc = VSYMBOL(code32init_start);
    if (!codedest)
        panic("No space for init relocation.\n");

    // Copy code and update relocs (init absolute, init relative, and runtime)
    dprintf(1, "Relocating init from %p to %p (size %d)\n"
            , codesrc, codedest, initsize);
    s32 delta = codedest - codesrc;
    //olly_printf("0----------reloc_preinit  delta = 0x%x\n", delta);
    memcpy(codedest, codesrc, initsize);
    updateRelocs(codedest, VSYMBOL(_reloc_abs_start), VSYMBOL(_reloc_abs_end)
                 , delta);
    updateRelocs(codedest, VSYMBOL(_reloc_rel_start), VSYMBOL(_reloc_rel_end)
                 , -delta);
    updateRelocs(VSYMBOL(code32flat_start), VSYMBOL(_reloc_init_start)
                 , VSYMBOL(_reloc_init_end), delta);

    //函数位置重新定位了
    if (f >= codesrc && f < VSYMBOL(code32init_end))
        func = f + delta;
    //olly_printf("1----------reloc_preinit f=0x%x func=0x%x\n",f, func);
    // Call function in relocated code.
    barrier();
    func(arg);  //  
}

/*
 * handle_post()
 *  dopost()
 *   code_mutable_preinit()
 */
// Runs after all code is present and prior to any modifications
void
code_mutable_preinit(void)
{
    if (HaveRunPost)
        // Already run
        return;
    // Setup reset-vector entry point (controls legacy reboots).
    rtc_write(CMOS_RESET_CODE, 0);
    barrier();
    HaveRunPost = 1;
    barrier();
}

/*
 * handle_post()
 *  dopost()
 */
// Setup for code relocation and then relocate.
void VISIBLE32INIT
dopost(void)
{
    olly_printf("0----------------in dopost----------------------------\n");
    code_mutable_preinit();
    olly_printf("1----------------in dopost----------------------------\n");
    // Detect ram and setup internal malloc.
    qemu_preinit();
    olly_printf("2----------------in dopost----------------------------\n");
    coreboot_preinit();
    olly_printf("3----------------in dopost----------------------------\n");
    malloc_preinit();
    olly_printf("4----------------in dopost----------------------------\n");
    // Relocate initialization code and call maininit().
    reloc_preinit(maininit, NULL);
    olly_printf("5----------------in dopost----------------------------\n");
}

// Entry point for Power On Self Test (POST) - the BIOS initilization
// phase.  This function makes the memory at 0xc0000-0xfffff
// read/writable and then calls dopost().
void VISIBLE32FLAT
handle_post(void)
{
    olly_printf("%s","0 --------------####--------------handle_post ----------###---------- \n");
    if (!CONFIG_QEMU && !CONFIG_COREBOOT)
        return;

    serial_debug_preinit(); //直接返回了
    olly_printf("%s","1 --------------####--------------handle_post ----------###---------- \n");
    debug_banner(); //只有打印SeaBios版本信息
    olly_printf("%s","2 --------------####--------------handle_post ----------###---------- \n");
    // Check if we are running under Xen.
    xen_preinit(); //检查是否运行在xen vm
    olly_printf("%s","3 --------------####--------------handle_post ----------###---------- \n");
    // Allow writes to modify bios area (0xf0000)
    
    make_bios_writable();
    olly_printf("%s","x --------------####--------------handle_post ----------###---------- \n");
    // Now that memory is read/writable - start post process.
    dopost();
}
