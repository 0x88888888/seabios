#ifndef __OLLY_STACK_DBG_H_
#define __OLLY_STACK_DBG_H_
#include "types.h"

struct stack_entry{
    u32 line;
    const char* function_name;
};

void push_stack_entry(u32 line, const char* function_name);
void pop_stack_entry(int cnt);
void printf_stack(void);

#define push_stack_entry_2() push_stack_entry(__LINE__, __func__)


#endif