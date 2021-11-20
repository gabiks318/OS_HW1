#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    cout << "smash got ctrl-Z" << endl;
    SmallShell& smash = SmallShell::getInstance();
    if(smash.current_process != -1) {
        smash.job_list.addJob(smash.current_cmd, smash.current_process, true);
        kill(smash.current_process, SIGSTOP);
        cout << "smash: process " << smash.current_process << " was stopped" << endl;
        smash.current_process = -1;
        smash.current_cmd = nullptr;
    }
}

void ctrlCHandler(int sig_num) {
  // TODO: Add your implementation
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

