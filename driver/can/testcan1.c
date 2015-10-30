#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#define CAN_MEM_SIZE	0x3A20	/*单个设备全局内存最大14880字节*/
#define CAN_SEND_SIZE	0xD0	/*发送内存最大208字节*/

static char send_buffer[CAN_SEND_SIZE]={0x88,0x01,0x04,0x00,0x00,0x11,0x11,0x11,0x11,0x00,0x00,0x00,0x00};
static char recv_buffer[CAN_MEM_SIZE];

int main()
{
	int fd = 0;
	int ret = 0;
	int i = 0;

	/*for (i = 13; i > 0;i--)
	{
		send_buffer[i] = i & 0xff;
	}*/

	fd = open("/dev/can1",O_RDWR);

	if(-1 == fd)
	{
		perror("can't open can");
	}
	else
	{
		printf("can1 has opened,fd is:%d\n",fd);
	}

	ret = read(fd,recv_buffer,CAN_MEM_SIZE);
	if(-1 == ret)
	{
		perror("read error");
	}
	/*else if(ret < NET_MEM_SIZE)
	{
		printf("only read partial\n");
	}*/
	else
	{
		printf("read correct:%d\n",ret);
		for(i = 0;i < ret;i++)
		{
			printf("%0#6x:%x ",i,recv_buffer[i] & 0xff);
		}
		printf("\n");
	}

	ret = write(fd,send_buffer,13);
	if(-1 == ret)
	{
		perror("write error");
	}

	else if(ret < 13)
	{
		printf("only write partial\n");
	}

	
	return 0;
}
