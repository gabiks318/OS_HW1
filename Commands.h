#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string.h>
#include <string>
#include <time.h>
#include <sys/wait.h>


#define PROCESSES_MAX_NUM (100)
#define PROCESS_NAME_MAX_LENGTH (50)
#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS 20

class Command {
protected:
    const char* cmd_line;
public:
    Command(const char *cmd_line);

    virtual ~Command() {};

    virtual void execute() = 0;
    const char* get_cmd_line(){return cmd_line;}
//    virtual void prepare();
//    virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {}

    void execute() override;
};

class PipeCommand : public Command {
    std::string command1;
    std::string command2;
    std::string delimiter;
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {}

    void execute() override;
};

class RedirectionCommand : public Command {
    char* command;
    char* filename;
    bool append;
    int stdout_copy;
    int fd;
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {}

    void execute() override;
    void prepare();
    void cleanup();
};

class ChpromptCommand : public BuiltInCommand {
public:
    ChpromptCommand(const char *cmd_line);

    virtual  ~ChpromptCommand() {}

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    char **plastPwd;
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {}

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {}

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {}

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
public:
    JobsList *jobs;
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {}

    void execute() override;
};

class AlarmList{
public:
    class AlarmEntry{
    public:
        std::string command;
        time_t time_created;
        time_t duration;
        time_t time_limit;
        pid_t pid;
        AlarmEntry(std::string command, time_t time_created, time_t duration, pid_t pid);
        ~AlarmEntry()= default;
    };

    std::vector<AlarmEntry> alarms;

    AlarmList();
    void add_alarm(std::string command, time_t duration, pid_t pid);
    void delete_alarms();
};

class JobsList {
public:
    class JobEntry {
    public:
        int job_id;
        pid_t job_pid;
        time_t time_created;
        std::string command;
        bool isStopped;

        JobEntry(int job_id, pid_t job_pid, time_t time_created, std::string& command, bool isStopped);
    };

    std::vector<JobEntry> job_list;
    int max_job_id;

    JobsList();

    ~JobsList();

    void addJob(Command *cmd, pid_t pid ,bool isStopped = false);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);
    // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    JobsCommand(const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand() {}

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    KillCommand(const char *cmd_line, JobsList *jobs);

    virtual ~KillCommand() {}

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    ForegroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~ForegroundCommand() {}

    void execute() override;
};


class BackgroundCommand : public BuiltInCommand {
    JobsList *jobs;
public:
    BackgroundCommand(const char *cmd_line, JobsList *jobs);

    virtual ~BackgroundCommand(){}

    void execute() override;
};

class HeadCommand : public BuiltInCommand {
public:
    HeadCommand(const char *cmd_line);

    virtual ~HeadCommand() {}

    void execute() override;
};

class TimeoutCommand: public Command{
public:
    TimeoutCommand(const char* cmd_line);

    virtual ~TimeoutCommand(){}

    void execute() override;
};


class SmallShell {
private:

    SmallShell();

public:
    std::string smash_prompt;
    pid_t pid;
    char* last_directory;
    JobsList job_list;

    pid_t current_process;
    std::string current_cmd;

    AlarmList alarm_list;
    pid_t alarm_pid;
    time_t current_duration;

    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line, bool alarm=false);
    void setLastDirectory(char *dir);
};

#endif //SMASH_COMMAND_H_
