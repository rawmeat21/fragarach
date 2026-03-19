#include<string>


class Sandbox
{
public:

    Sandbox(std::string_view binaryPath);

    const std::string& getBinaryPath();

    Sandbox& setTimeout(int seconds);
    Sandbox& setMemLimit(size_t bytes);
    void resetOverlay();
    bool launch();
    void cleanup();

private:
    std::string binaryPath;
    int timeoutTime=30;
    size_t memLimit=512*1024*1024;
    pid_t childPID=-1;
};

