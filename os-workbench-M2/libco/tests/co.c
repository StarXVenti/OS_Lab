#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdbool.h>
#include <time.h>
#define MAX_CO 100
typedef unsigned char uint8_t;
uint8_t *__stack;
void *__stack_backup;

//根据处理器框架定义宏
#if defined(__i386__)
  #define SP "%%esp"
#elif defined(__x86_64__)
  #define SP "%%rsp"
#endif

struct co {
    bool st;                          //协程状态
    int num;                          //编号
    jmp_buf buf;                      //保存和恢复协程上下文的缓冲区
    uint8_t stack[4096];
    void *stack_backup;
    void (*cu_func)(void *) ;
    void *cu_arg;                     //协程函数参数
    
}__attribute__((aligned(16)));
struct co coroutines[MAX_CO];
struct co *current;

                          
int cunt=-1;

//在main函数前运行,初始化
void function() __attribute__((constructor));
void function(){
    cunt=-1;
    
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {

     ++cunt;
  coroutines[cunt].num = cunt;
  coroutines[cunt].st = true;
  coroutines[cunt].cu_func = func;
  coroutines[cunt].cu_arg = arg;
  int val = setjmp(coroutines[cunt].buf);//    //if we don't use it, will never return to this function.
  if(!val) {
    __stack = coroutines[cunt].stack + sizeof(coroutines[cunt].stack); 
    asm volatile("mov " SP ", %0; mov %1, " SP :                  //汇编指令
                     "=g"(__stack_backup) :
                     "g"(__stack));  
    coroutines[cunt].stack_backup = __stack_backup;
    
    current = &coroutines[cunt];
    //printf("%d\n",cunt);
    /* char * t=(char *)cu_arg;
    printf("%s\n", t);*/
    //coroutines[cunt].cu_func(coroutines[cunt].cu_arg);
     //printf("这里\n");  //printf this sentence * cunt
    current->st = 0;        //current may change
    int temp = current->num;
    //printf("%d\n",temp);
    
    current = &coroutines[0];
    //longjmp(current->buf, 1);  
    __stack_backup = coroutines[temp].stack_backup;
    asm volatile("mov %0," SP : : "g"(__stack_backup)); 
	//printf("change esp\n");    
    longjmp(current->buf, 1);  
    
  }
  //else
  //  printf("return from co_yield\n");
  return &coroutines[cunt];
}

void co_wait(struct co *co) {
//printf("%d\n",cunt);
    setjmp(current->buf);
    if(co->st) {               //still need to execute
        int next = rand() % cunt + 1;
        while(next == current->num || !coroutines[next].st) {
           next = rand() % cunt + 1;
        }
        current = &coroutines[next];
        longjmp(current->buf, 1);        
    }
//printf("wait\n");
}

void co_yield() {
 //printf("change\n");
  int val = setjmp(current->buf);
  if (val == 0) {
    int next = rand() % (cunt + 1);
    while(next == current->num || !coroutines[next].st) {
        next = rand() % (cunt + 1);
    }
    //printf("%d\n", next);
    current = &coroutines[next];
    longjmp(current->buf, 1);
  } else {
  //printf("nmsl\n");       //now start this coroutines;
    return;
  }
}
