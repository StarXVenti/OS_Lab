#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LINE 4096
#define TEMP_FILE_TEMPLATE "/tmp/temp-XXXXXX"
char libc_name[] = "./tmp/lib.c";
char libso_name[] = "./tmp/lib.so";
int wrapper_num = 0;
int (*wrapper)();
char *gcc_option[] = {
    "gcc",
    "-shared",
    "-fPIC",
    libc_name,
    "-o",
    libso_name,
    NULL,
};

char *wrapper_func[] = {
    "int ",
    "wrapper_",
    "() { return ",
    "; }\n",
    NULL,
};

static void compile_and_load(const char *line) {
    FILE *f1 = fopen(libc_name, "a");
    fprintf(f1, "\n%s\n", line);
    fclose(f1);
    printf("Got %zu chars. Loading...\n", strlen(line)); // 输入内容
}

static void evaluate_expression(const char *line) {
    // 这是一个表达式.需要构造一个wrapper， wrapper_0  wrapper_1
    char wrapper_buf[1024];

    FILE *f2 = fopen(libc_name, "a");

    if (f2 == NULL)
    {
        perror("libc can not open");
    }
    size_t line_length = strlen(line);
    // 如果 line 非空且最后一个字符是换行符，则将长度减一
    if (line_length > 0 && line[line_length - 1] == '\n')
    {
      line_length--;
    }

    fprintf(f2, "%s%s%d%s%.*s%s", wrapper_func[0], wrapper_func[1], wrapper_num, wrapper_func[2],
            (int)line_length, line, wrapper_func[3]);
    fflush(f2);

    if (fork() == 0)
    {
        execvp("gcc", gcc_option);
        perror("gcc error");
        exit(-1);
    }
    wait(NULL);

    void *libHandle = dlopen(libso_name, RTLD_LAZY);
    if (!libHandle)
    {
      printf("handle error\n");
      return;
    }

    sprintf(wrapper_buf, "%s%d", "wrapper_", wrapper_num);
    //从共享库中获取 wrapper_buf 对应的函数地址，并将其赋值给 wrapper
    wrapper = (int (*)())dlsym(libHandle, wrapper_buf);
    if (!wrapper)
    {
      printf("wrapper error\n");
    }

    int res = wrapper();
    dlclose(libHandle);
    printf("(%s)==%d\n", line,res);
    wrapper_num++;
    fclose(f2);
}

int main(int argc, char *argv[]) {
    static char line[MAX_LINE];
    remove(libc_name);
    remove(libso_name);
    FILE *file = fopen(libc_name, "w");
    while (1) {
        printf("crepl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // 处理行末尾的回车
        line[strcspn(line, "\n")] = 0;

        if (strncmp(line, "int ", 4) == 0 && strstr(line, "int ") == line) {
            //printf("1\n");
            // 是个函数申明
            // int func_name(int a, ...) {....; return x;}
            compile_and_load(line);
        } 
        else {
            //printf("0\n");
            // 表达式
            evaluate_expression(line);
        }
    }

    return 0;
}

