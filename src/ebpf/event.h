#pragma once
#include<stdint.h>

// will be sent through the ring buffer everytime a syscall is made
struct event {
    __uint32_t pid;// pid
    __uint32_t syscall_nr;// syscall number
    __uint64_t timestamp;// when was syscall made    
};

