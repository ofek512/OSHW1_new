#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char *argv[])
{
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR)
    {
        perror("smash error: failed to set ctrl-C handler");
    }

    SmallShell &smash = SmallShell::getInstance();
    while (true)
    {
        std::cout << smash.getPrompt() << "> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);

        // Check if getline failed (e.g., due to Ctrl+C signal)
        if (std::cin.fail() && !std::cin.eof())
        {
            std::cin.clear(); // Clear the error state
            continue;         // Skip to next iteration to print prompt again
        }

        // If we got a command, execute it even if EOF is reached
        if (!cmd_line.empty())
        {
            smash.executeCommand(const_cast<char *>(cmd_line.c_str()));
        }

        if (std::cin.eof())
        {
            break; // Exit on EOF (Ctrl+D)
        }
    }
    return 0;
}
