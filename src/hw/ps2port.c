// Support for handling the PS/2 mouse/keyboard ports.
//
// Copyright (C) 2008  Kevin O'Connor <kevin@koconnor.net>
// Several ideas taken from code Copyright (c) 1999-2004 Vojtech Pavlik
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // GET_LOW
#include "output.h" // dprintf
#include "pic.h" // pic_eoi1
#include "ps2port.h" // ps2_kbd_command
#include "romfile.h" // romfile_loadint
#include "stacks.h" // yield
#include "util.h" // udelay
#include "x86.h" // inb

#include "stack_dbg.h"

/****************************************************************
 * Low level i8042 commands.
 ****************************************************************/

// Timeout value.
#define I8042_CTL_TIMEOUT       10000

#define I8042_BUFFER_SIZE       16

/*
 * guest等待设备 out buffer空闲
 */
static int
i8042_wait_read(void)
{
    dprintf(7, "i8042_wait_read\n");
    int i;
    for (i=0; i<I8042_CTL_TIMEOUT; i++) {
        u8 status = inb(PORT_PS2_STATUS);
        if (status & I8042_STR_OBF) //最低位为1
            return 0;
        udelay(50);
    }
    warn_timeout();
    return -1;
}

/*
 * guest等待设备 input buffer 空闲
 */
static int
i8042_wait_write(void)
{
    dprintf(7, "i8042_wait_write\n");
    int i;
     
    for (i=0; i<I8042_CTL_TIMEOUT; i++) {
        u8 status = inb(PORT_PS2_STATUS);
        //olly_printf("1----------i8042_wait_write status=%x\n", status);
        if (! (status & I8042_STR_IBF)){
            //olly_printf(" -i8042_wait_write status=%x    OK\n", status);
            return 0;
        }
        //olly_printf(" --i8042_wait_write status=%x\n   failed", status);
        udelay(50);
    }
   // olly_printf("3----------i8042_wait_write status=%x\n", status);
    warn_timeout();
    //olly_printf("4----------i8042_wait_write status=%x\n", status);
    return -1;
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     device_hardware_setup()
 *      ps2port_setup()
 *       ps2_keyboard_setup()
 *        i8042_flush()
 * 
 * guest将设备中的数据都读取处俩，丢弃
 */ 
static int
i8042_flush(void)
{
    dprintf(7, "i8042_flush\n");
    int i;
    for (i=0; i<I8042_BUFFER_SIZE /* 16字节 */; i++) {
        olly_printf("0-----------i8042_flush\n");
        u8 status = inb(PORT_PS2_STATUS); //0x0064
        if (! (status & I8042_STR_OBF)) //设备的out buffer，没有数据了
            return 0;

        udelay(50);
        olly_printf("1-----------i8042_flush\n");
        u8 data = inb(PORT_PS2_DATA); //0x0060,读一个字节出来
        dprintf(7, "i8042 flushed %x (status=%x)\n", data, status);
    }

    warn_timeout();
    return -1;
}

/*
 * i8042_command()
 *  __i8042_command()
 * 
 * __ps2_command()
 *  i8042_command()
 *   __i8042_command()
 *   
 * 发送command到 ps2 controller
 * 
 * 1.写命令到0x0064端口
 * 2.写数据到0x0060端口
 * 3.从0x0060端口读数据
 */
static int
__i8042_command(int command, u8 *param)
{
    int receive = (command >> 8) & 0xf; //需要receive(执行in指令)的次数 ,也就是数据的byte数量
    int send = (command >> 12) & 0xf;   //需要send(执行out指令)的次数,也就是数据的byte数量

    // Send the command.
    int ret = i8042_wait_write();
    if (ret)
        return ret;
    
    outb(command, PORT_PS2_STATUS); //0x0064，发送command到0x64端口
    
    // Send parameters (if any).
    int i;
    //如果有数据要发送，就还有发送数据到0x60数据端口
    for (i = 0; i < send; i++) {
        
        ret = i8042_wait_write(); //等待设备的 input buffer 可写
        
        if (ret)
            return ret;
        outb(param[i], PORT_PS2_DATA); //guest写数据到设备的input buffer
    }
    

    //olly_printf("------------------------------------------\n");
    // Receive parameters (if any).
    //发送命令后，如果需要从设备接收数据，从0x60数据端口接收数据
    for (i = 0; i < receive; i++) {
        ret = i8042_wait_read(); //guest 等待设备的output buffer 由数据
        if (ret){
            return ret;
        }
        param[i] = inb(PORT_PS2_DATA);  //guest将设备output buffer中的数据读进来
        olly_printf("\n param[i]=0x %x\n", param[i]);
    }
    return 0;
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     device_hardware_setup()
 *      ps2port_setup()
 *       ps2_keyboard_setup()
 *        i8042_command()
 * 
 * __ps2_command()
 *  i8042_command()
 * 
 * 去执行command， 将param[i]中的数据发送出去，或者结束数据到param[i]
 * 
 * command的低8bits为 ps2 controller的命令，高8bits为send和receive的次数
 */ 
static int
i8042_command(int command, u8 *param)
{
    dprintf(7, "i8042_command cmd=%x\n", command);
    int ret = __i8042_command(command, param);
    if (ret)
        dprintf(2, "i8042 command %x failed\n", command);
    return ret;
}

/*
 * 将c写到ps2 controller的 input buffer
 */
static int
i8042_kbd_write(u8 c)
{
    dprintf(7, "i8042_kbd_write c=%d\n", c);
    int ret = i8042_wait_write();
    if (! ret)
        outb(c, PORT_PS2_DATA);
    return ret;
}

static int
i8042_aux_write(u8 c)
{
    return i8042_command(I8042_CMD_AUX_SEND, &c);
}

void
i8042_reboot(void)
{
    if (! CONFIG_PS2PORT)
       return;
    int i;
    for (i=0; i<10; i++) {
        i8042_wait_write();
        udelay(50);
        //guest给ps2 controller发送 reset命令
        outb(0xfe, PORT_PS2_STATUS); /* pulse reset low */
        udelay(50);
    }
}


/****************************************************************
 * Device commands.
 ****************************************************************/

#define PS2_RET_ACK             0xfa
#define PS2_RET_NAK             0xfe

/*
 *
 * 从ps2 controller读取一个byte
 */
static int
ps2_recvbyte(int aux, int needack, int timeout)
{
    u32 end = timer_calc(timeout);
    for (;;) {
        //先得到ps2 contrller的状态
        u8 status = inb(PORT_PS2_STATUS); //0x0064

        if (status & I8042_STR_OBF) { //0x01,检查ps2 controller的out buffer是否有数据，如果有数据，guest就可以读取过来
            u8 data = inb(PORT_PS2_DATA); //0x0060
            dprintf(7, "ps2 read %x\n", data);

            /*
             * 读出来的status信息必须要与aux相等
             */
            if (!!(status & I8042_STR_AUXDATA) == aux) { //
                if (!needack) //如果不需要needack,就直接返回data了
                    return data;

                //下面是需要olly-vmm的ps2 controller 返回 ack的情况了

                if (data == PS2_RET_ACK) //0xfa
                    return data;

                if (data == PS2_RET_NAK) { //0xfe
                    dprintf(1, "Got ps2 nak (status=%x)\n", status);
                    return data;
                }
            }

            // This data not part of command - just discard it.
            dprintf(1, "Discarding ps2 data %02x (status=%02x)\n", data, status);
        }

        if (timer_check(end)) {
            warn_timeout();
            return -1;
        }
        yield();

        //outb(status, 0x2237);
    }
}

/*
 *
 * guest发送一个byte到设备
 */
static int
ps2_sendbyte(int aux, u8 command, int timeout)
{
    dprintf(7, "ps2_sendbyte aux=%d cmd=%x\n", aux, command);
    int ret;
    if (aux)
        ret = i8042_aux_write(command);
    else
        ret = i8042_kbd_write(command); //这里,将command发送出去

    //outb(ret, 0x8723);
    if (ret)
        return ret;

    // Read ack. 从qemu in一个ACK过来
    ret = ps2_recvbyte(aux, 1, timeout);
    
    if (ret < 0)
        return ret;

    if (ret != PS2_RET_ACK)
        return -1;

    return 0;
}

/*
 * 0x30
 */
u8 Ps2ctr VARLOW = I8042_CTR_KBDDIS | I8042_CTR_AUXDIS;

/*
 * aux:0, keyboard
 * aux:1, mouse
 * 
 * ps2_keyboard_setup()
 *  ps2_kbd_command() 0x01ff  , 0x00f5
 *   ps2_command(aux=0, command =0x01ff, 0x00f5 )
 *    __ps2_command(aux=0, command =0x01ff, 0x00f5 )
 */
static int
__ps2_command(int aux, int command, u8 *param)
{
    int ret2;
    int receive = (command >> 8) & 0xf;//8-11bit 表示要recieve的字节数
    int send = (command >> 12) & 0xf; // 12-15bit表示要send的字节数

    // Disable interrupts and keyboard/mouse.
    u8 ps2ctr = GET_LOW(Ps2ctr);

    /* 0b110000 | ps2ctr*/
    //把mouse和keyboard都禁用了先
    u8 newctr = ((ps2ctr | I8042_CTR_AUXDIS/* bit 5*/ | I8042_CTR_KBDDIS/*bit 4*/)
                 & ~(I8042_CTR_KBDINT/*bit 0*/|I8042_CTR_AUXINT /* bit 1 */));

    olly_printf("i8042 ctr old=%x new=%x\n", ps2ctr, newctr);
    //通过0x64端口的0x60命令 将newctrl out到 qemu的ps2 controller.
    int ret = i8042_command(I8042_CMD_CTL_WCTR /* 0x1060 */, &newctr);//0x60, newctr通过PORT_PS2_DATA 写到olly-vmm
    if (ret) //前一步，必须要返回0
        return ret;

    // Flush any interrupts already pending.
    olly_printf("__ps2_command : 0\n");
    yield();
    olly_printf("__ps2_command : 1\n");
    // Enable port command is being sent to.
    SET_LOW(Ps2ctr, newctr); //control register保存回来
    if (aux) // mouse
        newctr &= ~I8042_CTR_AUXDIS; //启用mouse中断
    else // keyboard
        newctr &= ~I8042_CTR_KBDDIS;//启动key board 中断

    //先通知ps2 controller,取消 mouse或则 keyboard
    ret = i8042_command(I8042_CMD_CTL_WCTR, &newctr); //newctr通过PORT_PS2_DATA 写到olly-vmm

    if (ret)
        goto fail;
 
    olly_printf("__ps2_command : 2\n");
    if ((u8)command == (u8)ATKBD_CMD_RESET_BAT) { //0xff
        // Reset is special wrt timeouts.
 
        // Send command.
 
        ret = ps2_sendbyte(aux, command, 1000); // 通过data port(0x60)发送 command
        //
        if (ret)
            goto fail;

        // Receive parameters.
        ret = ps2_recvbyte(aux, 0, 4000);  // 通过data port(0x60)接收数据
        //outb(ret, 0x7345);
        if (ret < 0)
            goto fail;
        param[0] = ret;
        if (receive > 1) {
            ret = ps2_recvbyte(aux, 0, 500);
            if (ret < 0)
                goto fail;
            param[1] = ret;
        }
    } else if (command == ATKBD_CMD_GETID) { //0xf2
        // Getid is special wrt bytes received.

        // Send command.
        ret = ps2_sendbyte(aux, command, 200);
        if (ret)
            goto fail;

        // Receive parameters.
        ret = ps2_recvbyte(aux, 0, 500);
        if (ret < 0)
            goto fail;
        param[0] = ret;
        if (ret == 0xab || ret == 0xac || ret == 0x2b || ret == 0x5d
            || ret == 0x60 || ret == 0x47) {
            // These ids (keyboards) return two bytes.
            ret = ps2_recvbyte(aux, 0, 500);
            if (ret < 0)
                goto fail;
            param[1] = ret;
        } else {
            param[1] = 0;
        }
    } else {//0x00f4
        // Send command.
        //olly_printf("------------1--------------------------------------ps2\n");
        ret = ps2_sendbyte(aux, command, 200);
        //olly_printf("------2--------------------------------------------ps2\n");
        if (ret)
            goto fail;

        // Send parameters (if any).
        int i;
        for (i = 0; i < send; i++) {
            ret = ps2_sendbyte(aux, param[i], 200);
            if (ret)
                goto fail;
        }
        // Receive parameters (if any).
        for (i = 0; i < receive; i++) {
            ret = ps2_recvbyte(aux, 0, 500);
            if (ret < 0)
                goto fail;
            param[i] = ret;
        }

    }

    ret = 0;

fail:
    // Restore interrupts and keyboard/mouse.
    SET_LOW(Ps2ctr, ps2ctr);
    ret2 = i8042_command(I8042_CMD_CTL_WCTR, &ps2ctr); //0x60 command
    if (ret2)
        return ret2;

    return ret;
}

/*
 * aux:0, keyboard
 * aux:1, mouse
 * 
 * ps2_keyboard_setup()
 *  ps2_kbd_command() 0x01ff  , 0x00f5
 *   ps2_command(aux=0, command =0x01ff,0x00f5 )
 * 
 * mouse_command() [src/mouse.c]
 *  ps2_mouse_command()
 *   ps2_command(aux = 1)
 */
static int
ps2_command(int aux, int command, u8 *param)
{
    dprintf(7, "ps2_command aux=%d cmd=%x\n", aux, command);
    int ret = __ps2_command(aux, command, param);
    if (ret)
        dprintf(2, "ps2 command %x failed (aux=%d)\n", command, aux);
    return ret;
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     device_hardware_setup()
 *      ps2port_setup()
 *       ps2_keyboard_setup()
 *        ps2_kbd_command() 0x01ff,00f5
 */ 
int
ps2_kbd_command(int command, u8 *param)
{

    if (! CONFIG_PS2PORT)
        return -1;
    return ps2_command(0, command, param);
}

/*
 * mouse_command() [src/mouse.c]
 *  ps2_mouse_command()
 */
int
ps2_mouse_command(int command, u8 *param)
{
    if (! CONFIG_PS2PORT)
        return -1;

    // Update ps2ctr for mouse enable/disable.
    if (command == PSMOUSE_CMD_ENABLE || command == PSMOUSE_CMD_DISABLE) {
        u8 ps2ctr = GET_LOW(Ps2ctr);

        if (command == PSMOUSE_CMD_ENABLE) 
            ps2ctr = ((ps2ctr | (CONFIG_HARDWARE_IRQ ? I8042_CTR_AUXINT : 0))
                      & ~I8042_CTR_AUXDIS);
        else
            ps2ctr = (ps2ctr | I8042_CTR_AUXDIS) & ~I8042_CTR_AUXINT;
        SET_LOW(Ps2ctr, ps2ctr);
    }

    return ps2_command(1, command, param);
}


/****************************************************************
 * IRQ handlers
 ****************************************************************/

// INT74h : PS/2 mouse hardware interrupt
void VISIBLE16
handle_74(void)
{
    if (! CONFIG_PS2PORT)
        return;

    debug_isr(DEBUG_ISR_74);

    u8 v = inb(PORT_PS2_STATUS);
    if ((v & (I8042_STR_OBF|I8042_STR_AUXDATA))
        != (I8042_STR_OBF|I8042_STR_AUXDATA)) {
        dprintf(1, "ps2 mouse irq but no mouse data.\n");
        goto done;
    }
    v = inb(PORT_PS2_DATA);

    if (!(GET_LOW(Ps2ctr) & I8042_CTR_AUXINT))
        // Interrupts not enabled.
        goto done;

    process_mouse(v);

done:
    pic_eoi2();
}

// INT09h : Keyboard Hardware Service Entry Point
void VISIBLE16
handle_09(void)
{
    if (! CONFIG_PS2PORT)
        return;

    debug_isr(DEBUG_ISR_09);

    // read key from keyboard controller
    u8 v = inb(PORT_PS2_STATUS);
    if (v & I8042_STR_AUXDATA) {
        dprintf(1, "ps2 keyboard irq but found mouse data?!\n");
        goto done;
    }
    v = inb(PORT_PS2_DATA);

    if (!(GET_LOW(Ps2ctr) & I8042_CTR_KBDINT))
        // Interrupts not enabled.
        goto done;

    process_key(v);

    // Some old programs expect ISR to turn keyboard back on.
    i8042_command(I8042_CMD_KBD_ENABLE, NULL);

done:
    pic_eoi1();
}

// Check for ps2 activity on machines without hardware irqs
void
ps2_check_event(void)
{
    if (! CONFIG_PS2PORT || CONFIG_HARDWARE_IRQ)
        return;
    u8 ps2ctr = GET_LOW(Ps2ctr);
    if ((ps2ctr & (I8042_CTR_KBDDIS|I8042_CTR_AUXDIS))
        == (I8042_CTR_KBDDIS|I8042_CTR_AUXDIS))
        return;
    for (;;) {
        u8 status = inb(PORT_PS2_STATUS);
        if (!(status & I8042_STR_OBF))
            break;
        u8 data = inb(PORT_PS2_DATA);
        if (status & I8042_STR_AUXDATA) {
            if (!(ps2ctr & I8042_CTR_AUXDIS))
                process_mouse(data);
        } else {
            if (!(ps2ctr & I8042_CTR_KBDDIS))
                process_key(data);
        }
    }
}


/****************************************************************
 * Setup
 ****************************************************************/
/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     device_hardware_setup()
 *      ps2port_setup()
 *       ps2_keyboard_setup()
 */ 
static void
ps2_keyboard_setup(void *data)
{
    // flush incoming keys (also verifies port is likely present)
    olly_printf("0--------ps2_keyboard_setup\n");
    int ret = i8042_flush(); //清空qemu设备中 ps2 controller中的垃圾数据
    olly_printf("1--------ps2_keyboard_setup\n");
    if (ret)
        return;

    // Disable keyboard / mouse and drain any input they may have sent
    ret = i8042_command(I8042_CMD_KBD_DISABLE, NULL); // 发送0xAD 到qemu ,disable keyboard
    if (ret)
        return;
    ret = i8042_command(I8042_CMD_AUX_DISABLE, NULL); //发送0xa7到qemu, disable mouse
    if (ret)
        return;
    ret = i8042_flush();  //再次清空 ps2 controller中的垃圾数据
    if (ret)
        return;

    olly_printf("2--------ps2_keyboard_setup\n");
    // Controller self-test.
    u8 param[2];
    ret = i8042_command(I8042_CMD_CTL_TEST, param); //发送 0xaa到qemu,返回0x55到param[0]
    
    if (ret)
        return;
    
    //qemu必须返回0x55
    if (param[0] != 0x55) {
        dprintf(1, "i8042 self test failed (got %x not 0x55)\n", param[0]);
        return;
    }

    olly_printf("3--------ps2_keyboard_setup param[0]=%x, param[1]=0x%x \n", param[0], param[1]);
    // Controller keyboard test.
    ret = i8042_command(I8042_CMD_KBD_TEST, param);//发送0xab到qemu，测试 the first PS/2 port(也就是keyboard用的 ps2 controller 端口)
    
    if (ret)
        return;
        
    //qemu必须返回0x00    
    if (param[0] != 0x00) {
        dprintf(1, "i8042 keyboard test failed (got %x not 0x00)\n", param[0]);
        return;
    }
    /* ------------------- keyboard side ------------------------*/
    /* reset keyboard and self test  (keyboard side) */
    int spinupdelay = romfile_loadint("etc/ps2-keyboard-spinup", 0);
    u32 end = timer_calc(spinupdelay);
    olly_printf("4--------ps2_keyboard_setup  param[0]=%x, param[1]=0x%x \n", param[0], param[1]);

    for (;;) {
        
        ret = ps2_kbd_command(ATKBD_CMD_RESET_BAT, param);//0xff, 应该是reset keyboard,需要接收olly-vmm返回一个0xaa
         
        olly_printf("44--------ps2_keyboard_setup ret=0x%x \n",ret);
        if (!ret) //qemu返回0才会推出循环
            break;

        if (timer_check(end)) {
            if (spinupdelay)
                warn_timeout();
            return;
        }
        yield();
    }
    
    olly_printf("5--------ps2_keyboard_setup\n");
    if (param[0] != 0xaa) {
        dprintf(1, "keyboard self test failed (got %x not 0xaa)\n", param[0]);
        return;
    }

    olly_printf("66666--------ps2_keyboard_setup\n");
    /* Disable keyboard scanning */
    ret = ps2_kbd_command(ATKBD_CMD_RESET_DIS, NULL);//0xf5
    
    olly_printf("--------66666--------ps2_keyboard_setup\n");
    if (ret)
        return;

    olly_printf("777777--------ps2_keyboard_setup\n");
    // Set scancode command (mode 2)
    param[0] = 0x02; //这个参数发送到qmeu
    ret = ps2_kbd_command(ATKBD_CMD_SSCANSET, param);//0xf0
    
    olly_printf("--------777777--------ps2_keyboard_setup\n");
    if (ret)
        return;

    // Keyboard Mode: disable mouse, scan code convert, enable kbd IRQ
    Ps2ctr = (I8042_CTR_AUXDIS | I8042_CTR_XLATE
              | (CONFIG_HARDWARE_IRQ ? I8042_CTR_KBDINT : 0));

    /* Enable keyboard */
    ret = ps2_kbd_command(ATKBD_CMD_ENABLE, NULL);//0xf4
    if (ret)
        return;

    dprintf(1, "PS2 keyboard initialized\n");
}

/*
 * handle_post()
 *  dopost()
 *   reloc_preinit(f==maininit)
 *    maininit()
 *     device_hardware_setup()
 *      ps2port_setup()
 */ 
void
ps2port_setup(void)
{
    ASSERT32FLAT();
    if (! CONFIG_PS2PORT)
        return;
    olly_printf("0------------ps2port_setup\n");
    //确定ps2 controller是否存在
    if (acpi_dsdt_present_eisaid(0x0303) == 0) {
        dprintf(1, "ACPI: no PS/2 keyboard present\n");
        return;
    }
    olly_printf("1------------ps2port_setup\n");
    dprintf(3, "init ps2port\n");

    enable_hwirq(1, FUNC16(entry_09)); //键盘中断处理函数
    olly_printf("2------------ps2port_setup\n");
    enable_hwirq(12, FUNC16(entry_74));  //鼠标中断处理函数
    olly_printf("3------------ps2port_setup\n");

    //键盘setup工作
    run_thread(ps2_keyboard_setup, NULL);
    
    olly_printf("4------------ps2port_setup\n");
}
