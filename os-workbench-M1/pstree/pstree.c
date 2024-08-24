#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#define SPACE   "                                                     "

int find = 0;
int main(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        assert(argv[i]); // C 标准保证
    }
    assert(!argv[argc]); // C 标准保证
    if (argv[1] != NULL) {
        char* temp = argv[1];
        char* p = "-p";
        char* n = "-n";
        char* v = "-v";
        if (!(strcmp(temp, p)))
            showPid(0);
        else if (!strcmp(temp, n)) {
            if (argv[2] == NULL) {
                printf("请输入pid！\n");
            }
            else {
                const char* t = argv[2];
                int PID=0;
                for (; *t; t++) {
                    if (!isdigit(*t)) {
                        printf("请输入正确的pid!\n");
                        return 0;
                    }
                    PID = PID * 10 + *t - '0';
                }
                showPid(PID);
            }
        }
        else if (!strcmp(temp, v))
            showVersion();
        else
            printf("请输入正确命令！\n");
    }
    else
        printf("请输入命令！\n");
    return 0;
}

int is_pid_folder(const struct dirent* entry, int* next_pid)
{
    const char* p;
    *next_pid = 0;
    for (p = entry->d_name; *p; p++) {
        if (!isdigit(*p))
            return 0;
        *next_pid = *next_pid * 10 + *p - '0';
    }

    return 1;
}

void showPid(int searchPID)//0为全输出，其它为进程号
{
    if (searchPID == 0 || searchPID == 1)
        printf("systemd(1)");
    else if (searchPID < 0) {
        printf("请输入正确的PID！\n");
        return;
    }
    DIR* procdir;
    FILE* fp;
    struct dirent* entry;
    char path[256 + 5 + 5]; // d_name + /proc + /stat
    char children_path[256 + 5 + 5];
    int pid;
    int next_pid = 0;
    unsigned long maj_faults;
    char parent_length[256];
    int flag = 0;


    // 打开 /proc 文件夹.
    procdir = opendir("/proc");
    if (!procdir) {
        perror("opendir failed");
        return 1;
    }

    // 遍历/proc中全部文件及文件夹
    while ((entry = readdir(procdir))) {
        // 跳过非PID文件夹.
        if (!is_pid_folder(entry, &next_pid))
            continue;

        //尝试打开子文件夹的stat /proc/<PID>/stat.
        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        fp = fopen(path, "r");

        if (!fp) {
            perror(path);
            continue;
        }

        // 获取PID 进程名 faults数量
        fscanf(fp, "%d %s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %lu",
            &pid, &path, &maj_faults
        );

        // 输出
        if (pid == 1) {
            fclose(fp);
            continue;
        }

        if (searchPID == 0 || searchPID == 1) {
            printf("+-%s(%d)", path, pid);
            fclose(fp);
            if (searchPID == 0) {
                strcpy(parent_length, "");
                strcpy(parent_length, "          | ");
                strncat(parent_length, SPACE, strlen(path));
                char ch[15];
                snprintf(ch, sizeof(ch), "(%d)", pid);
                strncat(parent_length, SPACE, strlen(ch));
                snprintf(children_path, sizeof(children_path), "/proc/%d/task", pid);
                findDirectChildren(pid, parent_length, children_path, searchPID);
            }
            printf("\n          ");
        }
        else if (searchPID > 1) {
            if (searchPID == pid) {
                find++;
                flag = 1;
                printf("结果如下：\n");
                printf("%s(%d)", path, pid);
                strcpy(parent_length, "");
                strncat(parent_length, SPACE, strlen(path));
                char ch[15];
                snprintf(ch, sizeof(ch), "(%d)", pid);
                strncat(parent_length, SPACE, strlen(ch));
                snprintf(children_path, sizeof(children_path), "/proc/%d/task", pid);
                fclose(fp);
                findDirectChildren(pid, parent_length, children_path, searchPID);
                printf("\n");
            }
            else {
                snprintf(children_path, sizeof(children_path), "/proc/%d/task", pid);
                findDirectChildren(pid, parent_length, children_path, searchPID);
            }
        }
    }
    closedir(procdir);
    if (find == 0 && searchPID > 1)
        printf("不存在进程号为%d的进程！\n", searchPID);
}
/**/
void findDirectChildren(int PID, char parent_length[], char fatherpath[], int searchPID)
{
    DIR* procdir1;
    FILE* fp1;
    struct dirent* entry;
    char path[256 + 5 + 5];
    int child_pid;
    char pname[100]; // 进程名
    unsigned long maj_faults;
    int count = 0;

    // 构建路径，打开/proc/<PID>/task目录
    procdir1 = opendir(fatherpath);

    if (!procdir1) {
        perror("opendir failed");
        return;
    }

    while ((entry = readdir(procdir1))) {
        if (is_pid_folder(entry, &child_pid) && child_pid != PID) {
            // 打开子进程的stat文件
            snprintf(path, sizeof(path), "%s/%s/stat", fatherpath, entry->d_name);
            //printf("%s",path);
            fp1 = fopen(path, "r");

            if (!fp1) {
                perror(path);
                continue;
            }

            // 读取子进程的信息
            fscanf(fp1, "%d (%[^)]) %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %lu",
                &child_pid, &pname, &maj_faults);

            // 在输出时考虑父进程的长度，做对齐
            if (searchPID == 0) {
                if (count == 0) {
                    count++;
                }
                else
                    printf("\n%s", parent_length);
                printf("+-%s(%d)", pname, child_pid);
                char children_length[256];
                strcpy(children_length, parent_length);
                strcat(children_length, "|");
                strncat(children_length, SPACE, strlen(pname));
                char ch[15];
                snprintf(ch, sizeof(ch), " (%d)", child_pid);
                strncat(children_length, SPACE, strlen(ch));
                snprintf(path, sizeof(path), "%s/%s/task", fatherpath, entry->d_name);
                //findDirectChildren(child_pid,children_length,path); 
            }
            else if (searchPID > 1 && searchPID == PID) {
                if (count == 0) {
                    count++;
                }
                else
                    printf("\n%s", parent_length);
                printf("+-%s(%d)", pname, child_pid);
            }
            else {
                if (searchPID == child_pid) {
                    find++;
                    printf("%s(%d)该进程无子进程！", pname, child_pid);
                    fclose(fp1);
                    closedir(procdir1);
                    return;
                }
            }
            fclose(fp1);
        }
    }
    closedir(procdir1);
}


void showVersion()
{
    printf("simple pstree version:1.0.\n");
}
