#include "stack_dbg.h"
#include "output.h"
#include "string.h"

u8 curr_stack_idx = 0;
u8 stack_has_element = 0;
struct stack_func dbg_stack[STACK_FN_SIZE];

void push_new_function(const char* func_name){
    if(curr_stack_idx + 1 >= STACK_FN_SIZE){
        olly_printf("dbg stack function overflow :function name:%s\n", func_name);
        return;
    }
    stack_has_element = 1;
    curr_stack_idx += 1;
    dbg_stack[curr_stack_idx].top = 0;
}

void pop_new_function(const char* where_fn, u32 where_ln){
    if(curr_stack_idx ==0 && stack_has_element == 1){ //最后一个元素了
        stack_has_element = 0;
        olly_printf( "%s :%d    ,pop the the last function dbg stack has no element \n",  where_fn, where_ln);
        return;
    } else if(curr_stack_idx ==0 && stack_has_element==0) {//stack中没有元素了
        olly_printf("%s :%d    , dbg stack function underflow  \n", where_fn, where_ln);
        return;
    }

    if(curr_stack_idx - 1 < 0){
        
        return;
    }
    curr_stack_idx -= 1;    
    if(curr_stack_idx == 0) {
        
    }
}
void push_point(const char* fn,u32 ln, const char* info){

    if(stack_has_element ==0   ) {
        olly_printf("%s :%d    ,push_point , there is no element in dbg stack\n",  fn, ln);
        return;
    }

    if(strcmp(fn, dbg_stack[curr_stack_idx].function_name) == 0) {
        olly_printf("push_point , current function: %s is not the top element of the dbg stack\n", fn);
        return;
    }

    u8 idx = dbg_stack[curr_stack_idx].top;
    struct stack_func_dbg_point dbg_point ;
    dbg_point.line = ln;
    dbg_point.info = info;

    dbg_stack[curr_stack_idx].points[idx] =  dbg_point;
    dbg_stack[curr_stack_idx].top += 1;      
}

void print_sp(u8 sp){
    for(int i=0;i<sp;i++){
        olly_printf(" ");
    }
}

void print_dbg_stack_points(u8 space, u8 idx){
    struct stack_func* cur=&(dbg_stack[idx]);
    print_sp(space);
    olly_printf("%s\n", cur->function_name);
    for(u8 i=0; i <=cur->top;i++){
        print_sp(space+2);
        olly_printf("  line: %d , %s", cur->points[i].line, cur->points[i].info);
        space+=2;
    }
}

void print_dbg_stack(void){
    u32 sp = 0;
    for(int i=0; i <=curr_stack_idx; i++) {
       print_dbg_stack_points(sp, i);     
       sp+=2;
       olly_printf("\n");
    }
}




