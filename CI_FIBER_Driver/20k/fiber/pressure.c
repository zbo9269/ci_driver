#include <stdio.h>

/*
 * use this program to take away CPU time slice.
 * run this program will take away about 25% CPU resources
 **/
int main()
{
    int i = 0;
    while(1)
    {
        for(i = 0;i < 20;i++)
        {
            printf("some thing be done\n");
        }
        usleep(1000); /*sleep 1ms*/
    }
    return 0;
}
