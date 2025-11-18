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

ChpromptCommand::ChpromptCommand(char *cmd_line) : ChpromptCommand(cmd_line){}

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


/////////////////////////////--------------External commands-------//////////////////////////////

/// ExternalCommand class

/////////////////////////////--------------Special commands-------//////////////////////////////

