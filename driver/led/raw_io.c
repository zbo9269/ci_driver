#include <stdio.h>
#include <sys/io.h>

#define BASEADDR 0x378

int main(int argc, char **argv)
{
    int i;

    if(ioperm(BASEADDR, 4, 1) < 0) {
        fprintf(stderr, "could not get i/o permission. are you root ?\n");
        return 5;
    }
    while(1)
    {
        outb(0x00, BASEADDR);
        sleep(1);
        outb(0x1F, BASEADDR);
        sleep(1);
    }
    ioperm(BASEADDR, 4, 0);
    return 0;
}
