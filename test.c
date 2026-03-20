#include <unistd.h>
#include <stdio.h>
#include <sys/reboot.h>

int main()
{
    printf("hello from sandbox\n");
    
    // try to reboot — requires CAP_SYS_BOOT
    int ret = reboot(RB_AUTOBOOT);
    if(ret == -1)
        printf("reboot failed (expected — capability dropped)\n");
    else
        printf("reboot succeeded (BAD — capability not dropped!), BYEEE\n");
    
    return 0;
}