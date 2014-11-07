#include <stdio.h>
#include <unistd.h>
#include <sys/io.h>  // needed for inb/outb
#define EC_INDEX 0x6f0
#define EC_DATA  0x6f1

int main() {
    signed char ambtemp, start;
    if (iopl(3))
    {  // Linux-specific, e.g. DOS doesn't need this
        printf("Failed to get I/O access permissions.\n");
        printf("You must be root to run this.\n");
        return 1;
    }
    
    outb(0x40, EC_INDEX); //access to start/stop register start = 0x01 | inb(EC_DATA);
    outb(start, EC_DATA); //activate monitor mode printf("Press CTRL+C to cancel!\n"); printf("AMBIENT\n");
    
    outb(0x26, EC_INDEX);  //read out ambient temp
    ambtemp = inb(EC_DATA);
    printf("%3d\n", ambtemp);
    fflush(stdout);

return 0;
}