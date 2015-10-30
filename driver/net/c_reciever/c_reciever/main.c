#include <winsock2.h>   
#include <stdio.h>   
#include <stdlib.h>   

#pragma comment(lib,"WS2_32.lib")

#define DEFAULT_COUNT 5
#define DEFAULT_PORT 3000
#define DEFAULT_BUFFER 2048
#define DEFAULT_MESSAGE _T("This is a test of the emergency broadcasting system")

SOCKET sClient;

void main()
{
    WSADATA wsd;
    struct sockaddr_in server;
    struct sockaddr_in server1;
    int nServerLen;
    int size,i,j;
    int len;
    int iPort = DEFAULT_PORT;
    BYTE buffer[1024];
    int addr_len =sizeof(struct sockaddr_in);


    //Send and receive data
    char szBuffer[DEFAULT_BUFFER];

    if (WSAStartup(MAKEWORD(2,2), &wsd) != 0)
    {
        printf("Failed to load Winsock library!\n");
        return 1;
    }

    //Create the socket, and attempt to connect to the server
    sClient = socket(AF_INET, SOCK_DGRAM, 0);
    if (sClient == INVALID_SOCKET)
    {
        printf("socket() failed:%d\n", WSAGetLastError());
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(3000);
    server.sin_addr.S_un.S_addr = inet_addr("192.168.1.100");

    server1.sin_family = AF_INET;
    server1.sin_port = htons(3000);
    server1.sin_addr.S_un.S_addr = inet_addr("192.168.1.2");

    if (connect(sClient, (struct sockaddr*)&server1, sizeof(struct sockaddr))
        == SOCKET_ERROR)
    {
        printf("connect() failed:%d\n", WSAGetLastError());
        return 1;
    }

    buffer[0] = 0x55;
    buffer[1] = 0xAA;
    buffer[10] = 0x03;//战场号
    buffer[15] = 0x0b;
    buffer[16] = 0x00;
    i = buffer[15];
    j = buffer[16]<<8;
    size = 19 + i + j;
    printf("%d\n",size);
    for (j =0;j<size;j++)
    {
        if(j!=0&&j!=1&&j!=10&&j!=15&&j!=16)
            buffer[j] = 0xEE;
    }

    if (size!=0)
    {
        size = sendto(sClient,(const char*)buffer,size,0,(struct sockaddr*)&server1,addr_len);
        //	size = sendto(sClient,(const char*)buffer,size,0,(struct sockaddr*) &server1,sizeof(struct sockaddr));
        if (size > 0)
        {
            printf("信息发送成功！\n");
            for (j=0;j<size;j++)
            {
                printf("%02X ",buffer[j]);
            }
            printf("\n");
        }
        else if (size == SOCKET_ERROR)
        {
            printf("信息发送失败:%d\n", WSAGetLastError());
        }
    }
    memset(buffer,0,1024);

    len = recvfrom(sClient,buffer,sizeof(buffer),0,(struct sockaddr_in *)&server1,&addr_len);

    //	size = recvfrom(sClient,buffer,200,0,(struct sockaddr*)&server1,sizeof(struct sockaddr));

    /*if (SIZE != 0)
    {
    SIZE = sendto(s2,(const char*)buffer,SIZE,0,(sockaddr*));
    }*/
    printf("%d ",len);

    printf("%s",buffer);


    /*int test = bind(s1,(sockaddr*)&sin,sizeof(sin));
    printf(" port:%d\n",test);
    size = recvfrom(s1,(char*)buffer,200,0,(sockaddr*)&from,&addrlen);
    while (-1==size)
    {
    size = recvfrom(s1,(char*)buffer,200,0,(sockaddr*)&from,&addrlen);
    }*/
    system("pause");
}