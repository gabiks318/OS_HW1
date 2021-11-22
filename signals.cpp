#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    cout << "smash got ctrl-Z" << endl;
    SmallShell& smash = SmallShell::getInstance();
    if(smash.current_process != -1) {
        Command* command = smash.CreateCommand(smash.current_cmd.c_str());
        smash.job_list.addJob(command, smash.current_process, true);
        kill(smash.current_process, SIGSTOP);
        cout << "smash: process " << smash.current_process << " was stopped" << endl;
        smash.current_process = -1;
        smash.current_cmd = "";
        delete command;
    }
}

void ctrlCHandler(int sig_num) {
  // TODO: Add your implementation
}

void alarmHandler(int sig_num) {
  cout << "smash: got an alarm" << endl;
  SmallShell& shell = SmallShell::getInstance();
  shell.alarm_list.delete_alarms();
}

