#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <assert.h>
#include <sched.h>

#define DPRAM_IOC_MAGIC  'k'
#define DPRAM_IOCRESET    _IO(DPRAM_IOC_MAGIC, 0)
#define DPRAM_IOC_ALLOC_MEMORY _IOWR(DPRAM_IOC_MAGIC,  1, int)

#define BUF_MAX_SIZE 0x10000

int alloc_write(int fd,int size)
{
    int ret = 0;
    static char buf[BUF_MAX_SIZE] = {0};
    int i = 0;

    ret = ioctl(fd,DPRAM_IOCRESET);
    if (-1 == ret)
    {
        perror("use ioctl reset dpram memory failed\n");

        return -1;
    }

    ret = ioctl(fd,DPRAM_IOC_ALLOC_MEMORY,size);
    if (-1 == ret)
    {
        perror("alloc memory failed");

        return -1;
    }
    srand(time(NULL));

    for(i = 0;i < size;i++)
    {
        buf[i] = rand() & 0xff;
    }
    ret = write(fd,buf,size);
    if (-1 == ret)
    {
        perror("write failed");

        return -1;
    }
}

int main()
{
    int32_t ret = 0;
    int fd = 0;
    int i = 0,j = 0;
    struct sched_param param; 
    int maxpri; 
    param.sched_priority = 40; 
    if (sched_setscheduler(getpid(), SCHED_RR, &param) == -1)
    { 
        perror("sched_setscheduler() failed"); 
        return -1;
    } 

    fd = open("/dev/dpram1",O_RDWR);

    if (-1 == fd)
    {
        perror("open /dev/dpram1 failed");

        return -1;
    }
    for(i = 1;i < 0xffff;i += 0xff)
    {
        ret = alloc_write(fd,i);
        if(-1 == ret)
        {
            return -1;
        }
    }
    return 0;
}
