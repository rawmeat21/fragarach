#include "sandbox.h"
#include<vector>
#include<sched.h>
#include<signal.h>
#include<string>
#include<cstring>
#include<errno.h>
#include<iostream>
#include<stdio.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/mount.h>
#include<sys/syscall.h>
#include<unistd.h>


static int childFunction(void*);

Sandbox::Sandbox(std::string_view binaryPath)
:binaryPath(binaryPath)
{}

const std::string& Sandbox::getBinaryPath()
{
    return binaryPath;
}

Sandbox& Sandbox::setTimeout(int seconds)
{
    timeoutTime=seconds;
    return *this;
}

Sandbox& Sandbox::setMemLimit(size_t bytes)
{
    memLimit=bytes;
    return *this;
}

void Sandbox::resetOverlay()
{
    system("rm -rf /opt/fragarach/overlay/upper/*");
    system("rm -rf /opt/fragarach/overlay/work/*");

    system("mkdir -p /opt/fragarach/overlay/upper");
    system("mkdir -p /opt/fragarach/overlay/work");
    system("mkdir -p /opt/fragarach/overlay/merged");    
}

bool Sandbox::launch()
{
    resetOverlay();
    // launches the binary as a child process

    const int STACK_SIZE=1024*1024;

    std::vector<char> childStack(STACK_SIZE);

    childPID=clone(childFunction,childStack.data()+STACK_SIZE,CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | SIGCHLD,this);

    if(childPID==-1)
    {
        std::cerr<<"Couldn't create a Sandbox for child process: "<<strerror(errno)<<"\n";
        return false;
    }

    std::cout<<"Got PID: "<<childPID<<"\n";

    int status;
    waitpid(childPID,&status,0);

    return true;
}

static int childFunction(void* arg)
{
    // this function will run inside the namespace

    // system("pwd");

    Sandbox* sb=static_cast<Sandbox*>(arg);

    const char* lower  = "/opt/fragarach/rootfs";
    const char* upper  = "/opt/fragarach/overlay/upper";
    const char* work   = "/opt/fragarach/overlay/work";
    const char* merged = "/opt/fragarach/overlay/merged";

    umount2(merged,MNT_DETACH);

    std::string opts="lowerdir="+std::string(lower)+",upperdir="+std::string(upper)+",workdir="+std::string(work);


    if(mount("overlay",merged,"overlay",0,opts.c_str()))
    {
        std::cerr<<"Overlay mount failed: "<<strerror(errno)<<"\n";
        return 1;
    }

    if(mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr))
    {
        std::cerr << "remount private failed: " << strerror(errno) << "\n";
        return 1;
    }

    if(mount(merged, merged, nullptr, MS_BIND, nullptr))
    {
        std::cerr << "bind mount failed: " << strerror(errno) << "\n";
        return 1;
    }

    if(chdir(merged))
    {
        std::cerr<<"chdir failed: "<<strerror(errno)<<"\n";
        return 1;
    }
    // pivot root to current dir (merged)
    if(syscall(SYS_pivot_root,".","."))
    {
        std::cerr<<"pivot_root failed: "<<strerror(errno)<<"\n";
        return 1;
    }


    if(chdir("/"))
    {
        std::cerr << "chdir to new root failed: " << strerror(errno) << "\n";
        return 1;
    }

    if(umount2(".",MNT_DETACH))
    {
        std::cerr<<"umount failed: "<<strerror(errno)<<"\n";
    }

    // replace the child process by the binary process
    execve(sb->getBinaryPath().c_str(),nullptr,nullptr);

    std::cerr<<"Failed to replace child process with target process: "<<strerror(errno)<<"\n";
    return 1;   
}

void Sandbox::cleanup()
{

}