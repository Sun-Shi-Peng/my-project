#include <stdint.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main()
{
    int efd=eventfd(0,EFD_CLOEXEC|EFD_NONBLOCK);
    if(efd<0)
    {
        perror("eventfd fdiled!!");
        return -1;
    }
    uint64_t val=2;
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    write(efd,&val,sizeof(val));
    uint64_t res=0;
    read(efd,&res,sizeof(res));
    printf("%ld\n",res);
    close(efd);
    return 0;
}