#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>

#define SNIFFER_IOC_MAGIC  'k'
#define SNIFFER_IOCRESET    _IO(SNIFFER_IOC_MAGIC, 0)
#define SNIFFER_IOC_SELECT_PORT _IOWR(SNIFFER_IOC_MAGIC,  11, int)
#define SNIFFER_IOC_MAXNR 1

int main()
{
    printf("%#x\n",SNIFFER_IOC_SELECT_PORT);
    return 0;
}
