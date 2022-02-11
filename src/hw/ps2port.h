// Basic ps2 port (keyboard/mouse) command handling.
#ifndef __PS2PORT_H
#define __PS2PORT_H

#include "types.h" // u8
/*
 * data port:
 *
 * The Data Port (IO Port 0x60) is used for 
 * reading data that was received from a PS/2 device or from the PS/2 controller itself and 
 * writing data to a PS/2 device or to the PS/2 controller itself. 
 */
#define PORT_PS2_DATA          0x0060
/*
 * read:  status register
 * write: command register 
 * 
 * 1.The Status Register contains various flags that show the state of the PS/2 controller
 * 2.The Command Port (IO Port 0x64) is used for sending commands to the PS/2 Controller (not to PS/2 devices). 
 */
#define PORT_PS2_STATUS        0x0064


/* 下面的这些command，最低一个byte表示命令
 * 比如 0x0120 ，命令为0x20,但是需要receive 0x1字节
 * 比如 0x1060 ，命令为0x60,但是需要write 0x1字节
 */
// Standard commands. 应该是通过0x64端口写
#define I8042_CMD_CTL_RCTR      0x0120 /* 0x20: Read "byte 0" from internal RAM  */
#define I8042_CMD_CTL_WCTR      0x1060 /* 0x60: Write next byte to "byte 0" of internal RAM */
#define I8042_CMD_CTL_TEST      0x01aa /* 0xAA: Test PS/2 Controller  */

#define I8042_CMD_KBD_TEST      0x01ab /* 0xAB: Test first PS/2 port  */
#define I8042_CMD_KBD_DISABLE   0x00ad /* 0xAD: Disable first PS/2 port  */
#define I8042_CMD_KBD_ENABLE    0x00ae /* 0xAE: Enable first PS/2 port  */

#define I8042_CMD_AUX_DISABLE   0x00a7 /* 0xA7: Disable second PS/2 port (only if 2 PS/2 ports supported)  */
#define I8042_CMD_AUX_ENABLE    0x00a8 /* 0xA8: Enable second PS/2 port (only if 2 PS/2 ports supported)  */
#define I8042_CMD_AUX_SEND      0x10d4 /* 0xD4: Write next byte to second PS/2 port input buffer (only if 2 PS/2 ports supported)  */

// Keyboard commands，应该是通过0x64端口写command,然后通过0x60,读写数据
#define ATKBD_CMD_SETLEDS       0x10ed
#define ATKBD_CMD_SSCANSET      0x10f0
#define ATKBD_CMD_GETID         0x02f2
#define ATKBD_CMD_ENABLE        0x00f4
#define ATKBD_CMD_RESET_DIS     0x00f5
#define ATKBD_CMD_RESET_BAT     0x01ff

// Mouse commands，应该是通过0x64端口写command,然后通过0x60,读写数据
#define PSMOUSE_CMD_SETSCALE11  0x00e6
#define PSMOUSE_CMD_SETSCALE21  0x00e7
#define PSMOUSE_CMD_SETRES      0x10e8
#define PSMOUSE_CMD_GETINFO     0x03e9
#define PSMOUSE_CMD_GETID       0x02f2
#define PSMOUSE_CMD_SETRATE     0x10f3
#define PSMOUSE_CMD_ENABLE      0x00f4
#define PSMOUSE_CMD_DISABLE     0x00f5
#define PSMOUSE_CMD_RESET_BAT   0x02ff

// Status register bits.
#define I8042_STR_PARITY        0x80
#define I8042_STR_TIMEOUT       0x40
#define I8042_STR_AUXDATA       0x20
#define I8042_STR_KEYLOCK       0x10
#define I8042_STR_CMDDAT        0x08 /* 从来就没有使用过 */
#define I8042_STR_MUXERR        0x04
#define I8042_STR_IBF           0x02
#define I8042_STR_OBF           0x01

// Control register bits.
#define I8042_CTR_KBDINT        0x01
#define I8042_CTR_AUXINT        0x02
#define I8042_CTR_IGNKEYLOCK    0x08
#define I8042_CTR_KBDDIS        0x10
#define I8042_CTR_AUXDIS        0x20
#define I8042_CTR_XLATE         0x40

// ps2port.c
void i8042_reboot(void);
int ps2_kbd_command(int command, u8 *param);
int ps2_mouse_command(int command, u8 *param);
void ps2_check_event(void);
void ps2port_setup(void);

#endif // ps2port.h
