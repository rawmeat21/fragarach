#pragma once
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string>
#include<dirent.h>
#include<iostream>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>

void mkdirP(const std::string& path);
void rmRF(const std::string& path);
int cpy(const std::string& from,const std::string& to);
int run(const char* path,char* const args[]);