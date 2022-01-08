#ifndef __OLLY_STACK_DBG_H_
#define __OLLY_STACK_DBG_H_
#include "types.h"

struct stack_func_dbg_point{
    u32 line;
    const char* info;
};

#define STACK_FUNC_ENTRY_SZ     28
struct stack_func{
    const char* function_name;
    u8  top ; //这个是没有被填写的索引
    struct stack_func_dbg_point points[STACK_FUNC_ENTRY_SZ];
};

#define STACK_FN_SIZE 64

void push_new_function(const char* func_name);
void pop_new_function(const char* where_fn, u32 where_ln);
void push_point(const char* fn, u32 line,const char* info);
void print_dbg_stack(void);

#define PUSH_DBG_FN()           push_new_function(__func__)
#define PUSH_DBG_POINT(info)    push_point(__func__, __LINE__, info)
#define SHOW_STACK()            print_dbg_stack()

#endif
