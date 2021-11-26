#include <unistd.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <exception>
#include <chrono>
#include <thread>
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
#define SYS_FAIL -1
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
    bool failed = false;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(_trim(s).length() + 1);
        if (!args[i]) {
            failed = true;
            break;
        }
        memset(args[i], 0, _trim(s).length() + 1);
        strcpy(args[i],
               _trim(s).c_str());
        i++;
    }
    if (failed) {
        for (int j = 0; j < i; j++)
            free(args[j]);
        i = -1;
    }
    return i;

    FUNC_EXIT()
}


char **init_args(const char *cmd_line, int *num_of_args) {
    char **args = (char **) malloc(COMMAND_MAX_ARGS * sizeof(char **));
    if (!args)
        return nullptr;
    for (int i = 0; i < COMMAND_MAX_ARGS; i++)
        args[i] = nullptr;
    int num = _parseCommandLine(cmd_line, args);
    if (num == -1)
        args = nullptr;
    *num_of_args = num;
    return args;
}

void free_args(char **args, int arg_num) {
    if (!args)
        return;

    for (int i = 0; i < arg_num; i++) {
        if (args[i])
            free(args[i]);
    }
    free(args);
}

bool is_number(const std::string &s) {
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

void print_sys_error(string sys_call) {
    cerr << "smash error " << sys_call << " failed" << endl;
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


SmallShell::SmallShell() : smash_prompt("smash"), pid(getpid()), last_directory(nullptr), job_list(), alarm_list(),
                           current_process(-1), current_cmd() ,alarm_pid(-1), current_duration(0) {

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


    string cmd_s = _trim(string(cmd_line)); //REMOVE WHITESPACES
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (strstr(cmd_line, ">") != NULL || strstr(cmd_line, ">>") != NULL) {
        return new RedirectionCommand(cmd_line);
    }

    if (strstr(cmd_line, " | ") != NULL || strstr(cmd_line, " |& ")) {
        return new PipeCommand(cmd_line);
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
    } else if ((firstWord.compare("head") == 0)) {
        return new HeadCommand(cmd_line);
    } else if (firstWord.compare("timeout") == 0){
        return new TimeoutCommand(cmd_line);
    } else{
        return new ExternalCommand(cmd_line);
    }

    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line, bool alarm) {

    if (string(cmd_line).find_first_not_of(" \n\r\t\f\v") == string::npos)
        return;
    job_list.removeFinishedJobs();
    Command *cmd = CreateCommand(cmd_line);
    cmd->execute();
    if (alarm) {
        int current_pid = alarm_pid > -1 ? alarm_pid : current_process;
        alarm_list.add_alarm(cmd->get_cmd_line(), current_duration, current_pid);
    }
    delete cmd;
    current_process = -1;
    alarm_pid = -1;
    current_duration = 0;
    current_cmd = "";
}

/////////////////////////////--------------Job List implementation-------//////////////////////////////

JobsList::JobEntry::JobEntry(int job_id, pid_t job_pid, time_t time_created, std::string& command, bool isStopped)
        : job_id(job_id),
        job_pid(job_pid),
        time_created(time_created),
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

    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        auto job = *it;
        int status;
        int ret_wait = waitpid(job.job_pid, &status, WNOHANG);
        if (ret_wait == job.job_pid) {
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
    int max_job_found = -1;
    for (auto it = job_list.begin(); it != job_list.end(); ++it) {
        auto job = *it;
        if(job.job_id > max_job_found){
            max_job_found = job.job_id;
        }
    }
    *lastJobId = max_job_found;
    return getJobById(max_job_found);
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
    string cmd_line(cmd->get_cmd_line());
    job_list.push_back(JobEntry(max_job_id, pid, time(nullptr), cmd_line,
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
    int num_of_args;
    char **args = init_args(this->cmd_line, &num_of_args);
    if (!args) {
        string sys_call("malloc");
        print_sys_error(sys_call);
        return;
    }
    SmallShell &shell = SmallShell::getInstance();
    if (num_of_args == 1) {
        shell.smash_prompt = "smash";
    } else {
        shell.smash_prompt = args[1];
    }
    free_args(args, num_of_args);
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

    int num_of_args;
    char **args = init_args(this->cmd_line, &num_of_args);
    if (!args) {
        print_sys_error("malloc");
        return;
    }
    SmallShell &shell = SmallShell::getInstance();

    if (num_of_args > 2) {
        cerr << "smash error: cd: too many arguments" << endl;
    } else if (num_of_args == 1) {
        cerr << "smash error: cd: no arguments" << endl;
    } else {
        long max_size = pathconf(".", _PC_PATH_MAX);
        char *buffer = (char *) malloc((size_t) max_size);
        if (!buffer) {
            print_sys_error("malloc");
            free_args(args, num_of_args);
            return;
        }
        if (getcwd(buffer, (size_t) max_size) == NULL) {
            print_sys_error("getcwd");
            free(buffer);
            free_args(args, num_of_args);
            return;
        }
        string next_dir = args[1];

        if (next_dir == "-") {
            if (!(*plastPwd)) {
                cerr << "smash error: cd: OLDPWD not set" << endl;
                free(buffer);
            } else {
                if (chdir(*plastPwd) == SYS_FAIL) {
                    print_sys_error("chdir");
                    free(buffer);
                    free_args(args, num_of_args);
                    return;
                } else {
                    if (*plastPwd)
                        free(*plastPwd);
                    *plastPwd = buffer;
                }
            }
        } else {
            if (chdir(args[1]) == SYS_FAIL) {
                print_sys_error("chdir");
                free_args(args, num_of_args);
                return;
            } else {
                if (*plastPwd)
                    free(*plastPwd);
                *plastPwd = buffer;
            }
        }
    }
    free_args(args, num_of_args);
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
    int num_of_args;
    char **args = init_args(this->cmd_line, &num_of_args);
    if (!args) {
        print_sys_error("malloc");
        return;
    }
    if (num_of_args != 3) {
        cerr << "smash error: kill: invalid arguments" << endl;
    } else {
        int job_id;
        try {
            // Check for a valid job-id
            if (!is_number(args[2]))
                throw exception();
            char first_char = string(args[1]).at(0);
            char minus = '-';
            if(first_char != minus)
                throw exception();
            job_id = stoi(args[2]);

        } catch (exception &) {
            cerr << "smash error: kill: invalid arguments" << endl;
            free_args(args, num_of_args);
            return;
        }
        if (JobsList::JobEntry *job = jobs->getJobById(job_id)) {
            int job_pid = job->job_pid;
            int signum;
            try {
                // Check for a valid signal number
                if (!is_number(string(args[1]).erase(0, 1)))
                    throw exception();
                signum = stoi(string(args[1]).erase(0, 1));
            } catch (exception &) {
                cerr << "smash error: kill: invalid arguments" << endl;
                free_args(args, num_of_args);
                return;
            }
            if (kill(job_pid, signum) == SYS_FAIL) {
                print_sys_error("kill");
                free_args(args, num_of_args);
                return;
            }
            cout << "signal number " << signum << " was sent to pid " << job_pid << endl;

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
}

/* FG Command
 * fg command brings a stopped process or a process that runs in the background to theforeground.
 */

ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void ForegroundCommand::execute() {
    // TODO: Check what the process name should be if stopped(ctrl-z after using this command)
    int num_of_args;
    char **args = init_args(this->cmd_line, &num_of_args);
    if (!args) {
        print_sys_error("malloc");
        return;
    }
    SmallShell &smash = SmallShell::getInstance();
    if (num_of_args == 2) {
        int job_id;
        try {
            if (!is_number(args[1]))
                throw exception();
            job_id = stoi(args[1]);
        } catch (exception &) {
            cerr << "smash error: fg: invalid arguments" << endl;
            free_args(args, num_of_args);
            return;
        }
        JobsList::JobEntry *job = jobs->getJobById(job_id);
        if (job_id >= 0 && job) {
            int job_pid = job->job_pid;
            if (job->isStopped) {
                if (kill(job_pid, SIGCONT) == SYS_FAIL) {
                    print_sys_error("kill");
                    free_args(args, num_of_args);
                    return;
                }
            }
            int status_p;
            cout << job->command << ":" << job_pid << endl;
            smash.current_process = job_pid;
            smash.current_cmd = job->command;
            jobs->removeJobById(job_id);

            if (waitpid(job_pid, &status_p, WUNTRACED) == SYS_FAIL) {

                print_sys_error("waitpid");
                free_args(args, num_of_args);
                return;
            }
        } else {
            cerr << "smash error: fg: job-id " << job_id << " does not exist" << endl;
        }

    } else if (num_of_args == 1) {
        int lastJobId;
        JobsList::JobEntry *max_job = jobs->getLastJob(&lastJobId);
        if (!max_job) {
            cerr << "smash error: fg: jobs list is empty" << endl;
        } else {
            if (max_job->isStopped) {
                if (kill(max_job->job_pid, SIGCONT) == SYS_FAIL) {
                    print_sys_error("kill");
                    free_args(args, num_of_args);
                    return;
                }
            }

            int status_p;
            jobs->removeJobById(lastJobId);
            cout << max_job->command << ":" << max_job->job_pid << endl;
            smash.current_process = max_job->job_pid;
            smash.current_cmd = max_job->command;
            if (waitpid(max_job->job_pid, &status_p, WUNTRACED) == SYS_FAIL) {
                print_sys_error("waitpid");
                free_args(args, num_of_args);
                return;
            }
        }
    } else {
        cerr << "smash error: fg: invalid arguments" << endl;
    }

    free_args(args, num_of_args);
}

/* BG Command
 * bg command resumes one of the stopped processes in the background.
 */

BackgroundCommand::BackgroundCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void BackgroundCommand::execute() {

    int num_of_args;
    char **args = init_args(this->cmd_line, &num_of_args);
    if (!args) {
        print_sys_error("malloc");
        return;
    }
    if (num_of_args == 2) {
        int job_id;
        try {
            if (!is_number(args[1]))
                throw exception();
            job_id = stoi(args[1]);
        } catch (exception &) {
            cerr << "smash error: bg: invalid arguments" << endl;
            free_args(args, num_of_args);
            return;
        }
        JobsList::JobEntry *job = jobs->getJobById(job_id);
        if (job_id >= 0 && job) {
            int job_pid = job->job_pid;
            if (!job->isStopped) {
                cerr << "smash error: bg:job-id " << job_id << " is already running in the background" << endl;
            } else {
                if (kill(job_pid, SIGCONT) == SYS_FAIL) {
                    print_sys_error("kill");
                    free_args(args, num_of_args);
                    return;
                }
                job->isStopped = false;
                cout << job->command << ":" << job_pid << endl;
            }
        } else {
            cerr << "smash error:bg:job-id " << job_id << " does not exist" << endl;
        }
    } else if (num_of_args == 1) {
        int lastJobId;
        JobsList::JobEntry *max_job = jobs->getLastStoppedJob(&lastJobId);
        if (lastJobId == -1) {
            cerr << "smash error: bg: there is no stopped jobs to resume" << endl;
        } else {
            if (kill(max_job->job_pid, SIGCONT) == SYS_FAIL) {
                print_sys_error("kill");
                free_args(args, num_of_args);
                return;
            }
            max_job->isStopped = false;
            cout << max_job->command << ":" << max_job->job_pid << endl;
        }
    } else {
        cerr << "smash error: bg: invalid arguments" << endl;
    }

    free_args(args, num_of_args);
}

HeadCommand::HeadCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void HeadCommand::execute() {
// TODO: what to do if more than 3 arguments?
    int line_num = 10;
    string line;
    char *filename;
    int num_of_args;
    char **args = init_args(this->cmd_line, &num_of_args);

    if (num_of_args == 3 || num_of_args == 2) {
        if (num_of_args == 3) {
            try {
                if (!is_number(string(args[1]).erase(0, 1)))
                    throw exception();
                line_num = stoi(string(args[1]).erase(0, 1));
            } catch (exception &) {
                cerr << "smash error: head: invalid arguments" << endl;
                free_args(args, num_of_args);
                return;
            }
            filename = args[2];
        } else {
            filename = args[1];
        }
        int fd = open(filename, O_RDWR);
        if (fd == SYS_FAIL) {
            free_args(args, num_of_args);
            print_sys_error("open");
            return;
        }
        close(fd);
        std::ifstream file(filename);
        for (int i = 0; i < line_num && getline(file, line); i++) {
            cout << line << endl;
        }
    } else {
        if (num_of_args == 1) {
            cerr << "smash error: head: not enough arguments" << endl;
        }
    }
    free_args(args, num_of_args);
}

/* Quit Command
 * quit command exits the smash.
 */

QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

void QuitCommand::execute() {
    int num_of_args;
    char **args = init_args(this->cmd_line, &num_of_args);
    if (num_of_args >= 2 && string(args[1]).compare("kill") == 0) {
        cout << "smash: sending SIGKILL signal to " << jobs->job_list.size() << " jobs:" << endl;
        jobs->killAllJobs();
    }
    free_args(args, num_of_args);
//    TODO: make sure its ok
    delete this;
    exit(0);
}

/////////////////////////////--------------External Commands-------//////////////////////////////

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

void ExternalCommand::execute() {
    string trimmed_command = _trim(string(cmd_line));
    char cmd_line_copy[COMMAND_ARGS_MAX_LENGTH];
    strcpy(cmd_line_copy, trimmed_command.c_str());

    bool is_background = _isBackgroundCommand(cmd_line);
    if(is_background)
        _removeBackgroundSign(cmd_line_copy);

    char executable[] = "/bin/bash";
    char executable_name[] = "bash";
    char flag[] = "-c";


    char* fork_args[] = {executable_name,  flag, cmd_line_copy, nullptr};

    pid_t pid = fork();
    if (pid == 0) { //son
        if (setpgrp() == SYS_FAIL) {
            print_sys_error("setpgrp");
            return;
        }
        if (execv(executable, fork_args) == SYS_FAIL) {
            print_sys_error("execv");
            return;
        }
    } else {
        SmallShell& smash = SmallShell::getInstance();
        if(is_background){
            smash.job_list.addJob(this, pid, false);
            smash.alarm_pid = pid;
        } else {
            smash.current_process = pid;
            smash.current_cmd = cmd_line;
            int status;

            if (waitpid(pid, &status, WUNTRACED) == SYS_FAIL) {
                cout << "meir" << endl;
                print_sys_error("waitpid");
                return;
            }
        }
    }

}

/////////////////////////////--------------I/O,Pipe,Head,Alarm implementation-------//////////////////////////////

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {
// TODO: check what to do with more then 3 arguments
    string cmd_line_copy(cmd_line);
    int num_of_args;
    char **args = init_args(this->cmd_line, &num_of_args);
    string s = string(cmd_line);
    string delimiter = s.find(">>") != std::string::npos ? ">>" : ">";

    string trimmed_command = _trim(s.substr(0, s.find(delimiter)));
    command = (char *) malloc(sizeof(char) * (strlen(trimmed_command.c_str()) + 1));
    strcpy(command, trimmed_command.c_str());

    string trimmed_filename = _trim(s.substr(s.find(delimiter) + delimiter.length(), s.length()));
    filename = (char *) malloc(sizeof(char) * (strlen(trimmed_filename.c_str()) + 1));
    strcpy(filename, trimmed_filename.c_str());

    char double_arrow[] = ">>";
    if (strcmp(delimiter.c_str(), double_arrow) == 0) {
        append = true;
    } else {
        append = false;
    }
    prepare();

    free_args(args, num_of_args);
}

void RedirectionCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    shell.executeCommand(command);
    cleanup();
}

void RedirectionCommand::prepare() {
    stdout_copy = dup(1);
    close(1);
    if (append)
        fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0666);
    else
        fd = open(filename, O_WRONLY | O_CREAT, 0666);
}

void RedirectionCommand::cleanup() {
    free(filename);
    free(command);
    close(fd);
    if (dup2(stdout_copy, 1) == SYS_FAIL) {
        print_sys_error("dup2");
    }
    close(stdout_copy);
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {
    string s = string(cmd_line);
    delimiter = s.find("|&") == std::string::npos ? "|" : "|&";
    command1 = s.substr(0, s.find(delimiter));
    command2 = s.substr(s.find(delimiter) + 1, s.length());
}

void PipeCommand::execute() {
// TODO: Check number of arguments
    int filedes[2];
    pipe(filedes);
    SmallShell &shell = SmallShell::getInstance();
    pid_t pid1 = fork(), pid2;
    int fail_check;
    if (pid1 == SYS_FAIL) {
        print_sys_error("fork");
        close(filedes[0]);
        close(filedes[1]);
        return;
    }
    if (pid1 == 0) { //first son
        if (setpgrp() == SYS_FAIL) {
            print_sys_error("setpgrp");
            close(filedes[0]);
            close(filedes[1]);
            return;
        }
        if (delimiter == "|") {
            if (dup2(filedes[1], 1) == SYS_FAIL) {
                print_sys_error("dup2");
                close(filedes[0]);
                close(filedes[1]);
                return;
            }
        } else {
            if (dup2(filedes[1], 2) == SYS_FAIL) {
                print_sys_error("dup2");
                close(filedes[0]);
                close(filedes[1]);
                return;
            }
        }
        close(filedes[0]);
        close(filedes[1]);
        shell.executeCommand(command1.c_str());
        exit(0);
    }
    pid2 = fork();
    if (pid2 == SYS_FAIL) {
        print_sys_error("fork");
        close(filedes[0]);
        close(filedes[1]);
        return;
    }
    if (pid2 == 0) { //second son
        if (setpgrp() == SYS_FAIL) {
            print_sys_error("setpgrp");
            close(filedes[0]);
            close(filedes[1]);
            return;
        }
        if (dup2(filedes[0], 0) == SYS_FAIL) {
            print_sys_error("dup2");
            close(filedes[0]);
            close(filedes[1]);
            return;
        }
        close(filedes[0]);
        close(filedes[1]);
        shell.executeCommand(command2.c_str());
        exit(0);
    }
    close(filedes[0]);
    close(filedes[1]);
    if (wait(NULL) == SYS_FAIL) {
        print_sys_error("wait");
        return;
    }
    if (wait(NULL) == SYS_FAIL) {
        print_sys_error("wait");
        return;
    }
}

AlarmList::AlarmList(): alarms() {}

void AlarmList::add_alarm(std::string command, time_t duration, pid_t pid) {
    alarms.push_back(AlarmEntry(command, time(NULL), duration, pid));
}

void AlarmList::delete_alarms() {
    for (auto it = alarms.begin(); it != alarms.end(); ++it) {
        auto alarm_entry = *it;
        if(alarm_entry.time_created >= alarm_entry.time_limit){
            cout << "smash: timeout " << alarm_entry.duration << " " << alarm_entry.command << " timed out!" << endl;
            kill(alarm_entry.pid, SIGINT);
        }
    }
}

AlarmList::AlarmEntry::AlarmEntry(std::string command, time_t time_created, time_t duration, pid_t pid) :
        command(command), time_created(time_created), duration(duration), pid(pid)  {
    time_limit = time(NULL) + duration;
}


TimeoutCommand::TimeoutCommand(const char *cmd_line) : Command(cmd_line) {}

void TimeoutCommand::execute() {
    int num_of_args;
    char **args = init_args(cmd_line, &num_of_args);
    if (!args) {
        print_sys_error("malloc");
        return;
    }
    SmallShell &shell = SmallShell::getInstance();
    int delay = stoi(args[1]);
    if (fork() == 0) {
        setpgrp();

        usleep(delay * 1000000);
        kill(shell.pid, SIGALRM);
        exit(0);
    }
    string new_cmd_line;
    for(int i = 2; i < num_of_args; i++){
        new_cmd_line.append(string(args[i]));
        new_cmd_line.append(" ");
    }
    shell.current_duration = delay;
    shell.executeCommand(new_cmd_line.c_str(), true);
    free_args(args, num_of_args);
}


