#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include <cstring>
#include <vector>
#include "Shell.hpp"
#include <pty.h>
#include <stdexcept>

# define MARKER "__END__"


Shell::Shell() {
    signal(SIGPIPE, SIG_IGN);

    pid = forkpty(&master_fd, nullptr, nullptr, nullptr);

    if (pid < 0) {
        throw std::runtime_error("Can't fork a process");
    }

    if (pid == 0) {
        execl("/bin/login", "login", nullptr);
        perror("execl");
        _exit(1);
    }
}

Shell::~Shell() {
    close(master_fd);
    kill(pid, 9);
}

