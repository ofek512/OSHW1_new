#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num)
{
    cout << "smash: got ctrl-C" << endl;

    SmallShell &smash = SmallShell::getInstance();

    if (smash.current_process != -1)
    {
        if (kill(smash.current_process, 2) == -1)
        {
            perror("smash error: kill failed");
            return;
        }

        cout << "smash: process " << smash.current_process << " was killed" << endl;
        smash.current_process = -1;
    }

    cout << smash.getPrompt() << "> ";
}
