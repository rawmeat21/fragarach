#pragma once
#include<string>

class Sandbox
{
public:

    Sandbox(std::string_view binaryPath);

    const std::string& getBinaryPath();
    const int getMemLimit();
    const pid_t getChildPID();
    const int getPIDLimit();
    const int getTimeoutTime();

    Sandbox& setTimeout(int seconds);
    Sandbox& setMemLimit(size_t bytes);


    void resetOverlay();
    bool launch();
    bool isRunning();
    void cleanup();
    int getNotifyFd();
    int syncPipe[2];
    int seccompPipe[2];
    
private:
    std::string binaryPath;
    int timeoutTime=60;
    size_t memLimit=512*1024*1024;
    int pidLimit=32;
    pid_t childPID=-1;
    
};

