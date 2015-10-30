#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define BUF_SIZE 0xffff

/*
 * 功能描述    : 使用16进制方式打印数据，该函数不允许使用strcat等长度未知的函数
 * 返回值      : 
 * 参数        : @buf以char数组保存的数据
 *               @len数据的长度
 * 作者        : 张彦升
 * 日期        : 2014年3月5日 12:45:05
 **/
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
int test_read(int fd,int len)
{
    int ret = 0;
    char buf[BUF_SIZE] = {0};
    char request[13] = {0x88,0x00,0x00,0x00,0x28,0x11,0x11,0x11,0x11,0x00,0x00,0x00,0x00};

    printf("test read %#x\n",len);
    ret = write(fd,request,13);
    if(-1 == ret)
    {
        perror("request error");
    }
    /*测试读取len个字节*/
    ret = read(fd,buf,len);
    if(-1 == ret)
    {
        perror("read error");
        return -1;
    }
    else if(0 == ret)
    {
        printf("read none data\n");
        return -1;
    }
    else
    {
        printf("read %#x char\n",ret);
        CI_HexDump(buf,ret);
    }
    return 0;
}

int main()
{
    int fd = 0;
    const char* dev_name = "/dev/cana";
    int ret = 0;

    fd = open(dev_name,O_RDWR);
    if(-1 == fd)
    {
        perror("open error");
        return 1;
    }
    /*测试读取0个字节*/
    ret = test_read(fd,0);
    assert(-1 != ret);
    /*测试读取1个字节*/
    ret = test_read(fd,1);
    assert(-1 != ret);
    /*测试读取2个字节*/
    ret = test_read(fd,2);
    assert(-1 != ret);
    /*测试读取3个字节*/
    ret = test_read(fd,3);
    assert(-1 != ret);
    /*测试读取7个字节*/
    ret = test_read(fd,7);
    assert(-1 != ret);
    /*测试读取0xff个字节*/
    ret = test_read(fd,0xff);
    assert(-1 != ret);
    /*测试读取0x1929个字节*/
    ret = test_read(fd,0x1929);
    assert(-1 != ret);
    /*测试读取0x1930个字节*/
    ret = test_read(fd,0x1930);
    assert(-1 != ret);
    /*测试读取0x1931个字节*/
    ret = test_read(fd,0x1931);
    assert(-1 != ret);
    /*测试读取0x1940个字节*/
    ret = test_read(fd,0x1940);
    assert(-1 != ret);
    /*测试读取0xffffffff个字节*/
    ret = test_read(fd,-1);
    assert(-1 != ret);

    return 0;
}
