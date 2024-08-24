#include "co.h"
#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define K 1024
#define STACK_SIZE (64 * K)

static void stack_switch_call(void *sp, void *entry, void *arg)//切换到一个新的栈，调用指定的函数
{
  asm volatile(//内嵌汇编,编译器不要优化代码,后面的指令 保留原样
#if __x86_64__
      "movq %%rax, 0(%0); movq %%rdi, 8(%0); movq %%rsi, 16(%0); movq %%rdx, 24(%0); movq %%rcx, 32(%0);movq %0, %%rsp; movq %2, %%rdi; call *%1"
      :
      : "b"((uintptr_t)sp - 48), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#else
      "movl %%eax, 0(%0); movl %%edi, 4(%0); movl %%esi, 8(%0); movl %%edx, 12(%0); movl %%ecx, 16(%0); movl %0, %%esp; movl %2, 0(%0); call *%1"
      :
      : "b"((uintptr_t)sp - 24), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#endif
  );
}

static void restore_return()//从当前栈中恢复寄存器值
{
  asm volatile(
#if __x86_64__
      "movq 0(%%rsp), %%rax; movq 8(%%rsp), %%rdi; movq 16(%%rsp), %%rsi; movq 24(%%rsp), %%rdx; movq 32(%%rsp), %%rcx"
      :
      :
#else
      "movl 0(%%esp), %%eax; movl 4(%%esp), %%edi; movl 8(%%esp), %%esi; movl 12(%%esp), %%edx; movl 16(%%esp), %%ecx"
      :
      :
#endif
  );
}

enum co_status               //表示协程状态，分别为1,2,3,4
{
  CO_NEW = 1,                //新建
  CO_RUNNING,                //执行或执行后被切换
  CO_WAITING,                //在co_wait()
  CO_DEAD,                   //已结束但未释放内存
};

struct co
{
  char *name;
  void (*func)(void *);      // co_start 指定的入口地址和参数
  void *arg;

  enum co_status status;     //协程状态
  struct co *waiter;         //是否有其它等待者
  jmp_buf context;           //寄存器
  uint8_t stack[STACK_SIZE]; //堆栈
};

struct co *current;          //正在运行的携程

typedef struct Co_node
{
  struct co *coroutine;
  struct Co_node *prev;
  struct Co_node *next;
} Co_node;

static Co_node *co_node = NULL;

void insert_node(struct co *new_co)
{
  Co_node *node = (struct Co_node *)malloc(sizeof(struct Co_node));
  node->coroutine = new_co;
  if (co_node == NULL)
  {
    node->prev = node->next = node;
    co_node = node;
  }
  else
  {
    node->prev = co_node->prev;
    node->next = co_node;
    node->prev->next = node->next->prev = node;
  }
}

 Co_node *remove_node()
{
  Co_node *victim = NULL;

  if (co_node == NULL)
  {
    return NULL;
  }
  else if (co_node->next == co_node)
  {
    victim = co_node;
    co_node = NULL;
  }
  else
  {
    victim = co_node;

    co_node = co_node->next;
    co_node->prev = victim->prev;
    co_node->prev->next = co_node;
  }

  return victim;
}

struct co *co_start(const char *name, void (*func)(void *), void *arg)
{
  // printf("start\n");
  struct co *new_co = (struct co *)malloc(sizeof(struct co));
  new_co->arg = arg;
  new_co->func = func;
  new_co->name = (char *)name;
  new_co->status = CO_NEW;
  new_co->waiter = NULL;
  insert_node(new_co);

  return new_co;
}

void co_wait(struct co *co)
{

  if (co->status != CO_DEAD)
  {
    co->waiter = current;
    //使携程进入等待状态，不再被调度
    current->status = CO_WAITING;
    co_yield ();
  }

  while (co_node->coroutine != co)
  {
    co_node = co_node->next;
  }

  assert(co_node->coroutine == co);

  free(co);
  free(remove_node());
}

#define JMP_RET 1

void co_yield ()
{
  int val = setjmp(current->context);
  if (val == 0)
  {
    //printf("val=%d\n",val);
    co_node = co_node->next;
    while (co_node->coroutine->status != CO_RUNNING && co_node->coroutine->status != CO_NEW)
    {
      co_node = co_node->next;
    }

    assert(co_node->coroutine->status == CO_RUNNING || co_node->coroutine->status == CO_NEW);
    current = co_node->coroutine;

    if (co_node->coroutine->status == CO_RUNNING)
    {
      longjmp(co_node->coroutine->context, JMP_RET);
    }
    else if (co_node->coroutine->status == CO_NEW)
    {
      //调用函数
      ((struct co volatile *)current)->status = CO_RUNNING;
      
      stack_switch_call(current->stack + STACK_SIZE, current->func, current->arg);

      restore_return();
      //返回后
      ((struct co volatile *)current)->status = CO_DEAD;

      
      if (current->waiter != NULL)
      {

        current->waiter->status = CO_RUNNING;
      }

      co_yield ();
    }
  }
  else
  {
     //printf("val=%d\n",val);
    //继续执行
    assert(val == JMP_RET && current->status == CO_RUNNING);
    return;
  }
}

 __attribute__((constructor)) void co_constructor(void)//初始化函数，在main（） 前执行
{

  current = co_start("main", NULL, NULL);
  current->status = CO_RUNNING;
}

 __attribute__((destructor)) void co_deconstruct(void)//main结束前调用，进行内存释放
{
  while (co_node != NULL)
  {
    free(co_node->coroutine);
    free(remove_node());
  }
}
