#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <algorithm>
#include <regex>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <time.h>
#include <algorithm>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY() \
    cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() \
    cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

string _ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s)
{
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args)
{
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;)
    {
        args[i] = (char *)malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line)
{
    const string str(cmd_line);
    size_t last_char_idx = str.find_last_not_of(WHITESPACE);

    //Check if a non-whitespace character was found
    if (last_char_idx == std::string::npos)
    {
        return false; // Empty or all-whitespace string
    }

    return str[last_char_idx] == '&';
}

void free_args(char **args, int num_of_args)
{
    for (int i = 0; i < num_of_args || args[i] != nullptr; i++)
    {
        free(args[i]);
    }
    // free(args); //need to think
}

void _removeBackgroundSign(char *cmd_line)
{
    const string str(cmd_line);
    unsigned int idx = str.find_last_not_of(WHITESPACE);

    if (idx == string::npos)
    {
        return;
    }
    if (cmd_line[idx] != '&')
    {
        return;
    }

    // 1. Find the last real character *before* the '&'
    size_t end_of_cmd = str.find_last_not_of(WHITESPACE, idx - 1);

    if (end_of_cmd == std::string::npos)
    {
        // The command was just "&" or "& "
        cmd_line[0] = 0; 
    }
    else
    {
        // 2. Truncate the string right after that character
        cmd_line[end_of_cmd + 1] = 0;
    }
}

char **init_args()
{
    char **args = (char **)malloc(COMMAND_MAX_ARGS * sizeof(char **));
    if (!args)
    {
        return nullptr;
    }
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
    {
        args[i] = nullptr;
    }
    return args;
}

bool is_legit_num(const string &s)
{
    auto it = s.begin();
    for (; it != s.end() && std::isdigit(*it); it++)
    {
    }
    return it == s.end();
}

bool extract_signal_number(char *input, int &signum)
{
    if (!input || strlen(input) < 2 || input[0] != '-')
    {
        return false;
    }
    string temp = (string(input)).erase(0, 1);

    if (!is_legit_num(temp))
    {
        return false;
    }
    signum = stoi(temp);

    if (signum > 31)
        return false;

    return true;
}


string Command::getCommandS()
{
    if (!alias_name.empty())
    {
        return alias_name;
    }
    return cmd_line;
}

string ExternalCommand::getCommandS()
{
    if (!alias_name.empty())
    {
        return alias_name;
    }
    return full_cmd;
}

bool Command::hasAlias()
{
    return !alias_name.empty();
}

void Command::setAlias(string command)
{
    alias_name = command;
}

string SmallShell::getPrompt() const
{
    return prompt;
}

// JobsList SmallShell::jobList;

SmallShell::SmallShell() : aliasMap(), aliasCreationOrder(), prompt("smash"), current_process(-1), prevWorkingDir(nullptr),
                           jobList(new JobsList()), commands(), pid(getpid())
{
    createCommandVector();
    /*    map<string,string> aliasMap;
        vector<string> sortedAlias;
        string prompt;
        pid_t current_process;
        //string currWorkingDir;
        char* prevWorkingDir;
        //static JobsList jobList;
        JobsList* jobList;
        vector<string> commands;*/
}

SmallShell::~SmallShell()
{
    if (prevWorkingDir)
        free(prevWorkingDir);
    // should we do smth else?
    delete jobList;
}

/**
 * Creates and returns a pointer to Command class which matches the given command line (cmd_line)
 */
Command *SmallShell::CreateCommand(char *cmd_line)
{

    std::string cmd_s = _trim(std::string(cmd_line));
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    // Pipe command
    if (strstr(cmd_line, "|&"))
    {
        return new PipeCommand(cmd_line, PipeCommand::STDERR);
    }
    else if (strstr(cmd_line, "|"))
    {
        return new PipeCommand(cmd_line, PipeCommand::STDOUT);
    }

    // Redirection command - CHECK BEFORE built-in commands!
    if (strstr(cmd_line, ">>"))
    {
        return new RedirectionCommand(cmd_line, RedirectionCommand::CONCAT);
    }
    else if (strstr(cmd_line, ">"))
    {
        return new RedirectionCommand(cmd_line, RedirectionCommand::TRUNCATE);
    }

    // Complex external command
    if (strstr(cmd_line, "*") || strstr(cmd_line, "?"))
    {
        return new ComplexExternalCommand(cmd_line);
    }
    // check if the command first word ends with &
    if (firstWord.back() == '&')
    {
        firstWord.pop_back();
    }


    if (firstWord == "showpid")
    {
        return new ShowPidCommand(cmd_line);
    } else if (firstWord == "chprompt") {
        return new ChpromptCommand(cmd_line);
    } else if (firstWord == "fg") {
        return new FGCommand(cmd_line);
    } else if (firstWord == "kill") {
        return new KillCommand(cmd_line);
    } else if (firstWord == "alias") {
        return new AliasCommand(cmd_line);
    } else if (firstWord == "unalias") {
        return new UnaliasCommand(cmd_line);
    } else if (firstWord == "unsetenv") {
        return new UnsetenvCommand(cmd_line);
    } else if (firstWord == "sysinfo") {
        return new SysinfoCommand(cmd_line);
    } else if (firstWord == "whoami") {
        return new WhoAmICommand(cmd_line);
    } else if (firstWord == "du") {
        return new DiskUsageCommand(cmd_line);
    }

    // if nothing else is matched, we treat as external command.
    return new ExternalCommand(cmd_line);
    // meow

    return nullptr;
}

void SmallShell::executeCommand(char *cmd_line)
{
    // remove finished jobs
    jobList->removeFinishedJobs();

    string cmd_s = _trim(std::string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    // Check if the command name is an alias
    if (!aliasMap.empty() && aliasMap.find(firstWord) != aliasMap.end())
    {
        // Get the command that the alias maps to
        std::string aliasCommand = aliasMap[firstWord];

        // Get any arguments that followed the alias
        string args = "";
        size_t spacePos = cmd_s.find_first_of(" \n");
        if (spacePos != string::npos)
        {
            args = cmd_s.substr(spacePos);
        }

        // Create the new command by substituting the alias
        string newCmd = aliasCommand + args;

        // Execute the substituted command
        Command *cmd = CreateCommand(const_cast<char *>(newCmd.c_str()));
        if (cmd == nullptr)
        {
            return;
        }
        // Set the alias name so the command knows its original form
        cmd->setAlias(cmd_line);
        cmd->execute();
        delete cmd;
    }
    else
    {
        // Not an alias
        Command *cmd = CreateCommand(cmd_line);
        if (cmd == nullptr)
        {
            return;
        }
        cmd->execute();
        delete cmd;
    }
}

void SmallShell::setPrompt(string newPrompt)
{
    prompt = newPrompt;
}

// helper function to remove background sign from string and not char*
void removeBackgroundSignFromString(std::string &cmd_line)
{
    char *char_cmd = new char[cmd_line.length() + 1];
    strcpy(char_cmd, cmd_line.c_str());

    _removeBackgroundSign(char_cmd);

    cmd_line = std::string(char_cmd);
    delete[] char_cmd;
}

/////////////////////////////--------------Job List implementation-------//////////////////////////////

JobsList::JobsList() : jobsList(), job_map(), max_id(-1) {}

void JobsList::printJobsList()
{
    removeFinishedJobs();

    jobsList.sort();

    for (JobsList::JobEntry *job : jobsList)
    {
        cout << "[" << job->jobId << "] " << job->command << endl; // TODO initialise job->command to have the name
    }
}

bool isFinished(JobsList::JobEntry *job)
{
    if (job == nullptr || job->cmd == NULL)
    {
        return true;
    }

    int status;
    pid_t result = waitpid(job->pid, &status, WNOHANG);

    if (result > 0)
    {
        // Process finished, interpret exit status if needed
        return true;
    }
    else if (result == 0)
    {
        // Process is still running
        return false;
    }
    else
    {
        // Error occurred (result is -1)
        if (errno == ECHILD)
        {
            // No child process exists - consider it finished
            return true;
        }
        else
        {
            // Other error
            perror("waitpid");
            return false; // Safer to assume it's still running TODO check
        }
    }
}

void JobsList::removeFinishedJobs()
{
    for (auto it = jobsList.begin(); it != jobsList.end();)
    {
        if (isFinished(*it) && (*it)->cmd != NULL)
        {
            job_map.erase((*it)->jobId);
            auto temp_it = it;
            it = jobsList.erase(it); // erase returns the next valid iterator
            if (max_id == (*temp_it)->jobId)
            {
                jobsList.sort();
                max_id = jobsList.empty() ? -1 : jobsList.back()->jobId;
            }
        }
        else
        {
            ++it;
        }
    }
}

void JobsList::printJobsBeforeQuit()
{
    // remove finished jobs before printing the jobs.
    removeFinishedJobs();
    std::cout << "smash: sending SIGKILL signal to " << jobsList.size() << " jobs:" << std::endl;
    for (auto listIt = jobsList.begin(); listIt != jobsList.end(); ++listIt)
    {
        JobsList::JobEntry *job = *listIt;
        std::cout << job->pid << ": " << job->command << std::endl;
    }
} // check for correctness

void JobsList::killAllJobs()
{
    for (auto listIt = jobsList.begin(); listIt != jobsList.end(); ++listIt)
    {
        if (*listIt)
        { // Check for null pointers
            kill((*listIt)->pid, SIGKILL);
            delete *listIt;
        }
    }
    max_id = -1;
    jobsList.clear(); // check if we need to do clear
}

void JobsList::removeJobById(int jobId)
{
    if (job_map.find(jobId) == job_map.end())
    {
        cerr << "removeJobById error: no job with id = " << jobId << endl;
        return;
    }
    auto job = job_map[jobId];
    // Deletion from map
    job_map.erase(jobId);
    // Deletion from list
    for (auto it = jobsList.begin(); it != jobsList.end(); it++)
    {
        if ((*it)->jobId == jobId)
        {
            jobsList.erase(it);
            // Update maxId
            if (max_id == jobId)
            {
                jobsList.sort();
                max_id = jobsList.empty() ? -1 : jobsList.back()->jobId;
            }
            delete job;
            break;
        }
    }
}

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped)
{
    removeFinishedJobs();
    int newJobId = (max_id == -1) ? 1 : max_id + 1;

    if (newJobId > 100)
    {
        cerr << "addJob error: reached limit of processes" << endl;
        return;
    }
    JobEntry *job_to_insert = new JobEntry(cmd, isStopped, newJobId, pid, cmd->getCommandS()); // im not sure how to convert it to string correcrtly

    job_map[newJobId] = job_to_insert;
    jobsList.push_back(job_to_insert);

    max_id = newJobId;

} // need to check correctness

bool JobsList::JobEntry::operator<(const JobsList::JobEntry &other) const
{
    return this->jobId < other.jobId;
}

JobsList::JobEntry *JobsList::getJobById(int jobId)
{
    removeFinishedJobs();
    if (job_map.find(jobId) == job_map.end())
    {
        return nullptr;
    }
    return job_map[jobId];
}

JobsList *SmallShell::getJobs()
{
    return jobList;
}

void SmallShell::getAllAlias(vector<string> &aliases)
{
    for (auto element : aliasCreationOrder)
    {
        aliases.push_back(element + "='" + aliasMap[element] + "'");
    }
}

string SmallShell::getAlias(string name)
{
    if (aliasMap.find(name) == aliasMap.end())
    {
        return "";
    }
    else
    {
        return aliasMap[name];
    }
}

bool SmallShell::validCommand(string name)
{
    for (string command : commands)
    {
        if (command == name)
        {
            return true;
        }
    }
    return false;
}

// todo important to add this to constructor of smallshell
void SmallShell::createCommandVector()
{
    commands = {"chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit",
                "kill", "sysinfo", "usbinfo", "unalias", "alias", "unsetenv", "du"};
}

void SmallShell::setAlias(string name, string command)
{
    aliasMap[name] = command;
    aliasCreationOrder.push_back(name);
}

bool SmallShell::removeAlias(string name)
{

    // Check whether alias exist
    if (SmallShell::getInstance().getAlias(name) == "")
    {
        return false;
    }

    // Erase from map and vector
    aliasMap.erase(name);

    aliasCreationOrder.erase(find(aliasCreationOrder.begin(), aliasCreationOrder.end(), name));

    return true;
}

/////////////////////////////--------------Built-in commands-------//////////////////////////////

// Helper function to create segments vector from command line inorder to parse commands
void createSegments(char *cmd_line, vector<string> &segments)
{
    std::string temp = cmd_line;
    std::string toAdd;
    stringstream stringLine(temp);
    while (getline(stringLine, toAdd, ' '))
    {
        if (!toAdd.empty())
        {
            segments.push_back(toAdd);
        }
    }
}

BuiltInCommand::BuiltInCommand(char *cmd_line) : Command(cmd_line)
{
    // Ignore background command for built-in commands!!
    if (_isBackgroundComamnd(cmd_line))
    {
        _removeBackgroundSign(cmd_line);
    }
    createSegments(cmd_line, cmd_segments);
}

/* ShowPid command */
ShowPidCommand::ShowPidCommand(char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute()
{
    SmallShell &shell = SmallShell::getInstance();
    cout << "smash pid is " << shell.pid << endl;
    return;
}

ChpromptCommand::ChpromptCommand(char *cmd_line) : BuiltInCommand(cmd_line) {}

void ChpromptCommand::execute(){
    SmallShell &shell = SmallShell::getInstance();
    char *parsedArgs[COMMAND_MAX_ARGS] = {};
    int argsRes = _parseCommandLine(cmd_line, parsedArgs);
    if(argsRes == 1){
        shell.setPrompt("smash");
    } else {
        shell.setPrompt(parsedArgs[1]);
    }
    return;
}

FGCommand::FGCommand(char *cmd_line) : BuiltInCommand(cmd_line) {}

void FGCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    JobsList* jobs = shell.getJobs();

    // remove all jobs that finished.
    jobs->removeFinishedJobs();

    // first we need to extract second word in cmd_line to see if its a number.
    char **args = init_args();
    if (!args) {
        cerr << "smash error: malloc failed" << endl;
        return;
    }
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args > 2) {
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    // If there are no arguments, we bring the job with max id to the foreground.
    else if (num_of_args == 1) {
        if (jobs->max_id == -1) {
            cerr << "smash error: fg: jobs list is empty" << endl;
            return;
        }
        JobsList::JobEntry* job = jobs->getJobById(jobs->max_id);
        if (!job) {
            cerr << "smash error: fg: jobs list is empty" << endl;
        }
        // check if job is stopped
        if (job->isStopped) {
            if (kill(job->pid, SIGCONT) < 0) {
                cerr << "smash error: kill failed" << endl;
                free_args(args, num_of_args);
                return;
            }
        }
        cout << job->command << " " << job->pid << endl;
        jobs->removeJobById(job->jobId);
        shell.current_process = job->pid;

        // wait for the process to finish
        int status;
        if (waitpid(job->pid, &status, 0) == -1) {
            cerr << "smash error: waitpid failed" << endl;
            free_args(args, num_of_args);
            shell.current_process = -1;
            return;
        }
        shell.current_process = -1;
    }
    // got two arguments, need to find job with id argument.
    else {
        // check if second argument is a legit number
        string secondArg = string(args[1]);
        if (!is_legit_num(secondArg)) {
            cerr << "smash error: fg: invalid arguments" << endl;
            free_args(args, num_of_args);
            return;
        }
        int jobId = stoi(secondArg);
        JobsList::JobEntry* job = jobs->getJobById(jobId);
        if (!job) {
            cerr << "smash error: fg: job-id " << jobId << " does not exist" << endl;
            free_args(args, num_of_args);
            return;
        }
        // check if job is stopped
        if (job->isStopped) {
            if (kill(job->pid, SIGCONT) < 0) {
                cerr << "smash error: kill failed" << endl;
                free_args(args, num_of_args);
                return;
            }
        }
        cout << job->command << " " << job->pid << endl;
        jobs->removeJobById(job->jobId);
        shell.current_process = job->pid;
        // wait for the process to finish
        int status;
        if (waitpid(job->pid, &status, 0) == -1) {
            cerr << "smash error: waitpid failed" << endl;
            free_args(args, num_of_args);
            shell.current_process = -1;
            return;
        }
        shell.current_process = -1;
    }
    free_args(args, num_of_args);
}

KillCommand::KillCommand(char *cmd_line) : BuiltInCommand(cmd_line) {}

void KillCommand::execute() {
    char **args = init_args();
    SmallShell &shell = SmallShell::getInstance();
    if (!args) {
        cerr << "smash error: malloc failed" << endl;
        return;
    }
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args != 3) {
        cerr << "smash error: kill: invalid arguments" << endl;
    }
    int signum = 0;
    int jobId = 0;

    // check if jobId number is valid
    if (!is_legit_num(string(args[2]))) {
        cerr << "smash error: kill: invalid arguments" << endl;
        free_args(args, num_of_args);
        return;
    }
    jobId = stoi(string(args[2]));

    //check if signal number is valid
    if (!extract_signal_number(args[1], signum)) {
        cerr << "smash error: kill: invalid arguments" << endl;
        free_args(args, num_of_args);
        return;
    }

    JobsList::JobEntry* job = shell.getJobs()->getJobById(jobId);
    if (!job) {
        cerr << "smash error: kill: job-id " << jobId << " does not exist" << endl;
        free_args(args, num_of_args);
        return;
    }
    if (kill(job->pid, signum) < 0) {
        cerr << "smash error: kill failed" << endl;
        free_args(args, num_of_args);
        return;
    }

    cout << "signal " << signum << " was sent to pid " << job->pid << endl;
    if (signum == SIGSTOP) {
        job->isStopped = true;
    } else if (signum == SIGCONT) {
        job->isStopped = false;
    }
    free_args(args, num_of_args);
}

AliasCommand::AliasCommand(char *cmd_line) : BuiltInCommand(cmd_line) {}

void AliasCommand::execute() {
    // if command only alias, print all aliases.
    if (cmd_segments.size() == 1) {
        vector<string> aliases;
        SmallShell::getInstance().getAllAlias(aliases);
        for (const string &alias : aliases) {
            cout << alias << endl;
        }
        return;
    }

    // else, we create an alias for the command.
    string full_command = cmd_line;
    if (this->backGround)
    {
        removeBackgroundSignFromString(full_command);
    }
    // trim the command
    full_command = _trim(full_command);
    static const std::regex aliasPattern("^alias ([a-zA-Z0-9_]+)='([^']*)'$");
    std::smatch matches;
    bool matched = std::regex_search(full_command, matches, aliasPattern);

    if (!matched)
    {
        cerr << "smash error: alias: invalid alias format" << std::endl;
        return;
    }

    string aliasName = matches[1];
    string aliasCommand = matches[2];

    //check if alias is reserved command.
    if (SmallShell::getInstance().validCommand(aliasName))
    {
        cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
        return;
    }

    //check if alias already exists
    if (SmallShell::getInstance().getAlias(aliasName).compare("") != 0)
    {
        cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
        return;
    }

    SmallShell::getInstance().setAlias(aliasName, aliasCommand);
}

UnaliasCommand::UnaliasCommand(char *cmd_line) : BuiltInCommand(cmd_line) {}

void UnaliasCommand::execute() {
    char **args = init_args();
    if (!args) {
        cerr << "smash error: malloc failed" << endl;
        return;
    }
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args == 1) {
        cerr << "smash error: unalias: not enough arguments" << endl;
        free_args(args, num_of_args);
        return;
    }

    // try to remove aliases until the first not valid alias.
    for (int i = 1; i < num_of_args; i++) {
        string current_alias(args[i]);

        //check if alias even exists
        if (!SmallShell::getInstance().removeAlias(current_alias))
        {
            cerr << "smash error: unalias: " << current_alias << " alias does not exist" << endl;
            free_args(args, num_of_args);
            return;
        }
    }
    free_args(args, num_of_args);
}

UnsetenvCommand::UnsetenvCommand(char *cmd_line) : BuiltInCommand(cmd_line) {}

void UnsetenvCommand::execute() {
    // If no variable names were provided then error
    if (cmd_segments.size() < 2)
    {
        cerr << "smash error: unsetenv: not enough arguments" << endl;
        return;
    }

    // Check existence by reading /proc
    char proc_path[256];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/environ", getpid());

    int fd = open(proc_path, O_RDONLY);
    if (fd < 0)
    {
        perror("smash error: open failed");
        return;
    }

    char buffer[4096]; // Assuming 4KB is enough for env vars
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0)
    {
        // This case should be rare, but handle it
        perror("smash error: read failed");
        return;
    }
    buffer[bytes_read] = '\0'; // Ensure null termination

    // Check each requested var
    for (size_t i = 1; i < cmd_segments.size(); ++i)
    {
        const string &var = cmd_segments[i];
        const string prefix = var + "=";
        bool found = false;

        // Iterate through the NUL-separated buffer
        char *p = buffer;
        while (p < buffer + bytes_read)
        {
            if (strncmp(p, prefix.c_str(), prefix.size()) == 0)
            {
                found = true;
                break;
            }
            p += strlen(p) + 1; // Move to the next string
        }

        if (!found)
        {
            cerr << "smash error: unsetenv: " << var << " does not exist" << endl;
            return; // Stop at the first invalid occurrence
        }
    }

    extern char **environ;
    for (size_t i = 1; i < cmd_segments.size(); ++i)
    {
        const string &var = cmd_segments[i];
        const string prefix = var + "=";
        int idx = 0;

        while (environ[idx] != nullptr)
        {
            if (strncmp(environ[idx], prefix.c_str(), prefix.size()) == 0)
            {
                // Found at environ[idx], shift all subsequent entries
                int shift_idx = idx;
                do
                {
                    environ[shift_idx] = environ[shift_idx + 1];
                    ++shift_idx;
                } while (environ[shift_idx] != nullptr);

                break; // Move to the next variable
            }
            ++idx;
        }
    }
}

SysinfoCommand::SysinfoCommand(char *cmd_line) : BuiltInCommand(cmd_line) {}

void SysinfoCommand::execute()
{
    // 1. Get System, Hostname, Kernel, and Architecture
    struct utsname sys_info;
    if (uname(&sys_info) == -1)
    {
        perror("smash error: uname failed");
        return;
    }

    // 2. Get Boot Time from /proc/stat
    int fd = open("/proc/stat", O_RDONLY);
    if (fd < 0)
    {
        perror("smash error: open failed");
        return;
    }

    char buffer[8192]; // Buffer to read /proc/stat
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (bytes_read <= 0)
    {
        perror("smash error: read failed");
        return;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the buffer
    // Find the "btime" line
    char *btime_line = strstr(buffer, "btime ");
    if (btime_line == nullptr)
    {
        cerr << "smash error: could not parse /proc/stat for btime" << endl;
        return;
    }

    long unsigned btime_stamp;
    if (sscanf(btime_line, "btime %lu", &btime_stamp) != 1)
    {
        cerr << "smash error: could not parse btime value" << endl;
        return;
    }

    // Format the timestamp
    time_t boot_time = (time_t)btime_stamp;
    struct tm *tm_info = localtime(&boot_time);

    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 3. Print all information
    cout << "System: " << sys_info.sysname << endl;
    cout << "Hostname: " << sys_info.nodename << endl;
    cout << "Kernel: " << sys_info.release << endl;
    cout << "Architecture: " << sys_info.machine << endl;
    cout << "Boot Time: " << time_str << endl;
}

/////////////////////////////--------------External commands-------//////////////////////////////

ExternalCommand::ExternalCommand(char *cmd_line) : Command(cmd_line)
{
    // Store the original command line
    backGround = _isBackgroundComamnd(cmd_line);
    alias_name = ""; // Initialize alias_name

    // Create a clean version of the command for segments
    std::string clean_cmd_line = cmd_line;
    // todo check this
    if (backGround)
    {
        full_cmd = string(cmd_line);
        _removeBackgroundSign(cmd_line);
        // Create a modifiable copy of the command line
        char *cmd_copy = strdup(cmd_line);
        if (cmd_copy)
        {
            // Store the clean command segments
            createSegments(cmd_copy, cmd_segments);
            free(cmd_copy);
        }
    }
    else
    {
        // If not a background command, just use as is
        createSegments(cmd_line, cmd_segments);
    }
}

void ExternalCommand::execute()
{
    if (cmd_segments.empty())
    {
        return; // Nothing to execute
    }

    // Create a clean version of the command line for execution
    char *cmd_copy = strdup(cmd_line);
    if (!cmd_copy)
    {
        perror("smash error: memory allocation failed");
        return;
    }

    // If it's a background command, remove the & sign
    if (backGround)
    {
        _removeBackgroundSign(cmd_copy);
    }

    // Parse the cleaned command
    char **args = init_args();
    if (!args)
    {
        perror("smash error: memory allocation failed");
        free(cmd_copy);
        return;
    }

    int num_args = _parseCommandLine(cmd_copy, args);
    free(cmd_copy); // Done with the copy

    if (num_args == 0)
    {
        free_args(args, num_args);
        return;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        // Fork failed
        perror("smash error: fork failed");
        free_args(args, num_args);
        return;
    }

    if (pid == 0)
    {
        // Child process
        setpgrp(); // Set process group ID to dissociate from shell

        // Execute the command using execvp which searches in PATH
        execvp(args[0], args);

        // If execvp failed, report error and exit
        perror("smash error: execvp failed");
        free_args(args, num_args);
        exit(1);
    }
    else
    {
        // Parent process
        SmallShell &smash = SmallShell::getInstance();

        if (!backGround)
        {
            // Wait for child process to complete if not a background command
            smash.current_process = pid;
            if (waitpid(pid, nullptr, 0) == -1)
            {
                perror("smash error: waitpid failed");
            }
            smash.current_process = -1;
        }
        else
        {
            // Add job to jobs list if it's a background command
            smash.getJobs()->addJob(this, pid, false);
        }

        free_args(args, num_args);
    }
}

ComplexExternalCommand::ComplexExternalCommand(char *cmd_line) : Command(cmd_line), bash_args()
{
    backGround = _isBackgroundComamnd(cmd_line);

    if (backGround)
    {
        _removeBackgroundSign(cmd_line);
    }

    bash_args[0] = (char *)"/bin/bash";
    bash_args[1] = (char *)"-c";
    bash_args[2] = (char *)cmd_line;
    bash_args[3] = nullptr;
}

void ComplexExternalCommand::execute()
{
    pid_t pid = fork();

    if (pid == -1)
    {
        perror("smash error: fork failed");
        return;
    }
    // Child process
    if (pid == 0)
    {

        if (setpgrp() == -1)
        {
            perror("smash error: setpgrp failed");
            return;
        }

        execv("/bin/bash", bash_args);

        perror("smash error: execv failed");
        exit(1);
    }
    else
    {
        // Parent process
        SmallShell &smash = SmallShell::getInstance();

        if (!backGround)
        {
            smash.current_process = pid;
            int status;
            // Run in background
            if (waitpid(pid, &status, 0) == -1)
            {
                perror("smash error: waitpid failed");
                return;
            }
            smash.current_process = -1;
        }
        else
        {
            // Add to joblist
            smash.getJobs()->addJob(this, pid, false);
        }
    }
}

/////////////////////////////--------------Special commands-------//////////////////////////////
WhoAmICommand::WhoAmICommand(char *cmd_line) : Command(cmd_line){}

void WhoAmICommand::execute()
{
    // Get current userid and g
    uid_t uid = getuid();
    gid_t gid = getgid();

    std::string uid_str = std::to_string(uid);

    int fd = open("/etc/passwd", O_RDONLY);
    if (fd == -1)
    {
        std::cerr << "smash error: whoami: cannot open passwd file" << std::endl;
        return;
    }

    char buffer[4096] = {0};
    read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    char *line = strtok(buffer, "\n");
    while (line)
    {
        std::string entry(line);
        size_t pos = 0;
        std::string username;
        std::string home_dir;

        // Get username (field 1)
        pos = entry.find(':');
        if (pos == std::string::npos)
        {
            line = strtok(NULL, "\n");
            continue;
        }
        username = entry.substr(0, pos);

        // Skip password field (field 2)
        entry = entry.substr(pos + 1);
        pos = entry.find(':');
        if (pos == std::string::npos)
        {
            line = strtok(NULL, "\n");
            continue;
        }
        entry = entry.substr(pos + 1);

        // Get UID (field 3)
        pos = entry.find(':');
        if (pos == std::string::npos)
        {
            line = strtok(NULL, "\n");
            continue;
        }
        std::string uid_field = entry.substr(0, pos);
        if (uid_field == uid_str)
        {
            // Found our user, now get home directory
            entry = entry.substr(pos + 1);

            // Skip GID and GECOS fields (fields 4 and 5)
            for (int i = 0; i < 2; i++)
            {
                pos = entry.find(':');
                if (pos == std::string::npos)
                    break;
                entry = entry.substr(pos + 1);
            }

            // Get home directory (field 6)
            pos = entry.find(':');
            if (pos != std::string::npos)
            {
                home_dir = entry.substr(0, pos);

                // --- NEW OUTPUT FORMAT [cite: 423-425] ---
                std::cout << uid << std::endl;
                std::cout << gid << std::endl;
                std::cout << username << " " << home_dir << std::endl;
                // --- END OF NEW FORMAT ---

                return; // Found user, we are done
            }
        }

        line = strtok(NULL, "\n");
    }

    std::cerr << "smash error: whoami: user not found" << std::endl;
}

PipeCommand::PipeCommand(char *cmd_line, Type command_type) : Command(cmd_line), command_type(command_type)
{
    string cmd1, cmd2;
    size_t index;
    string input(cmd_line);
    // Look for the delimiter
    if (command_type == STDOUT)
    {
        index = input.find('|');
    }
    else
    {
        index = input.find("|&");
    }

    // Retrieving the first command
    cmd1 = _trim(input.substr(0, index));
    command1 = (char *)malloc(sizeof(char) * (cmd1.length() + 1));

    // Check whether malloc succeed
    if (!command1)
    {
        perror("smash error: malloc failed");
        throw bad_alloc();
    }

    strcpy(command1, cmd1.c_str());

    // Retrieving the second command

    cmd2 = _trim(input.substr(index + command_type));
    command2 = (char *)malloc(sizeof(char) * (cmd2.length() + 1));

    // Check whether malloc succeed
    if (!command2)
    {
        perror("smash error: malloc failed");
        free(command1);
        throw bad_alloc();
    }

    strcpy(command2, cmd2.c_str());
}

void PipeCommand::execute()
{
    int fd[2];
    if (pipe(fd) == -1)
    {
        perror("smash error: pipe failed");
        return;
    }

    pid_t pid1 = fork();
    // Check whether fork succeed
    if (pid1 == -1)
    {
        perror("smash error: fork failed");
        close_pipe(fd);
        return;
    }

    // Writer process
    if (pid1 == 0)
    {
        // Set gid to pid
        if (setpgrp() == -1)
        {
            perror("smash error: setpgrp failed");
            close_pipe(fd);
            exit(1);
        }
        // Redirect output
        if (command_type == STDOUT)
        {
            if (dup2(fd[1], 1) == -1)
            {
                perror("smash error: dup2 failed");
                close_pipe(fd);
                exit(1);
            }
        }
        else
        {
            if (dup2(fd[1], 2) == -1)
            {
                perror("smash error: dup2 failed");
                close_pipe(fd);
                exit(1);
            }
        }
        if (!close_pipe(fd))
            exit(1);
        SmallShell::getInstance().executeCommand(command1);
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == -1)
    {
        perror("smash error: fork failed");
        close_pipe(fd);
        return;
    }

    // Reader process
    if (pid2 == 0)
    {
        // Change gid to pid
        if (setpgrp() == -1)
        {
            perror("smash error: setpgrp failed");
            close_pipe(fd);
            exit(1);
        }

        // Redirect input
        if (dup2(fd[0], 0) == -1)
        {
            perror("smash error: dup2 failed");
            close_pipe(fd);
            exit(1);
        }
        if (!close_pipe(fd))
            exit(1);
        SmallShell::getInstance().executeCommand(command2);
        exit(1);
    }

    // Closing pipe
    if (!close_pipe(fd))
        return;

    // Wait child processes

    if (waitpid(pid1, nullptr, WUNTRACED) == -1)
    {
        perror("smash error: waitpid failed");
        return;
    }

    if (waitpid(pid2, nullptr, WUNTRACED) == -1)
    {
        perror("smash error: waitpid failed");
        return;
    }
}

bool PipeCommand::close_pipe(int *fd)
{
    bool ret = true;
    if (close(fd[0]) == -1)
    {
        perror("smash error: close failed");
        ret = false;
    }
    if (close(fd[1]) == -1)
    {
        perror("smash error: close failed");
        ret = false;
    }
    return ret;
} // Helper function to close both file descriptors of the pipe

PipeCommand::~PipeCommand()
{
    free(command1);
    free(command2);
}

RedirectionCommand::RedirectionCommand(char *cmd_line, command_type type) : Command(cmd_line), type(type)
{
    string cmd;
    string file;
    size_t index = 0;
    string input(cmd_line);
    if (type == CONCAT)
    {
        index = input.find(">>");
    }
    else
    {
        index = input.find('>');
    }

    // Parse command
    cmd = _trim(input.substr(0, index));
    command = (char *)malloc(sizeof(char) * (cmd.length() + 1));

    // Check whether malloc succeed
    if (!command)
    {
        perror("smash error: malloc failed");
        throw bad_alloc();
    }

    strcpy(command, cmd.c_str());
    if (_isBackgroundComamnd(command))
    {
        _removeBackgroundSign(command);
    }

    // Parse path
    file = _trim(input.substr(index + type));
    file_name = (char *)malloc(sizeof(char) * (file.length() + 1));

    // Check whether malloc succeed
    if (!file_name)
    {
        free(command);
        perror("smash error: malloc failed");
        throw bad_alloc();
    }

    strcpy(file_name, file.c_str());
}

RedirectionCommand::~RedirectionCommand()
{
    free(command);
    free(file_name);
}

void RedirectionCommand::execute()
{

    int fd = 0;
    stdout_copy = dup(1);

    // Trying to open the file
    if (type == CONCAT)
    {
        fd = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
    }
    else
    {
        fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    // Check whether open succeed
    if (fd < 0)
    {
        perror("smash error: open failed");
        close(stdout_copy);
        return;
    }

    // Check whether redirection succeed
    if (dup2(fd, 1) == -1)
    {
        perror("smash error: dup2 failed");
        close(fd);
        close(stdout_copy);
        return;
    }

    // Command execution
    SmallShell::getInstance().executeCommand(command);

    // Restore redirection to stdout
    dup2(stdout_copy, 1);
    close(fd);
    close(stdout_copy);
}

DiskUsageCommand::DiskUsageCommand(char *cmd_line) : Command(cmd_line)
{
    createSegments(cmd_line, cmd_segments);
}

void DiskUsageCommand::execute()
{
    // Check number of arguments
    if (cmd_segments.size() > 2)
    {
        std::cerr << "smash error: du: too many arguments" << std::endl;
        return;
    }

    // Determine directory path
    const char *dir_path;
    if (cmd_segments.size() == 1)
    {
        // No path specified, use current directory
        dir_path = ".";
    }
    else
    {
        dir_path = cmd_segments[1].c_str();
    }

    // Check if directory exists
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        std::cerr << "smash error: du: directory " << dir_path << " does not exist" << std::endl;
        return;
    }
    closedir(dir);

    // Clear the inode tracking set before calculation
    counted_inodes.clear();

    // Calculate total size
    long size_in_bytes = calculate_dir_size(dir_path);
    if (size_in_bytes < 0)
    {
        std::cerr << "smash error: du: failed to calculate disk usage" << std::endl;
        return;
    }

    long kb_size = (size_in_bytes + 1023) / 1024; // Round up to nearest KB

    // Standard du format: just the size followed by the path
    cout << "Total disk usage: " << kb_size << " KB" << std::endl;
}