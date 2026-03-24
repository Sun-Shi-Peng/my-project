//学习不定参宏函数的使用
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#define LOG(fmt,...) printf("[%s:%d]" fmt,__FILE__,__LINE__,##__VA_ARGS__)

//C语言不定参函数使用，不定参数据访问
void printNum(int count, ...)
{
    va_list ap;
    va_start(ap,count); //获取哪一个参数后面不定参的第一个地址
    for(int i=0;i<count;i++)
    {
        int num=va_arg(ap,int); //获取数据，需要指定参数类型
        printf("param[%d]:%d\n",i,num);
    }
    va_end(ap);  //讲ap指针置空
}

void myprintf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap,fmt);
    char *res;
    int ret=vasprintf(&res,fmt,ap);
    if(ret!=-1)
    {
        printf("%s", res);
        free(res);
    }
    va_end(ap);
}

int main()
{
    printNum(2,666,777);
    printNum(7,2,4,78,9,44,33,2);
    myprintf("%s-%d\n","ssp",666);
    return 0;
}