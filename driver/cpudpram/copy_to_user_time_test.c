#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <assert.h>

#define DPRAM_IOC_MAGIC  'k'
#define DPRAM_IOCRESET    _IO(DPRAM_IOC_MAGIC, 0)
#define DPRAM_IOC_ALLOC_MEMORY _IOWR(DPRAM_IOC_MAGIC,  1, int)

#define BUF_MAX_SIZE 0x10000

int test_read(int fd,int size)
{
    int ret = 0;
    static char buf[BUF_MAX_SIZE] = {0};
    int i = 0;

    ret = read(fd,buf,size);
    if (-1 == ret)
    {
        perror("read failed");

        return -1;
    }
}

int main()
{
    int32_t ret = 0;
    int fd = 0;
    int i = 0,j = 0;

    fd = open("/dev/dpram1",O_RDWR);

    if (-1 == fd)
    {
        perror("open /dev/dpram1 failed");

        return -1;
    }
    for(i = 1;i < 0xffff;i += 0xff)
    {
        printf("i:%d\n",i);
        for(j = 0;j < 4;j++)
        {
            ret = test_read(fd,i);
            if(-1 == ret)
            {
                return -1;
            }
        }
    }
    return 0;
}
