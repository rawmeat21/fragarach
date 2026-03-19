#include "tracer.h"
#include "tracer.skel.h"
#include <bpf/libbpf.h>
#include <iostream>
#include <cstring>

Tracer::Tracer(int pid)
: targetPID(pid)
{}

Tracer::~Tracer()
{
    stop();
}

const std::vector<event>& Tracer::getEvents()
{
    return events;
}

bool Tracer::start()
{
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

void Tracer::poll()
{
    ring_buffer__poll(rb, 100); // 100ms timeout
}

int Tracer::handleEvent(void* ctx, void* data, size_t size)
{
    Tracer* tracer = static_cast<Tracer*>(ctx);
    event* e = static_cast<event*>(data);
    tracer->events.push_back(*e);
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

