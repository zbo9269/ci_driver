#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>

#define FIBER_IOC_MAGIC  'k'
#define FIBER_IOCRESET    _IO(FIBER_IOC_MAGIC, 0)
/*约定光纤从60开始*/
#define FIBER_IOC_ALLOC_MEMORY _IOWR(FIBER_IOC_MAGIC,60,int)
#define FIBER_IOC_MAXNR 1

int ioc_alloc_memory(int fd,int mem_size)
{
    int ret = 0;

    printf("alloc mem_size %#x\n",mem_size);

    ret = ioctl(fd,FIBER_IOCRESET);
    if(0 > ret)
    {
        perror("ioctl FIBER_IOCRESET failed");
        return -1;
    }
    ret = ioctl(fd,FIBER_IOC_ALLOC_MEMORY,mem_size);
    if(0 > ret)
    {
        perror("ioctl FIBER_IOC_ALLOC_MEMORY failed");
        return -1;
    }
    return 0;
}

void ioc_test(const char* file_name)
{
    int fd = 0;
    int mem_size = 0;
    int ret = 0;

    fd = open(file_name,O_RDWR);
    if(0 > fd)
    {
        perror("open failed");
        return;
    }
    ret = ioc_alloc_memory(fd,0);
    assert(-1 == ret);
    for(mem_size = 1;mem_size < 0xffc;mem_size++)
    {
        ret = ioc_alloc_memory(fd,mem_size);
        assert(-1 != ret);
    }
    ret = ioc_alloc_memory(fd,0xffc);
    assert(-1 == ret);
    return;
}
int main()
{
    ioc_test("/dev/fibera1");
    ioc_test("/dev/fibera2");
    ioc_test("/dev/fibera3");
    ioc_test("/dev/fibera4");
    ioc_test("/dev/fibera5");
    ioc_test("/dev/fiberb1");
    ioc_test("/dev/fiberb2");
    ioc_test("/dev/fiberb3");
    ioc_test("/dev/fiberb4");
    ioc_test("/dev/fiberb5");
    return 0;
}
