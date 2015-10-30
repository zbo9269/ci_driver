#include <stdio.h>
#include <fcntl.h>

int main()
{
    int fd = 0;
    int ret = 0;
    unsigned char c = 1;
    unsigned char z = 0;

    fd = open("/dev/short0",O_RDWR);
    if(-1 == fd)
    {
        perror("opcn /dev/short0 failed");
        return 1;
    }
    while(1)
    {
        printf("%#x\n",c);
        /*close*/
        ret = write(fd,&z,1);
        if(-1 == ret)
        {
            perror("write error");
            return 1;
        }
        sleep(1);
        /*open*/
        ret = write(fd,&c,1);
        if(-1 == ret)
        {
            perror("write error");
            return 1;
        }
        sleep(1);
        c <<= 1;
    }

    return 0;
}

