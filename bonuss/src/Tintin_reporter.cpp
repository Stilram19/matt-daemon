#include "Tintin_reporter.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <stdexcept>
#include <sys/select.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h> 

// (*) constructor & destructor
Tintin_reporter::Tintin_reporter(const char *logFilePath) {
    ensureDirExists(logFilePath);

    this->fd = open(logFilePath, O_WRONLY | O_CREAT | O_APPEND, 0644); // O_APPEND gives write atomicity (in multithreading)

    if (this->fd < 0) {
        printf("cannot open lock file!\n");
        throw std::runtime_error("failure to open the log file"); 
    }
}

Tintin_reporter::~Tintin_reporter() {
    close(this->fd);
}

// (*) private helpers

void Tintin_reporter::ensureDirExists(const char *path) {
    char *pathCopy = strdup(path);
    const char *dir = dirname(pathCopy);

    struct stat st;
    if (stat(dir, &st) != 0) {
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
            printf("cannot create directory '%s': %s\n", dir, strerror(errno));
            free(pathCopy);
            throw std::runtime_error("failed to create log directory");
        }
    } else if (!S_ISDIR(st.st_mode)) {
        printf("path exists but is not a directory: %s\n", dir);
        free(pathCopy);
        throw std::runtime_error("invalid log directory");
    }

    free(pathCopy);
}

void Tintin_reporter::getTimestamp(char *buff) {
    time_t t;
    struct tm tmp;

    t = time(NULL);
    localtime_r(&t, &tmp);

    strftime(buff, TIMESTAMP_MAX_LEN, TIMESTAMP_FORMAT, &tmp); // guarantees buffer null termination if size > 0
}

void Tintin_reporter::logger(const char *log, size_t len) const {
    size_t totalWritten = 0;

    // ensure the whole message is written into the log file
    while (totalWritten < len) {
        ssize_t ret = write(this->fd, log + totalWritten, len - totalWritten);
        if (ret <= 0) {
            // if write fails due to interrupt it's fine
            if (ret < 0 && errno == EINTR) {
                continue;
            }
            exit(EXIT_FAILURE);
        }
        totalWritten += ret;
    }
}

const char  *Tintin_reporter::getLogTypeStr(LogType type) {
    switch (type) {
        case LOG:
            return "LOG";
        case ERROR:
            return "ERROR";
        default:
            return "INFO";
    }
}


// (*) public interface

const Tintin_reporter &Tintin_reporter::getLoggerInstance(const char *logFilePath) {
    try {
        static Tintin_reporter logger(logFilePath);

        return (logger);
    } catch (std::runtime_error &e) {
        exit(EXIT_FAILURE);
    }
}

void Tintin_reporter::log(LogType type, const char *msg) const {
    char timestamp[TIMESTAMP_MAX_LEN];
    Tintin_reporter::getTimestamp(timestamp);
    const char *logTypeStr = Tintin_reporter::getLogTypeStr(type);

    char log[LOG_MAX_LEN];
    int written = snprintf(log, LOG_MAX_LEN, "[%s] [ %s ] - Matt_daemon: %s.\n", timestamp, logTypeStr, msg);

    if (written < 0) {
        return;
    }

    size_t finalLen = ((size_t)written >= LOG_MAX_LEN) ? LOG_MAX_LEN - 1 : (size_t)written; 

    this->logger(log, finalLen);
}

int Tintin_reporter::getLogFileFd(void) const {
    return (this->fd);
}
