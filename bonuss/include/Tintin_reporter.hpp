#ifndef TINTIN_REPORTER_HPP
#define TINTIN_REPORTER_HPP

// singleton + thread-safety 

#include <cstddef>

class Tintin_reporter {
    public:
        enum LogType {
            LOG,
            INFO,
            ERROR
        };

    private:
        int fd; // file descriptor to the open log file
        static constexpr size_t TIMESTAMP_MAX_LEN = 256;
        static constexpr size_t LOG_MAX_LEN = 4096;
        static constexpr const char *TIMESTAMP_FORMAT = "%d/%m/%Y-%H:%M:%S";


    private:
        explicit Tintin_reporter(const char *logFilePath);

    public:
        Tintin_reporter() = delete; // no default construction
        Tintin_reporter(const Tintin_reporter &other) = delete; // no copy
        Tintin_reporter &operator=(const Tintin_reporter &other) = delete; // no copy assignment

        ~Tintin_reporter(); // destructor to cleanup resources acquired during construction

    // helpers
    private:
        void                logger(const char *msg, size_t len) const; // logs the message directly into the logFile
        static void         getTimestamp(char *buff); // stores the timestamp in char *buff (thread-safe)
        static const char   *getLogTypeStr(LogType type); // returns the corresponding string (ERROR, INGO, LOG) to the type (returns a char *literal)
        static void         ensureDirExists(const char *path); // ensures the directory of the path exists (if it doesn't exists, it attemts to create it)

    // interface
    public:
        int getLogFileFd(void) const; // returns the log file fd
        void log(LogType type, const char *msg) const; // logs a log
        static const Tintin_reporter &getLoggerInstance(const char *logFilePath);
};

#endif
