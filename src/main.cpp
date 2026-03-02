#include "Tintin_reporter.hpp"
#include "Matt_daemon.hpp"
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

int main() {
    // (*) the user is must be root
    if (geteuid() != 0) {
        printf("only root can run this program!");
        exit(EXIT_FAILURE);
    }

    // (*) creating the logger instance
    const Tintin_reporter &tintin_reporter = Tintin_reporter::getLoggerInstance("/var/log/matt_daemon/matt_daemon.log");

    Matt_daemon &matt_daemon = Matt_daemon::getMattDaemon(tintin_reporter);

    matt_daemon.start();
}
