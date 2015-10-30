#include <stdio.h>
#include <fcntl.h>		/* open() */
#include <sys/types.h>		/* open() */
#include <sys/stat.h>		/* open() */
#include <asm/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>

#define DEVICE "/dev/parport0"


/* trivial example how to write data */
int write_data(int fd, unsigned char data)
{
    return(ioctl(fd, PPWDATA, &data));
}

/* example how to read 8 bit from the data lines */
int read_data(int fd)
{
    int mode, res;
    unsigned char data;

    mode = IEEE1284_MODE_ECP;
    res=ioctl(fd, PPSETMODE, &mode);	/* ready to read ? */
    mode=255;
    res=ioctl(fd, PPDATADIR, &mode);	/* switch output driver off */
    printf("ready to read data !\n");
    fflush(stdout);
    sleep(10);
    res=ioctl(fd, PPRDATA, &data);	/* now fetch the data! */
    printf("data=%02x\n", data);
    fflush(stdout);
    sleep(10);
    data=0;
    res=ioctl(fd, PPDATADIR, data);
    return 0;
}

/* example how to read the status lines. */
int status_pins(int fd)
{
    int val;

    ioctl(fd, PPRSTATUS, &val);
    val^=PARPORT_STATUS_BUSY; /* /BUSY needs to get inverted */

    printf("/BUSY  = %s\n",
            ((val & PARPORT_STATUS_BUSY)==PARPORT_STATUS_BUSY)?"HI":"LO");
    printf("ERROR  = %s\n",
            ((val & PARPORT_STATUS_ERROR)==PARPORT_STATUS_ERROR)?"HI":"LO");
    printf("SELECT = %s\n",
            ((val & PARPORT_STATUS_SELECT)==PARPORT_STATUS_SELECT)?"HI":"LO");
    printf("PAPEROUT = %s\n",
            ((val & PARPORT_STATUS_PAPEROUT)==PARPORT_STATUS_PAPEROUT)?"HI":"LO");
    printf("ACK = %s\n",
            ((val & PARPORT_STATUS_ACK)==PARPORT_STATUS_ACK)?"HI":"LO");
    return 0;
}

/* example how to use frob ... toggle STROBE on and off without messing
   around the other lines */
int strobe_blink(int fd)
{
    struct ppdev_frob_struct frob;

    frob.mask = PARPORT_CONTROL_STROBE;	/* change only this pin ! */
    while(1) {
        frob.val = PARPORT_CONTROL_STROBE;	/* set STROBE ... */
        ioctl(fd, PPFCONTROL, &frob);
        usleep(500);
        frob.val = 0;				/* and clear again */
        ioctl(fd, PPFCONTROL, &frob);
        usleep(500);
    }
}

int main(int argc, char **argv)
{
    struct ppdev_frob_struct frob;
    int fd;
    int mode;

    if((fd=open(DEVICE, O_RDWR)) < 0) {
        fprintf(stderr, "can not open %s\n", DEVICE);
        return 10;
    }
    if(ioctl(fd, PPCLAIM)) {
        perror("PPCLAIM");
        close(fd);
        return 10;
    }

    while(1)
    {
        write_data(fd,0x1f);
        sleep(1);
        write_data(fd,0);
        sleep(1);
    }
    /* put example code here ... */

    ioctl(fd, PPRELEASE);
    close(fd);
    return 0;
}
