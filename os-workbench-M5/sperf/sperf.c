#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

struct Node{
  char name[80];         //函数名
  double time;           //累计时间
}func[3000];

int fildes[2];           //存储管道文件描述符
char mem[10000];         //存储从strace中提取的数据
char name[80];           //临时变量，保存函数名
int cunt;                //记录函数数量
double total;            //函数调用总时间


void analysis(char *str){
  int len=strlen(str)-1;
  int i=0;
  int cunt0=0;        //temp长度
  char temp[80];      //存数字字符
  double res1=0,res2=0;
  //去掉无需分析的数据
  if(str[len-1]!='>' || str[0]=='+'){
    return;
  }
  
  //提取函数名
  for(;str[i]!='(';i++)
    name[i]=str[i];
  name[i]='\0';

  //提取时间信息
  i = len;
  while(i){
    if('0'<=str[i] && str[i]<='9')
      break;
    i--;
  }

  //提取时间的小数部分
  //提取小数部分字符
  for(;str[i]!='.';i--,cunt0++){
    temp[cunt0]=str[i];
  }

  //转成小数数值
  for(int j=0;j<cunt0;j++){
    res1+=temp[j]-'0';
    res1/=10.0;
  }

  //提取整数部分
  cunt0=0;
  i--;
  //整数部分转换成字符
  for(;str[i]!='<';i--,cunt0++){
    temp[cunt0]=str[i];
  }
  //转换成整数
  for(int j=cunt0-1;j>=0;j--){
    res2=res2*10.0+temp[j]-'0';
  }
  //printf("%lf\n",res1+res2);
  
  //计算时间，检查同一个函数
  total+=(res1+res2);
  for(int k=0;k<cunt;k++){
    if(strcmp(name,func[k].name)==0){
      func[k].time+=(res1+res2);
      return;
    }
  }
  strcpy(func[cunt].name,name);
  func[cunt++].time=res1+res2;
}

void print(){
	printf("\033[2J"); //清屏 
	printf("\033[0;0H");//设置光标位置
  for(int i=0; i < cunt; i++){
    printf("name:%-16s\ttime:%.6lf\t%.2lf%%\n", func[i].name,func[i].time,func[i].time*100.0/total);
  }
}

int main(int argc, char *argv[]) {
  cunt=0;
  total=0;
  //管道创建
  if(pipe(fildes)!=0){
    printf("error!\n");
    return -1;
  }

  //创建子进程
  int pid = fork();
  if(pid==0){//子进程
    char **my_arg = (char **)malloc((argc+2)*sizeof(char *));
    my_arg[0]="strace";
    my_arg[1]="-T";
    for(int i = 1; i < argc; ++i){
	  my_arg[i + 1] = argv[i];
    } 
    my_arg[argc+1]=NULL;
     /*for(int i = 0; i <= argc; ++i){
	  printf("%s ",my_arg[i]);
    } */
    //printf("son\n");
    close(fildes[0]);
    dup2(fildes[1],STDERR_FILENO);
    int fd = open("/dev/null", O_WRONLY);
    dup2(fd, STDOUT_FILENO);
    execvp("strace", my_arg);//执行 strace 命令，传递命令参数
    printf("Here\n");
  }
  else{//父进程
    //printf("father\n");
    close(fildes[1]);
    dup2(fildes[0], STDIN_FILENO);
    while(fgets(mem, 10000, stdin)){
      printf("%s",mem);
      analysis(mem); 
      print();
      usleep(40000);      //to print
      /*if(i%10==0)
        system("reset");*/
    }
  }

  return 0;
}

