#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>

#define CAN_IOC_MAGIC       'e'
#define CAN_IOCRESET        _IO(CAN_IOC_MAGIC, 0)
#define CAN_IOC_SET_DATAMASK _IOW(CAN_IOC_MAGIC, 1, int*)
#define CAN_IOC_SET_DELAY_COUNTER _IOW(CAN_IOC_MAGIC, 2, int)
#define CAN_IOC_SET_READ_ADDRESS _IOW(CAN_IOC_MAGIC, 3, int)
#define CAN_IOC_CHECK_SJA _IOW(CAN_IOC_MAGIC, 4, int)
#define CAN_IOC_CHECK_MEM _IOW(CAN_IOC_MAGIC, 5, int)

int main()
{
    int ret = 0;
    int fd = 0;
    fd = open("/dev/cana",O_RDWR);
    if(0 > fd)
    {
        perror("open /dev/cana failed");
        return -1;
    }
    ret = ioctl(fd,CAN_IOCRESET);
    if(0 > ret)
    {
        perror("CAN_IOC_MAGIC failed");
        return -1;
    }
    else
    {
        printf("reset ok\n");
    }
    ret = ioctl(fd,CAN_IOC_CHECK_SJA);
    if(0 > ret)
    {
        perror("CAN_IOC_CHECK_SJA failed");
        return -1;
    }
    else
    {
        printf("check sja ok\n");
    }
    ret = ioctl(fd,CAN_IOC_CHECK_MEM);
    if(0 > ret)
    {
        perror("CAN_IOC_CHECK_MEM failed");
        return -1;
    }
    else
    {
        printf("check mem ok\n");
    }
    printf("done\n");
    return 0;
}
