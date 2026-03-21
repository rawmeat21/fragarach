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
#include "json.hpp"
#include<fstream>
#include<filesystem>

bool setup()
{
    mkdirP("/opt/fragarach/rootfs");

    // create overlay directories

    // upper- writable directory, all writes go here
    mkdirP("/opt/fragarach/overlay/upper");

    // work- internal bookeeping for kernel
    mkdirP("/opt/fragarach/overlay/work");

    // merged- combined view of /upper and /rootfs
    mkdirP("/opt/fragarach/overlay/merged");

    // create graphs directory
    mkdirP("/opt/fragarach/raw");

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

    if(access("/opt/fragarach/rootfs/bin", F_OK))
    {
        std::cerr<<"Please install a proper filesystem in /opt/fragarach/rootfs/\n";
        return false;
    }

    return true;
}

int main(int argc,char* argv[])
{
    int label=-1;// indicates if current binary is malware

    if(argc<2)
    {
        std::cerr<<"Binary path not provided.\n";
        return 1;
    }

    if(argc>=3)
    {
        label=std::stoi(argv[2]);
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
    Tracer tracer{sandbox};
    std::cout<<"Tracer start\n";
    tracer.start();
    std::cout<<"Tracer started\n";

    write(sandbox.syncPipe[1],"x",1); // write to pipe, only now child will run
    // std::cerr<<strerror(errno)<<"\n";
    close(sandbox.syncPipe[1]);

    std::cout<<"Checking start\n";

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

        std::cout<<"Enter poll\n";
        tracer.pollmethod();
        std::cout<<"Exit poll\n";
        usleep(1e5);
    }

    std::cout<<"Sandbox closed\n";

    tracer.stop();
    sandbox.cleanup();

    std::cout<<"Number of syscalls: "<<tracer.getEvents().size()<<"\n";
    for(auto& e : tracer.getEvents())
    {
        if(e.blocked) std::cout << "[BLOCKED] ";
        std::cout << "PID: " << e.pid << " SYSCALL: " << e.syscall_nr << " TIME: " << e.timestamp << "\n";
    }

    SyscallGraph sg{};

    sg.build(tracer.getEvents());
    COOGraph cg=sg.cooExport();
    cg.label=label;    

    if(cg.label==-1)
    {
        std::cout<<"Inference mode\n";
    }
    else
    {
        std::cout<<"Testing mode\n";
        std::cout<<"The malware has been tested\n";

        std::string binaryName=std::filesystem::path(binaryPath).filename();

        std::string jsonfile="/opt/fragarach/raw/"+binaryName+".json";

        std::ofstream outfile(jsonfile);

        if(!outfile.is_open())
        {
            std::cerr<<"Failed to open file\n";
            return 1;
        }

        json data=cg.tojson();

        outfile<<data;
        outfile.close();
    }

}