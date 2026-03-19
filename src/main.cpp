#include<iostream>
#include<unistd.h>
#include "sandbox/sandbox.h"

bool firstRun()
{
    return access("/opt/fragarach/rootfs/bin", F_OK);
}

bool setup()
{
    system("mkdir -p /opt/fragarach/rootfs");
    system("mkdir -p /opt/fragarach/overlay/upper");
    system("mkdir -p /opt/fragarach/overlay/work");
    system("mkdir -p /opt/fragarach/overlay/merged");

    int ret = system(
        "wget -q https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/"
        "x86_64/alpine-minirootfs-3.18.0-x86_64.tar.gz "
        "-O /tmp/alpine.tar.gz"
    );
    
    if (ret != 0) {
        std::cerr << "Failed to download Alpine rootfs\n";
        return false;
    }
    
    system("tar -xzf /tmp/alpine.tar.gz -C /opt/fragarach/rootfs");
    system("rm /tmp/alpine.tar.gz");

    return true;
}

int main(int argc,char* argv[])
{
    if(argc<2)
    {
        std::cerr<<"Binary path not provided.\n";
        return 1;
    }

    if(firstRun()) if(!setup()) return 1;


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