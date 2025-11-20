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
    char *getPrevWorkingDirectory() const;
    void setPrevWorkingDir(char* newDir);
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
};

class PwdCommand : public BuiltInCommand{
public:
    explicit PwdCommand(char *cmd_line);

    virtual ~PwdCommand() = default;

    void execute() override;
};

class CdCommand : public BuiltInCommand{
public:
    explicit CdCommand(char *cmd_line);

    virtual ~CdCommand() = default;

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

class JobsCommand : public BuiltInCommand {
public:
    explicit JobsCommand(char *cmd_line);
    virtual  ~JobsCommand() = default;
    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    explicit AliasCommand(char *cmd_line);

    virtual ~AliasCommand() = default;

    void execute() override;
};

class QuitCommand : public BuiltInCommand{
public:
    explicit QuitCommand(char *cmd_line);

    virtual ~QuitCommand() = default;

    void execute() override;
};

class UnaliasCommand : public BuiltInCommand {
public:
    explicit UnaliasCommand(char *cmd_line);
    virtual ~UnaliasCommand() = default;
    void execute() override;
};

class UnsetenvCommand : public BuiltInCommand {
public:
    explicit UnsetenvCommand(char *cmd_line);
    virtual ~UnsetenvCommand() = default;
    void execute() override;
};

class SysinfoCommand : public BuiltInCommand {
public:
    explicit SysinfoCommand(char *cmd_line);
    virtual ~SysinfoCommand() = default;
    void execute() override;
};


//////////////////////////////--------------External commands-------/////////////////////////////
class ExternalCommand : public Command
{
public:
    string full_cmd;

    ExternalCommand(char *cmd_line);

    virtual ~ExternalCommand(){}

    string getCommandS() override;

    void execute() override;
}; // DONE

class ComplexExternalCommand : public Command
{
public:
    char *bash_args[4];
    ComplexExternalCommand(char *cmd_line);

    virtual ~ComplexExternalCommand()
    {
    }

    void execute() override;
}; // DONE



/////////////////////////////--------------Special commands-------//////////////////////////////

class RedirectionCommand : public Command
{
    char *command;
    char *file_name;
    int stdout_copy;

public:
    enum command_type
    {
        TRUNCATE = 1,
        CONCAT = 2
    };

    command_type type;

    explicit RedirectionCommand(char *cmd_line, command_type type);

    virtual ~RedirectionCommand();

    void execute() override;
}; // DONE

class PipeCommand : public Command
{
    // TODO: Add your data members
public:
    char *command1;
    char *command2;

    enum Type
    {
        STDOUT = 1,
        STDERR = 2
    };

    Type command_type;

    PipeCommand(char *cmd_line, Type command_type);

    virtual ~PipeCommand();

    bool close_pipe(int *fd);

    void execute() override;
};

class WhoAmICommand : public Command
{
public:
    WhoAmICommand(char *cmd_line);
    virtual ~WhoAmICommand(){}
    void execute() override;
};


class DiskUsageCommand : public Command
{
private:
    std::unordered_set<ino_t> counted_inodes;

    long calculate_dir_size(const char *path)
    {
        struct stat st;
        if (lstat(path, &st) != 0)
        {
            perror("smash error: lstat failed");
            return 0; // can't stat this entry
        }

        // Only count each inode once (prevents double-counting hard links)
        if (counted_inodes.insert(st.st_ino).second == false)
        {
            return 0;
        }

        // Start with this entry's blocks
        long size = st.st_blocks * 512;

        // If it's a directory (and not a symlink) recurse into it
        if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode))
        {
            int fd = open(path, O_RDONLY | O_DIRECTORY);
            if (fd < 0)
            {
                perror("smash error: open failed");
                return size;
            }

            char buf[BUF_SIZE];
            long bytes_read;

            // Use getdents64 instead of readdir
            while ((bytes_read = syscall(SYS_getdents64, fd, buf, BUF_SIZE)) > 0)
            {
                for (long bpos = 0; bpos < bytes_read;)
                {
                    struct linux_dirent64 *d = (struct linux_dirent64 *)(buf + bpos);

                    if (strcmp(d->d_name, ".") == 0 ||
                        strcmp(d->d_name, "..") == 0)
                    {
                        bpos += d->d_reclen; // Move to next entry
                        continue;
                    }

                    char child_path[PATH_MAX];
                    snprintf(child_path, sizeof(child_path), "%s/%s", path, d->d_name);

                    // Recurse
                    size += calculate_dir_size(child_path);

                    bpos += d->d_reclen; // Move to next entry
                }
            }

            close(fd);

            if (bytes_read < 0)
            {
                perror("smash error: getdents64 failed");
            }
        }

        return size;
    }

public:
    DiskUsageCommand(char *cmd_line);

    virtual ~DiskUsageCommand()
    {
    }

    // execute() method in Commands.cpp does not need to be changed.
    void execute() override;
};

void removeBackgroundSignFromString(std::string &cmd_line);

#endif // SMASH_COMMAND_H_