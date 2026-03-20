#pragma once
#include<unistd.h>
#include<sys/types.h>
#include<vector>
#include "../ebpf/event.h"
#include "../sandbox/sandbox.h"


struct tracer_bpf;
struct ring_buffer;


class Tracer
{

private:
    pid_t targetPID;
    struct tracer_bpf* skel=nullptr;
    struct ring_buffer* rb=nullptr;
    std::vector<event> events{};
    int seccompFd;
    int kernelNotifyFd;
    static int handleEvent(void* ctx, void* data, size_t size);

public:
    Tracer(Sandbox& sb);

    bool start();
    void pollmethod();
    void stop();
    void handleSeccompNotification();
    const std::vector<event>& getEvents();

};


