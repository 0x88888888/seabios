#ifndef __E820MAP_H
#define __E820MAP_H

#include "types.h" // u64

#define E820_RAM          1  /* 映射到dram的物理地址 */
#define E820_RESERVED     2  /* 比如pci设备的配置空间，用mmio的方式访问，这段物理地址没有映射到物理dram的 */
#define E820_ACPI         3
#define E820_NVS          4
#define E820_UNUSABLE     5

struct e820entry {
    u64 start;
    u64 size;
    u32 type;
};

void e820_add(u64 start, u64 size, u32 type);
void e820_remove(u64 start, u64 size);
void e820_prepboot(void);

// e820 map storage
extern struct e820entry e820_list[];
extern int e820_count;

#endif // e820map.h
