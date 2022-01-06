#include "stack_dbg.h"
#include "output.h"
#include "string.h"

#define STACK_SIZE 64
int idx = 0;//第idx为NULL
struct stack_entry stack[STACK_SIZE];

void push_stack_entry(u32 line, const char* function_name) {
    struct stack_entry entry={.line= line, function_name=function_name};
    stack[idx] = entry;
    idx++;
    if(idx>=STACK_SIZE) {
        olly_printf("push_stack_entry  stack index overflow , idx=%x\n", idx);
    }
}

void pop_stack_entry(int cnt){
    if((idx-cnt) >=0) {
        idx = idx - cnt;
    } else{
        olly_printf("can't pop so much stack entry, stack idx=0x%x ,pop count=0x%x \n", idx, cnt);
    }
}

void printf_stack(void) {
    char sp=' ';
    
    printf("STACK:\n");
    for(int i=0; i <idx; i++) {
        for(int j=0; j<i;j++){
            olly_printf("%c",sp);
        }
        printf("%s:%d\n", stack[i].function_name, stack[i].line);
    }
}
