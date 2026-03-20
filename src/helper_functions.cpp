#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include<dirent.h>
#include<iostream>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<stdio.h>

void mkdirP(const std::string& path)
{
    for(int i=1;i<path.size();++i)
    {
        if(path[i]=='/')
        {
            mkdir(path.substr(0,i).c_str(),0755);
        }
    }

    mkdir(path.c_str(),0755);
}

void rmRF(const std::string& path)
{
    if(path=="") return;

    DIR* dir=opendir(path.c_str());

    if(dir==NULL)
    {
        std::cerr<<"Cannot open directory: "<<strerror(errno)<<"\n";
        return ;
    }

    struct dirent* ch=nullptr;

    while ((ch=readdir(dir)))
    {
        if(strcmp(ch->d_name, ".") == 0 || strcmp(ch->d_name, "..") == 0) continue;    
                
        struct stat st;
        std::string mod=path;
        if(mod[mod.size()-1]!='/') mod.push_back('/');
        mod.append(ch->d_name);

        lstat(mod.c_str(), &st);

        if(S_ISLNK(st.st_mode))
        {
            unlink(mod.c_str());
        }
        else if(S_ISREG(st.st_mode))
        {
            unlink(mod.c_str());
        }
        else if(S_ISDIR(st.st_mode))
        {
            
            rmRF(mod); // safe to recurse, we know it's a real directory
        }
    }

    closedir(dir);

    if(rmdir(path.c_str())) std::cerr<<"Error deleting directory: "<<strerror(errno)<<"\n";
}

int cpy(const std::string& from,const std::string& to)
{
    int file1 = open(from.c_str(),O_RDONLY);
    int file2= open(to.c_str(),O_WRONLY | O_CREAT | O_TRUNC, 0755);

    if(file1==-1 || file2==-1)
    {
        std::cerr<<"Couldn't open files for copying: "<<strerror(errno)<<"\n";
        if(file1!=-1) close(file1);
        if(file2!=-1) close(file2);
        return 1;
    }

    char buffer[4096];
    ssize_t bytesread;

    int exitcode=0;
    while (true)
    {
        if(exitcode) break;

        bytesread=read(file1,buffer,sizeof(buffer));

        if(bytesread==0) break;

        if(bytesread==-1) 
        {
            if(errno==EINTR) continue;
            std::cerr<<"Read error: "<<strerror(errno)<<"\n";
            exitcode=1;
            break;
        }

        char* ptr=buffer;

        while (bytesread)
        {
            ssize_t byteswrite = write(file2, ptr, bytesread);

            if(byteswrite <= 0) 
            {
                if (byteswrite == -1 && errno == EINTR) continue;
                std::cerr<<"Write error: "<<strerror(errno)<<"\n";
                exitcode=1;
                break;
            } 

            bytesread-=byteswrite;
            ptr+=byteswrite; 
        }
    }
    
    close(file1);
    close(file2);

    return exitcode;
}

int run(const char* path,char* const args[])
{
    pid_t pid=fork();

    if(pid==-1) return 1;

    if(pid==0)
    {
        // in the child copy
        execv(path, args);
        exit(1);// exit the child process immediately
    }
    else
    {
        // in parent
        int status;
        waitpid(pid,&status,0);

        if(WIFEXITED(status)) return WEXITSTATUS(status);

        return 1;
    }

    return 0;
}

int is_fd_open(int fd) {
    if (fcntl(fd, F_GETFD) == -1) {
        if (errno == EBADF) {
            return 0;
        }
    }
    return 1;
}
