#include<iostream>
#include<unistd.h>
#include "sandbox/sandbox.h"


bool setup()
{
    // this is where AlpineLinux rootfs lives
    system("mkdir -p /opt/fragarach/rootfs");

    // create overlay directories

    // upper- writable directory, all writes go here
    system("mkdir -p /opt/fragarach/overlay/upper");

    // work- internal bookeeping for kernel
    system("mkdir -p /opt/fragarach/overlay/work");

    // merged- combined view of /upper and /rootfs
    system("mkdir -p /opt/fragarach/overlay/merged");


    // create cgroup for fragarach
    system("mkdir -p /sys/fs/cgroup/fragarach/");
    
    // enable memory, cpu, and pid controllers for child cgroups
    system("echo '+memory +cpu +pids' > /sys/fs/cgroup/cgroup.subtree_control");
    system("echo '+memory +cpu +pids' > /sys/fs/cgroup/fragarach/cgroup.subtree_control");

    // download AlpineLinux rootfs if it doesn't exist
    if(access("/opt/fragarach/rootfs/bin", F_OK))
    {
        int ret = system(
            "wget -q https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/"
            "x86_64/alpine-minirootfs-3.18.0-x86_64.tar.gz "
            "-O /tmp/alpine.tar.gz"
        );
        
        if (ret != 0) {
            std::cerr << "Failed to download Alpine rootfs\n";
            return false;
        }    
        // save rootfs to /opt/fragarach/rootfs
        system("tar -xzf /tmp/alpine.tar.gz -C /opt/fragarach/rootfs");
        system("rm /tmp/alpine.tar.gz");
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
}