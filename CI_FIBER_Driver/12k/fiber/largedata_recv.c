#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#define FIBER_IOC_MAGIC  'm'
#define FIBER_IOC_RESET    _IO(FIBER_IOC_MAGIC, 0)
#define FIBER_IOC_ALLOC_MEMORY _IOWR(FIBER_IOC_MAGIC,  1, int)
#define FIBER_IOC_MAXNR 1

#define MAX_LEN (11 * 0xff9)

void CI_HexDump(const void* data, int len)
{
    char str[80] = {0}, octet[10] = {0};
    const char* buf = (const char*)data;
    int ofs = 0, i = 0, l = 0;

    for (ofs = 0; ofs < len; ofs += 16)
    {
        memset(str,0,80);

        sprintf( str, "%07x: ", ofs );

        for (i = 0; i < 16; i++)
        {
            if ((i + ofs) < len)
            {
                sprintf( octet, "%02x ", buf[ofs + i] & 0xff );
            }
            else
            {
                strncpy( octet, "   ",3);
            }

            strncat(str,octet,3);
        }
        strncat(str,"  ",2);
        l = strlen(str);

        for (i = 0; (i < 16) && ((i + ofs) < len); i++)
        {
            str[l++] = isprint( buf[ofs + i] ) ? buf[ofs + i] : '.';
        }

        str[l] = '\0';
        printf( "%s\n", str );
    }
}
int alloc_memory(int fd,int size)
{
    int ret = 0;

    ret = ioctl(fd,FIBER_IOC_RESET);
    if(-1 == ret)
    {
        perror("ioctl reset error");
        return -1;
    }
    ret = ioctl(fd,FIBER_IOC_ALLOC_MEMORY,size);
    if(-1 == ret)
    {
        perror("ioctl alloc_memory memory error");
        return -1;
    }

    return 0;
}

int try_read(int fd,int size)
{
    int ret = 0;
    unsigned char data[MAX_LEN] = {0};

    ret = read(fd,&data,size);
    if(-1 == ret)
    {
        perror("read error");
        return -1;
    }
    else if(0 == ret)
    {
        printf("read 0 data\n");
        return -1;
    }
    CI_HexDump(&data,size);
    printf("\n");

    return 0;
}

int do_read(int fd,int size)
{
    int ret = 0;

    printf("do read size:%#x\n",size);
    ret = alloc_memory(fd,size);
    if(0 != ret)
    {
        return -1;
    }
    ret = try_read(fd,size);
    if(0 != ret)
    {
        return -1;
    }
}

int main()
{
    int fd = 0;
    int ret = 0;

    fd = open("/dev/fibera1",O_RDWR);
    if(-1 == fd)
    {
        perror("open /dev/fibera1 failed");
        return 1;
    }
    //ret = do_read(fd,-1);
    //assert(0 != ret);
    //ret = do_read(fd,0);
    //assert(0 != ret);
    //ret = do_read(fd,1);
    //assert(0 == ret);
    ret = do_read(fd,2);
    assert(0 == ret);
    //ret = do_read(fd,0xfef);
    //assert(0 == ret);
    //ret = do_read(fd,0xff0);
    //assert(0 == ret);
    //ret = do_read(fd,0xff1);
    //assert(0 == ret);
    //ret = do_read(fd,0xff2);
    //assert(0 == ret);
    //ret = do_read(fd,0xfff);
    //assert(0 == ret);
    //ret = do_read(fd,0xfef * 10);
    //assert(0 == ret);
    //ret = do_read(fd,0xff9 * 10 + 1);
    //assert(0 != ret);
    //ret = do_read(fd,0xff9 * 10 + 10);
    //assert(0 != ret);

    return 0;
}
