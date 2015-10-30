#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define FIBER_IOC_MAGIC  'k'
#define FIBER_IOCRESET    _IO(FIBER_IOC_MAGIC, 0)
/*约定光纤从60开始*/
#define FIBER_IOC_ALLOC_MEMORY _IOWR(FIBER_IOC_MAGIC,60,int)
#define FIBER_IOC_MAXNR 1

int main()
{
    printf("FIBER_IOCRESET:%#x\n",FIBER_IOCRESET);
    printf("FIBER_IOC_ALLOC_MEMORY:%#x\n",FIBER_IOC_ALLOC_MEMORY);
    return 0;
}
