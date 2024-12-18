#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/adxl345-0"

int main(int argc, char **argv)
{
    int fd;
    char buf[2];

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0)
    {
        printf("Error opening file\n");
        return -1;
    }

    /*test ioctl*/
    // 0x00 -> X axis
    // 0x01 -> Y axis
    // 0x02 -> Z axis
    for (size_t type = 0; type < 3; type++)
    {
        printf("Reading from axis %d\n", type);
        if (ioctl(fd, 0, type) < 0)
        {
            printf("Error sending data\n");
            close(fd);
            return -1;
        }
        // reads 2 bytes from the device and compares them to the expected values
        int bytes_read = 0;
        for (size_t i = 0; i < 10; i++)
        {
            usleep(10000);
            bytes_read = read(fd, buf, sizeof(buf));
            if (bytes_read < 0)
            {
                printf("Error reading data\n");
                close(fd);
                return -1;
            }
            else
            {
                int16_t value = buf[0] | (buf[1] << 8); // little-endian
                printf("Read:%d\n", value);
            }
        }
    }

    close(fd);
    return 0;
}
/*
mount -t 9p -o trans=virtio mnt /mnt -oversion=9p2000.L,msize=10240
insmod /mnt/adxl345.ko
arm-linux-gnueabihf-gcc -Wall -static -o main main.c
make CROSS_COMPILE=arm-linux-gnueabihf- ARCH=arm KDIR=../linux-5.15.6/build/
*/