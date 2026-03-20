#include <unistd.h>
#include <stdio.h>
#include <sys/reboot.h>
#include <sys/types.h>

int main()
{
    printf("hello from sandbox\n");
    uid_t euid = geteuid();

    // Check if the EUID is 0 (root)
    if (euid == 0) {
        printf("The process is running as root (EUID = 0).\n");
    } else {
        printf("The process is not running as root (EUID = %d).\n", euid);
    }
    
    return 0;
}