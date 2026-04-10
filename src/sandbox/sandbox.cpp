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
#include<sys/capability.h>
#include<unistd.h>
#include<seccomp.h>
#include<chrono>
#include "../helper_functions.h"

static int childFunction(void*);


const char* lower  = "/opt/fragarach/rootfs";
const char* upper  = "/opt/fragarach/overlay/upper";
const char* work   = "/opt/fragarach/overlay/work";
const char* merged = "/opt/fragarach/overlay/merged";

Sandbox::Sandbox(std::string_view binaryPath)
:binaryPath(binaryPath)
{}

const std::string& Sandbox::getBinaryPath()
{
    return binaryPath;
}

const int Sandbox::getMemLimit()
{
    return memLimit;
}

const int Sandbox::getChildPID()
{
    return childPID;
}

const int Sandbox::getPIDLimit()
{
    return pidLimit;
}

const int Sandbox::getTimeoutTime()
{
    return timeoutTime;
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
    // clean 

    rmRF("/opt/fragarach/overlay/upper/");
    rmRF("/opt/fragarach/overlay/work/");

    mkdirP("/opt/fragarach/overlay/upper");
    mkdirP("/opt/fragarach/overlay/work");   
    mkdirP("/opt/fragarach/overlay/merged");
}

bool Sandbox::launch()
{    
    std::cout<<"Starting launch\n";

    // launches the binary as a child process
    resetOverlay();

    if(cpy(binaryPath,"/opt/fragarach/overlay/upper/target")) 
    {
        std::cerr << "Failed to copy binary into sandbox\n";
        return false;
    }
    else std::cout<<"target copied\n";

    chmod("/opt/fragarach/overlay/upper/target",0755);

    
    pipe(syncPipe);
    pipe(seccompPipe);


    const int STACK_SIZE=1024*1024;// 1MB stack size for the child process
    std::vector<char> childStack(STACK_SIZE);

    // launch a child process in a linux namespace

    /*
    CLONE_NEWPID- PID namespace (PID of child is 1), it cannot see other processes
    CLONE_NEWNS- mount namespace (gives child its own mount table, system's mount table is untouched)
    CLONE_NEWNET- network namespace (child has no connection to the newtork interfaces)
    SIGCHILD- signal to the parent for cleanup
    */

    childPID=clone(childFunction,childStack.data()+STACK_SIZE,CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | SIGCHLD,this);
    close(seccompPipe[1]);
    close(syncPipe[0]);

    if(childPID==-1)
    {
        std::cerr<<"Couldn't create a Sandbox for child process: "<<strerror(errno)<<"\n";
        return false;
    }

    // PID has been assigned, child process is running with childPID
    std::cout<<"Got PIDD: "<<childPID<<"\n";

    /*
    // set up UID and GID mappings to the child process 

    int mp=open(std::string("/proc/"+std::to_string(childPID)+"/uid_map").c_str(),O_WRONLY);

    if(mp==-1)
    {
        std::cerr<<"open failed: "<<strerror(errno)<<"\n";
        return false;
    }

    write(mp,"0 1000 1",8);
    close(mp);

    mp=open(std::string("/proc/"+std::to_string(childPID)+"/setgroups").c_str(),O_WRONLY);

    if(mp==-1)
    {
        std::cerr<<"open failed: "<<strerror(errno)<<"\n";
        return false;
    }

    write(mp,"deny",4);
    close(mp);

    mp=open(std::string("/proc/"+std::to_string(childPID)+"/gid_map").c_str(),O_WRONLY);

    if(mp==-1)
    {
        std::cerr<<"open failed: "<<strerror(errno)<<"\n";
        return false;
    }

    write(mp,"0 1000 1",8);
    close(mp);
    */

    // create cgroup for this process
    std::string cgroup="/sys/fs/cgroup/fragarach/";

    // std::cout<<getChildPID()<<"\n";
    mkdir(std::string(cgroup+std::to_string(getChildPID())).c_str(),0755);

    int fd=open((std::string(cgroup)+std::to_string(getChildPID())+"/memory.max").c_str(),O_WRONLY);

    if(fd==-1)
    {
        std::cerr<<"open failed: "<<strerror(errno)<<"\n";
        return false;
    }

    std::string data=std::to_string(getMemLimit());
    write(fd,data.c_str(),data.size());
    close(fd);

    fd=open((std::string(cgroup)+std::to_string(getChildPID())+"/pids.max").c_str(),O_WRONLY);

    if(fd==-1)
    {
        std::cerr<<"open failed: "<<strerror(errno)<<"\n";
        return false;
    }

    data=std::to_string(getPIDLimit());
    write(fd,data.c_str(),data.size());
    close(fd);

    fd=open((std::string(cgroup)+std::to_string(getChildPID())+"/cgroup.procs").c_str(),O_WRONLY);

    if(fd==-1)
    {
        std::cerr<<"open failed: "<<strerror(errno)<<"\n";
        return false;
    }

    data=std::to_string(getChildPID());
    write(fd,data.c_str(),data.size());
    close(fd);

    fd=open((std::string(cgroup)+std::to_string(getChildPID())+"/cpu.max").c_str(),O_WRONLY);

    if(fd==-1)
    {
        std::cerr<<"open failed: "<<strerror(errno)<<"\n";
        return false;
    }

    data="50000 100000";
    write(fd,data.c_str(),data.size());
    close(fd);

    return true;
}

bool Sandbox::isRunning()
{
    int status;
    int ret = waitpid(childPID, &status, WNOHANG);
    if(ret == -1) return false;
    if(ret == 0) return true;
    return !WIFEXITED(status) && !WIFSIGNALED(status);
}


static int childFunction(void* arg)
{
    

    // this function will run inside the child process, which will be complete clone of the parent

    // get the original sandbox object (passed into clone() earlier in Sandbox::launch())
    Sandbox* sb=static_cast<Sandbox*>(arg);

    close(sb->seccompPipe[0]);

    close(sb->syncPipe[1]); 

    // unmount the filesystem at merged (safety cleanup incase previous overlay crashed or something)
    // MNT_DETACH- detach from tree immediately
    umount2(merged,MNT_DETACH);
    std::string opts="lowerdir="+std::string(lower)+",upperdir="+std::string(upper)+",workdir="+std::string(work);

    // create an overlay filesystem
    // writes go to upper only
    // lower is untouched
    // merged is a view of upper and lower
    if(mount("overlay",merged,"overlay",0,opts.c_str()))
    {
        std::cerr<<"Overlay mount failed: "<<strerror(errno)<<"\n";
        return 1;
    }

    // remounts / and all other mountpts under it as private
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

    // cd to merged
    if(chdir(merged))
    {
        std::cerr<<"chdir failed: "<<strerror(errno)<<"\n";
        return 1;
    }

    // pivot root to current dir (merged), now the child has its root in merged overlayFS
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

    if(mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr))
    {
        std::cerr << "proc mount failed: " << strerror(errno) << "\n";
        return 1;
    }

    if(umount2(".",MNT_DETACH))
    {
        std::cerr<<"umount failed: "<<strerror(errno)<<"\n";
    }

    const char* path = "/target";// target is the binary we want to run

    if(access("/target",F_OK))
    {
        std::cerr<<"Yu are kooked lil bro\n";
        return 1;
    }
    else std::cerr<<"target exists\n";

    char* argv[] = {
        const_cast<char*>(path),
        nullptr
    };

    char* envp[] = {
        const_cast<char*>("PATH=/usr/bin:/bin"),
        const_cast<char*>("HOME=/root"),
        nullptr
    };

    // replace the child process by the binary process
    // system("ls");

    // drop capabilities to target, it can try to do bad things but it can't
    cap_t caps=cap_get_proc();
    cap_clear(caps);
    cap_set_proc(caps);
    cap_free(caps);

    // create seccomp context
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    // rules for dangerous calls
    seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(init_module), 0);
    seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(kexec_load), 0);
    seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(ptrace), 0);
    seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(reboot), 0);
    seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(setuid), 0);

    // load into the kernel 
    if(seccomp_load(ctx)<0)
    {
        std::cerr<<"Failed to load Seccomp context into kernel!!!\n";
        return 1;
    }    
    // get notify id
    int notifyFd=seccomp_notify_fd(ctx);
    std::cout<<notifyFd<<"\n";
    // free context
    seccomp_release(ctx);

    if(!is_fd_open(sb->seccompPipe[1])) std::cout<<"COOKED\n";
    //write notifyfd to the pipe
    write(sb->seccompPipe[1],&notifyFd,sizeof(notifyFd));
    // std::cerr<<strerror(errno)<<"\n";
    close(sb->seccompPipe[1]);

    char buf;
    std::cout<<"Child trying to get a start signal...\n";
    read(sb->syncPipe[0], &buf, 1); // will hang until syncPipe[0] is written to
    std::cout<<"got "<<buf<<"\n";
    close(sb->syncPipe[0]);

    std::cout<<"Started binary\n";
    execve(path,argv,envp);

    std::cerr<<"Failed to replace child process with target process: "<<strerror(errno)<<"\n";
    return 1;   
}

void Sandbox::cleanup()
{
    int status;
    // kill child
    kill(childPID,SIGKILL);
    // collect dead child
    waitpid(childPID,&status,0);
    rmdir(std::string("/sys/fs/cgroup/fragarach/"+std::to_string(childPID)).c_str());
    resetOverlay();
}
