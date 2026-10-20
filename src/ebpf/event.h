#pragma once
#include<stdint.h>

struct event {
    __uint32_t pid;// pid
    __uint32_t syscall_nr;// syscall number
    __uint64_t timestamp;// when was syscall made
    __uint8_t blocked;    
};

