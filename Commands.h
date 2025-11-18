// Ver: 10-4-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
using namespace std;
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>
#include <string>
#include <unordered_set>
#include <dirent.h>
#include <sys/stat.h>
#include <iostream>
#include <cstring>
#include <limits.h>
#include <time.h>
#include <algorithm>

#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

struct linux_dirent64
{
    ino64_t d_ino;           /* 64-bit inode number */
    off64_t d_off;           /* 64-bit offset to next structure */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char d_type;    /* File type */
    char d_name[];           /* Filename (null-terminated) */
};

#define BUF_SIZE 1024 * 32

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

/////////////////////////////--------------Class declarations-------//////////////////////////////

class Command
{
protected:
    bool backGround;
    vector<string> cmd_segments;
    const char *cmd_line;
    string alias_name; // Store the original alias name if this command came from an alias
public:
    explicit Command(char *cmd_line) : cmd_line(cmd_line), alias_name("") {};

    virtual ~Command() = default;

    virtual void execute() = 0;

    virtual string getCommandS();
    string printCommand();
    bool hasAlias();
    void setAlias(string command);
};

class JobsList
{
public:
    class JobEntry
    {
    public:
        Command *cmd;
        bool isStopped;
        int jobId;
        pid_t pid;
        string command;
        JobEntry(Command *cmd, bool isStopped, int jobId, int pid, string command) : cmd(cmd), isStopped(isStopped), jobId(jobId), pid(pid), command(command) {} // maybe pass by reference the command
        ~JobEntry() = default;                                                                                                                                   // maybe delete cmd?
        bool operator<(const JobEntry &other) const;
    };

    list<JobEntry *> jobsList;

    std::unordered_map<int, JobEntry *> job_map; // I think we need to add it for more efficient search

    int max_id;

    JobsList();

    ~JobsList() = default;

    void addJob(Command *cmd, pid_t pid, bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    void printJobsBeforeQuit();

    // TODO: Add extra methods or modify exisitng ones as needed
};

class SmallShell
{
private:
    map<string, string> aliasMap;
    vector<string> aliasCreationOrder;
    string prompt;
    char *prevWorkingDir;
    JobsList *jobList;
    vector<string> commands;
    SmallShell();

public:
    pid_t current_process;
    pid_t pid;
    Command *CreateCommand(char *cmd_line);

    SmallShell(SmallShell const &) = delete;     // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance()             // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(char *cmd_line);

    // TODO: add extra methods as needed
    void setPrompt(string newPrompt);
    string getPrompt() const;
    char *getCurrWorkingDir() const;
    void setCurrWorkingDir(string newDir);
    string getPrevWorkingDir() const;
    void setPrevWorkingDir(string newDir);
    JobsList *getJobs();
    void getAllAlias(std::vector<std::string> &aliases);
    string getAlias(string name);
    bool validCommand(string name);
    void createCommandVector();
    void setAlias(string name, string command);
    bool removeAlias(string name);
};

/////////////////////////////--------------Built-in commands-------//////////////////////////////

class BuiltInCommand : public Command
{
public:
    explicit BuiltInCommand(char *cmd_line);

    virtual ~BuiltInCommand()
    {
    }
};

class ShowPidCommand : public BuiltInCommand
{
public:
    explicit ShowPidCommand(char *cmd_line);

    virtual ~ShowPidCommand() = default;

    void execute() override;
}; // DONE

class ChpromptCommand : public BuiltInCommand{
public:
    explicit ChpromptCommand(char *cmd_line);

    virtual ~ChpromptCommand() = default;

    void execute() override;
}; // DONE

class FGCommand : public BuiltInCommand {
public:
    explicit FGCommand(char *cmd_line);

    virtual ~FGCommand() = default;

    void execute() override;
}; // DONE

class KillCommand : public BuiltInCommand {
public:
    explicit KillCommand(char *cmd_line);

    virtual ~KillCommand() = default;

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    explicit AliasCommand(char *cmd_line);

    virtual ~AliasCommand() = default;

    void execute() override;
};

//////////////////////////////--------------External commands-------/////////////////////////////




/////////////////////////////--------------Special commands-------//////////////////////////////



void removeBackgroundSignFromString(std::string &cmd_line);

#endif // SMASH_COMMAND_H_