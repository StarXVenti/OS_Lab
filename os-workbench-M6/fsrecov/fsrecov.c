#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fat32.h"

char *copy;
char filename[80], sha[100];

//创建一个新文件，并将从镜像中提取的BMP文件数据写入该文件。使用sha1sum工具计算文件的SHA1校验和，并打印结果。
void print_sha1sum(int size, int start){
  int fd=open(filename, O_CREAT|O_TRUNC|O_RDWR,S_IRWXU);
  write(fd, copy+start, size);
  close(fd);
  memset(sha,0,sizeof(sha));
  //printf("%s\n",filename);
  strcpy(sha, "sha1sum ");
  strcat(sha, filename);
  system(sha);
}

//根据FAT32的长文件名（LFN）格式恢复文件名。如果短文件名（SFN）存在且有效，直接返回。否则，恢复长文件名，包括对长文件名的分段处理，并检查文件名格式是否正确。
int findname(int x){
  int temp=x;
  int flag=1;
  //检查指定位置的8字节数据是否包含'~'字符。如果包含，返回0，表示该位置是短文件名的标记。
  for(int i=x;i<x+8;i++){
    if(copy[i]=='~')
      flag=0;
  }
  if(flag){
    memset(filename, 0, sizeof(filename));
    int i=0;
    while(copy[temp]!=0x20){
      filename[i]=copy[temp];
      temp++;
      i++;
    }
    filename[i]='\0';
    strcat(filename,".bmp");
    return 1;
  }
  int tempp=x-32;
  int cnt=1;
  while(1){
    if(copy[tempp]==(char)cnt){
      tempp-=32;
      cnt++;
    }
    else if(copy[tempp]==((char)cnt|(char)0x40)){   //结束
      break;
    }
    else return 0;    //存在错误
  }
  for(int i=28;i<32;i++){
    if(copy[tempp+i]!=(char)0xff)
      return 0;
  }
  memset(filename, 0, sizeof(filename));    //修复长名字
  int finish=0;
  int i=0,j=0;
  for(j=x-32;j>=tempp;j-=32){
    temp=j+0x1;
    for(int k=0;k<5;k++){    //part1
      filename[i]=copy[temp];
      if(copy[temp]==0x0&&copy[temp+1]==0x0){
        finish=1;
        break;
      }
      i++, temp+=2;
    }
    if(finish)
      break;
    
    temp=j+0xe;
    for(int k=0;k<6;k++){    //part2
      filename[i]=copy[temp];
      if(copy[temp]==0x0&&copy[temp+1]==0x0){
        finish=1;
        break;
      }
      i++, temp+=2;
    }
    if(finish)
      break;

    temp=j+0x1c;
    for(int k=0;k<2;k++){    //part3
      filename[i]=copy[temp];
      if(copy[temp]==0x0&&copy[temp+1]==0x0){
        finish=1;
        break;
      }
      i++, temp+=2;
    }
    if(finish)
      break;    
  }
  if(j!=tempp)
    return 0;
  else
    return 1;
}

int main(int argc, char *argv[]) {
  //打开磁盘映像文件
  int fd = open(argv[1], O_RDONLY);//只读
  if(fd==-1)
    printf("打开失败\n");
  
  //内存映射，将文件映射到内存中，映射大小为 512MB
  copy=(char *)mmap(NULL, 2<<29, PROT_READ, MAP_SHARED, fd, 0); //512mb，copy为起始位置
  close(fd);

  //从映射的内存区域中读取 FAT32 文件系统的参数
  short sector_bit=*(short *)&copy[0xb];     //每个扇区的字节数
  int cluster_sector_num=(int)copy[0xd];     //每个簇的扇区数
  int sector_num = *(int *)&copy[0x20];      //总扇区数
  
  int end = sector_bit * sector_num;         //磁盘映像的总大小

  int cluster_start=*(int *)&copy[0x2c];     //簇区域的起始扇区
  int fat_start = *(short *)&copy[0xe];      //FAT 区域的起始扇区
  int fat_num=*(int *)&copy[0x10];           //FAT 数量
  int fat_sector_num=*(int *)&copy[0x24];    //每个 FAT 的扇区数

  int start=(fat_start+fat_num*fat_sector_num+(cluster_start-2)*cluster_sector_num)*sector_bit;//数据区的起始扇区

  /*printf("sector_bit=%d\n",sector_bit);
  printf("cluster_sector_num=%d\n",cluster_sector_num);
  printf("sector_num=%d\n",sector_num);
  printf("end=%08x\n", end);
  printf("cluster_start=%d\n", cluster_start);
  printf("start=%d\n", start);*/

  //遍历数据区，寻找"BM"开头的BMP 文件
  for(int i=start;i<end;i++){
    if(copy[i]=='B'&&copy[i+1]=='M'&&copy[i+2]=='P'){
      int base=i-8;
      if(copy[base]==0xe5)    //被删除
        continue;

      int high_c=*(unsigned short *)&copy[base+0x14];
      int short_c=*(unsigned short *)&copy[base+0x1a]; 
      int file_cluster= (high_c<<16)+short_c;

      if(file_cluster<cluster_start)      //簇错误
        continue;

      if(!findname(base))
        continue;
      
      int file_address = start+(file_cluster-cluster_start)*cluster_sector_num*sector_bit;
      int file_size=*(int *)&copy[base+0x1c];

      print_sha1sum(file_size, file_address);       //计算校验和
    }
  }

  return 0;
}

