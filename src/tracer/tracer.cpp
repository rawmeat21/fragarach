#include "tracer.h"
#include "tracer.skel.h"
#include <bpf/libbpf.h>
#include <iostream>
#include <cstring>
#include<unistd.h>
#include<sys/syscall.h>
#include<sys/ioctl.h>
#include "../sandbox/sandbox.h"
#include<poll.h>
#include<seccomp.h>
#include<fcntl.h>
#include "../helper_functions.h"

Tracer::Tracer(Sandbox& sb)
: targetPID(sb.getChildPID())
{
    // read the notifyFd of child into seccompFd
    seccompFd=sb.seccompPipe[0];
}


const std::vector<event>& Tracer::getEvents()
{
    return events;
}

bool Tracer::start()
{
    read(seccompFd,&kernelNotifyFd,sizeof(int));
    close(seccompFd);

    std::cout<<"goon\n";
    int pidfd = syscall(SYS_pidfd_open, targetPID, 0);
    kernelNotifyFd = syscall(SYS_pidfd_getfd, pidfd, kernelNotifyFd, 0);// now Tracer has the file's view
    close(pidfd);

    // open and load the BPF skeleton
    skel = tracer_bpf__open_and_load();

    if(!skel)
    {
        std::cerr << "Failed to open and load BPF skeleton\n";
        return false;
    }

    // write targetPID into the target_pid map
    __u32 key = 0;
    __u32 pid = targetPID;
    bpf_map__update_elem(skel->maps.target_pid, &key, sizeof(key), &pid, sizeof(pid), BPF_ANY);

    std::cout<<"Tracer started with target PID: "<<targetPID<<"\n";
    // attach the tracepoint
    int err = tracer_bpf__attach(skel);
    if(err)
    {
        std::cerr << "Failed to attach BPF program\n";
        return false;
    }

    // create ring buffer, tell it to call handleEvent when data arrives
    rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handleEvent, this, nullptr);
    if(!rb)
    {
        std::cerr << "Failed to create ring buffer\n";
        return false;
    }

    return true;
}

void Tracer::pollmethod()
{
    std::cout << "rb pointer: " << rb << "\n";
    std::cout<<"doing poll...\n";

    if(!is_fd_open(kernelNotifyFd)) std::cerr<<"You are fucked!\n";
    else std::cerr<<"You are good\n";

    struct pollfd fds[2];

    fds[0].fd = ring_buffer__epoll_fd(rb); // eBPF ring buffer fd
    fds[0].events = POLLIN;

    fds[1].fd = kernelNotifyFd;
    fds[1].events = POLLIN;


    int ret=poll(fds,2,100);// watch 2 fds, 100ms timeout
    if(ret < 0) std::cerr << "poll failed: " << strerror(errno) << "\n";
    std::cout<<"now running checks..\n";
    if(fds[0].revents & POLLIN) ring_buffer__consume(rb); // drain the ring buffer
    std::cout<<"now running checks..2\n";
    if(fds[1].revents & POLLIN) handleSeccompNotification(); // handle seccomp event

    std::cout<<"poll done\n";
}

void Tracer::handleSeccompNotification()
{
    struct seccomp_notif notif{};

    int ret = ioctl(kernelNotifyFd,SECCOMP_IOCTL_NOTIF_RECV,&notif);

    if(ret < 0)
    {
        std::cerr << "RECV failed: " << strerror(errno) << "\n";
        return;
    }

    // get timestamp for event struct
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    event e;
    e.pid = notif.pid;
    e.syscall_nr = notif.data.nr;
    e.timestamp = timestamp;
    e.blocked = 1;
    events.push_back(e);

    struct seccomp_notif_resp resp{};

    resp.id=notif.id;
    resp.error=-EPERM;
    resp.val=0;
    resp.flags=0;

    ret=ioctl(kernelNotifyFd,SECCOMP_IOCTL_NOTIF_SEND,&resp);

    if(ret < 0) std::cerr << "SEND failed: " << strerror(errno) << "\n";
}

int Tracer::handleEvent(void* ctx, void* data, size_t size)
{
    Tracer* tracer = static_cast<Tracer*>(ctx);
    event* e = static_cast<event*>(data);
    event ec=*e;
    ec.blocked=0;
    tracer->events.push_back(ec);

    return 0;
}

void Tracer::stop()
{
    if(rb)
    {
        // consume any remaining events
        ring_buffer__consume(rb);
        ring_buffer__free(rb);
        rb = nullptr;
    }

    if(skel)
    {
        tracer_bpf__destroy(skel);
        skel = nullptr;
    }
}

