#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <linux/ptrace.h>
// weird syntax incoming... libbpf works like this sadly

struct event {
    __u32 pid;
    __u32 syscall_nr;
    __u64 timestamp;
};

// ring buffer to send events to userspace
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24); // buffer size is 16MB
} events SEC(".maps");

// target PID map
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} target_pid SEC(".maps");

struct sys_enter_args {
    unsigned long long unused;
    long syscall_nr;
    long args[6];
};

// SEC line will tell the kernel where to attach this function
SEC("tracepoint/raw_syscalls/sys_enter")
int handle_syscall(struct bpf_raw_tracepoint_args* ctx)
{
    // ctx is the cpu registers struct

    // get pid for the syscall
    // bpf_get_current_pid_tgid() returns a 64 bit number, upper 32 bits- PID, lower 32 bits- thread ID
    __u32 pid = bpf_get_current_pid_tgid() >> 32;


    __u32 key = 0;
    __u32* target = bpf_map_lookup_elem(&target_pid, &key);// get key 0 in target_pid map, returns NULL if not found

    if(!target || pid != *target) return 0;// not the syscall we want

    // fill and submit event
    struct event* e = bpf_ringbuf_reserve(&events, sizeof(struct event), 0);

    if(!e) return 0;// buffer is full

    e->pid = pid;
    e->syscall_nr = ctx->args[1];
    e->timestamp = bpf_ktime_get_ns();

    bpf_ringbuf_submit(e, 0);
    return 0;
}

char LICENSE[] SEC("license") = "GPL";