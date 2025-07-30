#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <mutex>
#include <thread>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>

std::mutex mtx;
std::vector<std::pair<pid_t, std::string>> pids;
char threadRunning = 1;
std::thread pidThr;
int pidParent;

void safeExit(int sig)
{
    mtx.lock();
    for(auto x : pids)
        kill(x.first, 9);
    pids.clear();
    threadRunning = 0;
    mtx.unlock();
    pidThr.join();

    if(sig == SIGINT)
    {
        std::cout << "\nSIGINT - safely terminated execution\n";
        _exit(0);
    }

    if(sig != -1)
        exit(0);
}

void forkWaiter()
{
    std::vector<std::pair<pid_t, std::string>> pids_c;
    while(threadRunning)
    {        
        mtx.lock();
        pids_c = pids;
        mtx.unlock();
        std::pair<pid_t, std::string> *val = NULL;
        for(auto it = pids_c.begin(); it != pids_c.end();)
            if(waitpid(it.base()->first, NULL, WNOHANG) > 0)
            {
                val = it.base();
                break;
            }
        if(val)
        {
            mtx.lock();
            for(auto it = pids.begin(); it != pids.end();)
            {
                if(it.base()->first == val->first)
                    it = pids.erase(it);
                else
                    it++;
            }
            mtx.unlock();
        }
        usleep(100000);
    }
}

#define unmasked(i) (i == SIGKILL || i == SIGSTOP || i == SIGCONT || i == SIGTSTP)

void ignoreSignals()
{
    for(int i = 0; i < 32; i++, i += unmasked(i))
        signal(i, SIG_IGN);
}

void restoreSignals()
{
    for(int i = 0; i < 32; i++, i += i == SIGINT)
        signal(i, SIG_DFL);
    signal(SIGINT, safeExit);
}

char** vtoppc(std::vector<std::string> &src, int start, int end)
{
    char **ret = new char*[end - start + 1];
    for(int i = 0, j = start; i < end - start + 1; i++, j++)
    {
        ret[i] = new char[src[j].size() + 1];
        strcpy(ret[i], src[j].c_str());
    }
    ret[end - start + 1] = NULL;
    return ret;
}

void startBackground(std::vector<std::string> &command)
{
    int pidF = fork();
    if(pidF == -1)
    {
        std::cerr << "Fork : unable to start fork process\n";
        return;
    }
    if(pidF != 0)
    {
        mtx.lock();
        pids.push_back({pidF, command[0]});
        mtx.unlock();
        return;
    }

    char **args = vtoppc(command, 0, command.size() - 2);
    
    if(setpgid(0, 0) == -1)
    {
        std::cout << "setpgid : FATAL ERROR - unable to create new process group";
        _exit(1);
    }

    execvp(command[0].c_str(), args);
    std::cerr << "'"<< command[0] << "' is not recognized as a command or executable\n";
    _exit(1);
}

void startForeground(std::vector<std::string> &command)
{   
    int pidF = fork();
    if(pidF == -1)
    {
        std::cerr << "Fork : unable to start fork process\n";
        return;
    }
    if(pidF == 0)
    {
        char **args = vtoppc(command, 0, command.size() - 1);
        execvp(args[0], args);
        std::cerr << "'"<< command[0] << "' is not recognized as a command or executable\n";
        _exit(1);
    }
    ignoreSignals();
    waitpid(pidF, NULL, 0);
    restoreSignals();
}

std::vector<std::string> splitString(std::string str, char c)
{
    std::vector<std::string> ret;
    std::stringstream sstream(str);
    std::string temp = "";

    int s = str.length();
    
    //while(std::getline(sstream, temp, c))
    //    ret.push_back(temp);

    for(int i = 0; i < s; i++)
    {
        if(str[i] == c)
        {
            ret.push_back(temp);
            temp = "";
        }
        else if(str[i] == '\"')
        {
            i++;
            while(str[i] != '\"')
                temp += str[i++];
            ret.push_back(temp);
            temp = "";
            continue;
        }
        else
            temp += str[i];
    }
    ret.push_back(temp);

    if(ret.size() > 1)
    {
        auto it = ret.begin();
        while(it != ret.end())
        {
            if(it.base()->empty())
                it = ret.erase(it);
            else
                it++;
        }
    }

    return ret;
}

std::string fromBackDelim(std::string str, char c)
{
    int s = str.length(), i;
    for(i = s - 1; s >= 0 && str[i] != c; i--);
    return str.substr(i + 1, s - i);
}

void handleProg(std::vector<std::string> command)
{
    if(command[command.size() - 1] == "&")
        startBackground(command);
    else
        startForeground(command);
}

int main()
{
    pidParent = getpid();
    pidThr = std::thread(forkWaiter);

    signal(SIGINT, safeExit);

    std::string com;
    char _chBuff[1024];

    std::cout << "Terminal initialised\n";

    while(1)
    {
        std::cout << getcwd(_chBuff, 255) << ">";

        std::getline(std::cin, com);

        if(com.empty()) continue;
        
        std::vector<std::string> command = splitString(com, ' ');
        if(command[0] == "exit")
            break;
        else if(com[0] == 'c' && com[1] == 'd' && (com[2] == '.' || com[2] == ' '))
        {
            if(command.size() < 2 && command[0].length() == 2)
            {
                std::cout << getcwd(_chBuff, 1024) << "\n\n";
                continue;
            }
            else if(command.size() < 2 && command[0][2] == '.')
            {
                std::string _tSplit = command[0];
                command.clear();
                command.push_back(_tSplit.substr(0, 2));
                command.push_back(_tSplit.substr(2, _tSplit.length() - 2));
            }
            else if(command[0].length() != 2)
            {
                handleProg(command);
                continue;
            }

            if(chdir(command[1].c_str()) == -1)
                std::cout << "cd : Cannot find path '" << getcwd(_chBuff, 1024) << "/" << command[1] << "' because it does not exist.\n";
        }
        else if(command[0] == "ps")
        {
            std::cout << "PID\time\n";
            std::vector<std::pair<pid_t, std::string>> pids_c;
            
            mtx.lock();
            pids_c = pids;
            mtx.unlock();

            for(auto x : pids_c)
                std::cout << x.first << "\t" << fromBackDelim(x.second, '/') << "\n";
        }
        else if(command[0] == "kill")
        {
            if(command.size() == 1)
                std::cout << "kill : usage: kill [PID] [SIGNUM]\n";
            if(command.size() != 3)
                std::cout << "kill : incorrect number of arguments, syntax: kill [PID] [SIGNUM]\n";
            char flag = 0;
            pid_t pidDst = std::stoi(command[1]);
            for(auto x : pids)  
                if(x.first == pidDst)
                    flag = 1;
            if(flag)
                kill(pidDst, std::stoi(command[2]));
            else
                std::cout << "kill : (" << pidDst << ") - unable to find process";
        }
        else
            handleProg(command);
    }

    safeExit(-1);

    return 0;
}