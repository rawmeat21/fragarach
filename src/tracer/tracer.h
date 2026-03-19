#pragma once
#include<unistd.h>
#include<sys/types.h>
#include<vector>
#include "../ebpf/event.h"



struct tracer_bpf;
struct ring_buffer;


class Tracer
{

private:
    pid_t targetPID;
    struct tracer_bpf* skel=nullptr;
    struct ring_buffer* rb=nullptr;
    std::vector<event> events{};

    static int handleEvent(void* ctx, void* data, size_t size);

public:
    Tracer(int pid);
    ~Tracer();

    bool start();
    void poll();
    void stop();
    const std::vector<event>& getEvents();

};


