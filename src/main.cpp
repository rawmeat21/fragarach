#include<iostream>
#include<unistd.h>
#include "sandbox/sandbox.h"
#include "tracer/tracer.h"
#include "syscall_graph/syscallGraph.h"
#include<chrono>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/syscall.h>
#include "helper_functions.h"

bool setup()
{
    // this is where AlpineLinux rootfs lives
    mkdirP("/opt/fragarach/rootfs");

    // create overlay directories

    // upper- writable directory, all writes go here
    mkdirP("/opt/fragarach/overlay/upper");

    // work- internal bookeeping for kernel
    mkdirP("/opt/fragarach/overlay/work");

    // merged- combined view of /upper and /rootfs
    mkdirP("/opt/fragarach/overlay/merged");

    // create cgroup for fragarach
    mkdirP("/sys/fs/cgroup/fragarach");

    // enable memory, cpu, and pid controllers for child cgroups

    int fd=open("/sys/fs/cgroup/cgroup.subtree_control",O_WRONLY);
    
    if(fd==-1)
    {
        std::cerr<<"Couldn't open /sys/fs/cgroup/cgroup.subtree_control : "<<strerror(errno)<<"\n";
        return false;
    }

    const char* wr="+memory +cpu +pids";
    write(fd,wr,strlen(wr));
    close(fd);

    fd=open("/sys/fs/cgroup/fragarach/cgroup.subtree_control",O_WRONLY);
    
    if(fd==-1)
    {
        std::cerr<<"Couldn't open /sys/fs/cgroup/fragarach/cgroup.subtree_control : "<<strerror(errno)<<"\n";
        return false;
    }

    write(fd,wr,strlen(wr));
    close(fd);


    // download AlpineLinux rootfs if it doesn't exist
    if(access("/opt/fragarach/rootfs/bin", F_OK))
    {
        char* wgetArgs[] = {
            (char*)"wget",
            (char*)"-q",
            (char*)"https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/alpine-minirootfs-3.18.0-x86_64.tar.gz",
            (char*)"-O",
            (char*)"/tmp/alpine.tar.gz",
            nullptr
        };

        int ret=run("/usr/bin/wget", wgetArgs);
        
        if (ret) 
        {
            std::cerr << "Failed to download Alpine rootfs\n";
            return false;
        }    

        // save rootfs to /opt/fragarach/rootfs
        char* tarArgs[] = {
            (char*)"tar",
            (char*)"-xzf",
            (char*)"/tmp/alpine.tar.gz",
            (char*)"-C",
            (char*)"/opt/fragarach/rootfs",
            nullptr
        };

        ret=run("/usr/bin/tar", tarArgs);

        if (ret) 
        {
            std::cerr << "Failed to extract files\n";
            return false;
        }  

        unlink("/tmp/alpine.tar.gz");
    }

    return true;
}

int main(int argc,char* argv[])
{
    if(argc<2)
    {
        std::cerr<<"Binary path not provided.\n";
        return 1;
    }

    if(!setup())
    {
        std::cerr<<"Setup failed\n";
        return 1;
    }


    std::string binaryPath=argv[1];

    if(access(binaryPath.c_str(),F_OK))
    {
        std::cerr<<"Error: File not found: "<<binaryPath<<"\n";
        return 1;
    }

    Sandbox sandbox{binaryPath};

    sandbox.setTimeout(30);

    sandbox.launch();
    Tracer tracer{sandbox.getChildPID()};
    tracer.start();

    write(sandbox.syncPipe[1],"x",1); // write to pipe, only now child will run
    close(sandbox.syncPipe[1]);

    auto start=std::chrono::steady_clock::now();

    while (sandbox.isRunning())
    {
        std::cout<<"Check\n";
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

        if(elapsed>=sandbox.getTimeoutTime())
        {
            std::cerr<<"Sandbox timed out, killing child...\n";
            break;
        }

        tracer.poll();

        usleep(1e5);
    }
    std::cout<<"Sandbox closed\n";

    tracer.stop();
    sandbox.cleanup();

    std::cout<<"Number of syscalls: "<<tracer.getEvents().size()<<"\n";
    for(auto& e : tracer.getEvents())
    {
        std::cout << "PID: " << e.pid << " SYSCALL: " << e.syscall_nr << " TIME: " << e.timestamp << "\n";
    }

    SyscallGraph sg{};

    sg.build(tracer.getEvents());

    COOGraph cg=sg.cooExport();

    // for(int i=0;i<cg.from.size();++i)
    // {
    //     std::cout<<cg.from[i]<<' '<<cg.to[i]<<' '<<cg.weights[i]<<"\n";
    // }
}