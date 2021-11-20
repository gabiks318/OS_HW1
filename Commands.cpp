#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif


string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    // Remove whitespace
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    // Split into different arguments
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

void free_args(char **args, int arg_num) {

    for (int i = 0; i < arg_num; i++) {
        if (!args[i])
            free(args[i]);
    }
}


bool _isBackgroundCommand(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}


Command::Command(const char *cmd_line) : cmd_line(cmd_line) {}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {}




// TODO: Add your implementation for classes in Commands.h 

SmallShell::SmallShell() : smash_prompt("smash"), pid(getpid()), last_directory(nullptr), job_list(),
                           current_process(-1), current_cmd(nullptr) {

}

SmallShell::~SmallShell() {
    if (last_directory) {
        free(last_directory);
    }
}

void SmallShell::setLastDirectory(char *dir) {
    last_directory = dir;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // TODO: Error check for valid command
    // TODO: If background command, create job id and add to job list

    string cmd_s = _trim(string(cmd_line)); //REMOVE WHITESPACES
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (strstr(cmd_line, " > ") != NULL || strstr(cmd_line, " >> ") != NULL) {
        return new RedirectionCommand(cmd_line);
    }

    if (firstWord.compare("chprompt") == 0) {
        return new ChpromptCommand(cmd_line);
    } else if (firstWord.compare("showpid") == 0) {
        return new ShowPidCommand(cmd_line);
    } else if (firstWord.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd_line);
    } else if (firstWord.compare("cd") == 0) {
        return new ChangeDirCommand(cmd_line, &last_directory);
    } else if (firstWord.compare("jobs") == 0) {
        return new JobsCommand(cmd_line, &job_list);
    } else if (firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line, &job_list);
    } else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line, &job_list);
    } else if (firstWord.compare("bg") == 0) {
        return new BackgroundCommand(cmd_line, &job_list);
    } else if (firstWord.compare("quit") == 0) {
        return new QuitCommand(cmd_line, &job_list);
    } else {
        return new ExternalCommand(cmd_line);
    }

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {

    Command *cmd = CreateCommand(cmd_line);
    job_list.removeFinishedJobs();
    cmd->execute();
    cout << "Freeee 1" << endl;
    delete cmd;
    cout << "Freeee finished" << endl;
    current_process = -1;
    current_cmd = nullptr;
}
/////////////////////////////--------------Job List implementation-------//////////////////////////////

JobsList::JobEntry::JobEntry(int job_id, pid_t job_pid, time_t time_created, std::string command, bool isStopped)
        : job_id(job_id), job_pid(job_pid), time_created(time_created),
          command(command),
          isStopped(isStopped) {}

JobsList::JobsList() : job_list(), max_job_id(0) {
}

JobsList::~JobsList() {}

void JobsList::removeFinishedJobs() {
    if (job_list.empty()) {
        max_job_id = 0;
        return;
    }

// TODO: If there
    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        auto job = *it;
        int status;
        if (waitpid(job.job_pid, &status, WNOHANG) == job.job_pid) {
            job_list.erase(it);
            --it;
        }
    }

    int curr_max = 0;
    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        auto job = *it;
        if (job.job_id > curr_max) {
            curr_max = job.job_id;
        }
    }
    max_job_id = curr_max + 1;

}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        auto job = *it;
        if (job.job_id == jobId) {
            return &(*it);
        }
    }
    return nullptr;
}

void JobsList::killAllJobs() {
    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        auto job = *it;
        cout << job.job_pid << ": " << job.command << endl;
        kill(job.job_pid, SIGKILL);
    }
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    *lastJobId = max_job_id;
    return &job_list[job_list.size() - 1];
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    int max_stopped = -1;
    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        auto job = *it;
        if (job.isStopped) {
            max_stopped = job.job_id;
        }
    }
    *jobId = max_stopped;
    return &job_list[max_stopped];
}

void JobsList::removeJobById(int jobId) {
    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        auto job = *it;
        if (job.job_id == jobId) {
            job_list.erase(it);
            break;
        }
    }
}

void JobsList::addJob(Command *cmd, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    job_list.push_back(JobEntry(max_job_id, pid, time(nullptr), cmd->get_cmd_line(),
                                isStopped));
    max_job_id++;
}

//////////////////////////////-------------Built-in Commands-------------//////////////////////////////

/* Chprompt Command
  *chprompt command will allow the user to change the prompt displayed by the smash while waiting for the next command.
*/

ChpromptCommand::ChpromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
}

void ChpromptCommand::execute() {
    char **args = (char **) malloc(COMMAND_MAX_ARGS);
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    SmallShell &shell = SmallShell::getInstance();
    if (num_of_args == 1) {
        shell.smash_prompt = "smash";
    } else {
        shell.smash_prompt = args[1];
    }
    free_args(args, num_of_args);
    free(args);
}

/* ShowPidCommand
 * showpid command prints the smash pid.
 */

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    cout << "smash pid is " << shell.pid << endl;
}

/* PWD Command
 * pwd prints the full path of the current working directory. In the next command (cd command) will explain how
 * to change the current working directory
 */

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
    long max_size = pathconf(".", _PC_PATH_MAX);
    char *buffer = (char *) malloc((size_t) max_size);
    if (buffer != nullptr) {
        getcwd(buffer, (size_t) max_size);
//        if(!buffer){
//            // TODO: error handling
//        }
        cout << buffer << endl;
        free(buffer);
    }
}

/* CD Command
 *Change directory (cd) command receives a single argument <path> that describes the relative or full path to change the
 * current working directory to it. There is a special argument that cd can get which is “-“. If “-“ was specified as the
 * only argument of cdcommand then it will change the current working directory to the last working directory.
 */
ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) : BuiltInCommand(cmd_line),
                                                                            plastPwd(plastPwd) {};

void ChangeDirCommand::execute() {

// TODO: doesn't work with too many arguments

    char **args = (char **) malloc(COMMAND_MAX_ARGS);
    int num_of_args = _parseCommandLine(this->cmd_line, args);

    SmallShell &shell = SmallShell::getInstance();

    if (num_of_args > 2) {
        cerr << "smash error: cd: too many arguments" << endl;
    } else if (num_of_args == 1) {
        cerr << "smash error: cd: no arguments" << endl;
    } else {
        long max_size = pathconf(".", _PC_PATH_MAX);
        char *buffer = (char *) malloc((size_t) max_size);
        getcwd(buffer, (size_t) max_size);
        string next_dir = args[1];

        if (next_dir.compare("-") == 0) {
            if (!(*plastPwd)) {
                cerr << "smash error: cd: OLDPWD not set" << endl;
            } else {
                if (chdir(*plastPwd) != 0) {
                    // TODO: Error handling
                } else {
                    if (*plastPwd)
                        free(*plastPwd);
                    *plastPwd = buffer;
                }
            }
        } else {
            if (chdir(args[1]) != 0) {
                // TODO: Error handling
            } else {
                if (*plastPwd)
                    free(*plastPwd);
                *plastPwd = buffer;
            }
        }
    }
    free_args(args, num_of_args);
    if (args)
        free(args);
}

/* Jobs Command
 * jobs command prints the jobs list.
 */

JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void JobsCommand::execute() {
    jobs->removeFinishedJobs();
    for (auto it = jobs->job_list.begin(); it != jobs->job_list.end(); ++it) {
        auto job = *it;
        if (job.isStopped) {
            cout << "[" << job.job_id << "] " << job.command << ":" << job.job_pid
                 << " " << difftime(time(nullptr), job.time_created) << " secs (stopped)" << endl;
        } else {
            cout << "[" << job.job_id << "] " << job.command << ":" << job.job_pid
                 << " " << difftime(time(nullptr), job.time_created) << " secs" << endl;
        }
    }
}

/* Kill Command
 * Kill command sends a signal whose number is specified by <signum> to a job whose sequence ID in jobs list is <job-id>
 * (same as job-id in jobs command), and prints a message reporting that the specified signal was sent to the specified job
 */

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute() {
    char **args = (char **) malloc(COMMAND_MAX_ARGS);
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    if (num_of_args != 3) {
        cerr << "smash error: kill: invalid arguments" << endl;
    } else {
// TODO: Check arguments
        int job_id = stoi(args[2]);
        if (JobsList::JobEntry *job = jobs->getJobById(job_id)) {
            int job_pid = job->job_pid;
            int signum = stoi(string(args[1]).erase(0, 1));
            cout << "signal number " << signum << " was sent to pid " << job_pid << endl;
            kill(job_pid, signum);
            if (signum == SIGTSTP) {
                job->isStopped = true;
            } else if (signum == SIGCONT) {
                job->isStopped = false;
            }
        } else {
            cerr << "smash error: kill: job-id " << job_id << " does not exist" << endl;
        }
    }

    free_args(args, num_of_args);
    free(args);
}

/* FG Command
 * fg command brings a stopped process or a process that runs in the background to theforeground.
 */

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute() {
    char **args = (char **) malloc(COMMAND_MAX_ARGS);
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    SmallShell &smash = SmallShell::getInstance();
    if (num_of_args == 2) {
        int job_id = stoi(args[1]);
        if (job_id < 0) { //TODO: make sure this is correct
            cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
        } else {
            if (JobsList::JobEntry *job = jobs->getJobById(job_id)) {
                int job_pid = job->job_pid;
                if (job->isStopped) {
                    kill(job_pid, SIGCONT);
                }
                int status_p;
                cout << job->command << ":" << job_pid << endl;
                smash.current_process = job_pid;
                smash.current_cmd = this;
                jobs->removeJobById(job_id);

                waitpid(job_pid, &status_p, WUNTRACED);
            } else {
                cerr << "smash error: fg: job-id" << job_id << "does not exist" << endl;
            }
        }
    } else if (num_of_args == 1) {
        int lastJobId;
        JobsList::JobEntry *max_job = jobs->getLastJob(&lastJobId);
        if (lastJobId == 0) {
            cerr << "smash error: fg: jobs list is empty" << endl;
        } else {
            if (max_job->isStopped) {
                kill(max_job->job_pid, SIGCONT);
            }

            int status_p;
            jobs->removeJobById(lastJobId);
            cout << max_job->command << ":" << max_job->job_pid << endl;
            smash.current_process = max_job->job_pid;
            smash.current_cmd = this;
            waitpid(max_job->job_pid, &status_p, WUNTRACED);
        }
    } else {
        cerr << "smash error: fg: invalid arguments" << endl;
    }
}

/* BG Command
 * bg command resumes one of the stopped processes in the background.
 */

BackgroundCommand::BackgroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void BackgroundCommand::execute() {
    char **args = (char **) malloc(COMMAND_MAX_ARGS);
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    if (num_of_args == 2) {
        int job_id = stoi(args[1]);
        if (job_id < 0) { //TODO: make sure this is correct
            cerr << "smash error:bg:job-id " << job_id << " does not exist" << endl;
        } else {
            if (JobsList::JobEntry *job = jobs->getJobById(job_id)) {
                int job_pid = job->job_pid;
                if (!job->isStopped) {
                    cerr << "smash error:bg:job-id " << job_id << " is already running in the background" << endl;
                } else {
                    kill(job_pid, SIGCONT);
                    job->isStopped = false;
                    cout << job->command << ":" << job_pid << endl;
                }

            } else {
                cerr << "smash error:bg:job-id " << job_id << " does not exist" << endl;
            }
        }
    } else if (num_of_args == 1) {
        int lastJobId;
        JobsList::JobEntry *max_job = jobs->getLastStoppedJob(&lastJobId);
        if (lastJobId == -1) {
            cerr << "smash error: bg:  there is no stopped jobs to resume" << endl;
        } else {
            kill(max_job->job_pid, SIGCONT);
            max_job->isStopped = false;
            cout << max_job->command << ":" << max_job->job_pid << endl;
        }
    } else {
        cerr << "smash error: bg: invalid arguments" << endl;
    }
}

/* Quit Command
 * quit command exits the smash.
 */

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute() {
    char **args = (char **) malloc(COMMAND_MAX_ARGS);
    int num_of_args = _parseCommandLine(this->cmd_line, args);
    if (num_of_args == 2 && string(args[1]).compare("kill") == 0) {
        cout << "smash: sending SIGKILL signal to " << jobs->job_list.size() << " jobs:" << endl;
        jobs->killAllJobs();
    }
    exit(0);
}

//////////////////////////////-------------External Commands-------------//////////////////////////////

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
    char *cmd_line_copy = (char *) malloc(sizeof(char) * strlen(cmd_line));
    strcpy(cmd_line_copy, cmd_line);
    bool is_background = _isBackgroundCommand(cmd_line);
    if (is_background) {
        _removeBackgroundSign(cmd_line_copy);
    }
    SmallShell &smash = SmallShell::getInstance();
    char bash[] = "/bin/bash";
    char flag[] = "-c";
    char *args[] = {bash, flag, cmd_line_copy, nullptr};


    pid_t pid = fork();
    if (pid == 0) { //son
        setpgrp();
        execv(args[0], args);
        // TODO: error handling?
    } else { //father

        if (is_background) {
            smash.job_list.addJob(this, pid, false);
        } else {
            smash.current_process = pid;
            smash.current_cmd = this;
            int status;
            waitpid(pid, &status, WUNTRACED);
        }
    }
    free(cmd_line_copy);
}

/////////////////////////////--------------I/O,Pipe,Head,Alarm implementation-------//////////////////////////////

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {
// TODO: check what to do with more then 3 arguments
    char *cmd_line_copy = (char *) malloc(sizeof(char) * strlen(cmd_line));
    strcpy(cmd_line_copy, cmd_line);
    if (_isBackgroundCommand(cmd_line)) {
        _removeBackgroundSign(cmd_line_copy);
    }
    char **args = (char **) malloc(COMMAND_MAX_ARGS);
    int num_of_args = _parseCommandLine(cmd_line_copy, args);

    command = (char *) malloc(sizeof(char) * strlen(args[0]) + 1);
    strcpy(command, args[0]);
    filename = (char *) malloc(sizeof(char) * strlen(args[2]) + 1);
    strcpy(filename, args[2]);
    if(strcmp(args[1], ">>") == 0) {
        append = true;
    } else {
        append = false;
    }
    prepare();

    free_args(args, num_of_args);
    free(args);
    free(cmd_line_copy);
}

void RedirectionCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    shell.executeCommand(command);
}

void RedirectionCommand::prepare() {
    stdout_copy = dup(1);
    close(1);
    if(append)
        fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0666);
    else
        fd = open(filename, O_WRONLY | O_CREAT, 0666);

}

void RedirectionCommand::cleanup() {

    free(filename);
    cout << "Freeee 3" << endl;
    free(command);
    cout << "Freeee 4" << endl;
//    close(fd);
    cout << "Freeee 5" << endl;
    dup2(stdout_copy,1);
    cout << "Freeee 6" << endl;
    close(stdout_copy);
}


