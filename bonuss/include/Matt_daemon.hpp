#ifndef MATT_DAEMON_HPP
#define MATT_DAEMON_HPP

#include "Tintin_reporter.hpp"
#include <atomic>
#include <string>
#include <vector>
#include "Shell.hpp"
#include "RSA_Encryption.hpp"



// singleton
class Matt_daemon {
    private:
        static std::atomic<int> receivedSignal;
        static std::atomic<int> quitRequested;
        static constexpr int MAX_CLIENTS = 3;
        static constexpr const char *PORT = "4242";
        static constexpr const char *lockFile = "/var/lock/matt_daemon.lock";

    private:
        struct Client {
            int fd;
            size_t size;
            Shell *shell;
            std::string session_key;
            std::vector<unsigned char> buffer;
            std::string msg;

            private:
                Client();

            public:
                Client(int fd): fd(fd), size(0), shell(nullptr) {}
                ~Client();
                
        };

    private:
        int lockFd; // lockfile file descriptor (shouldn't be closed as the lock will be released)
        int listenFd; // socket listening for connection requests
        std::vector<Client> clients; // clients sockets
        const Tintin_reporter &tintin_reporter;

    private:
        Matt_daemon(const Tintin_reporter &tintin_reporter);

    public:
        ~Matt_daemon();
        Matt_daemon() = delete; // no default constructor
        Matt_daemon(const Matt_daemon &other) = delete; // no copy constructor
        Matt_daemon &operator=(const Matt_daemon &other) = delete; // no copy assignment

    public:
        void start(void);

    public:
        static Matt_daemon &getMattDaemon(const Tintin_reporter &tintin_reporter); // returns always the same Matt_daemon instance

    private:
        static void signalHandler(int sig);
        void setupSignals(void) const;
        void createServer(void);
        void cleanup(void);
        void eventLoop(void);
        void createLockFile(void); // should be called before daemonization (as it requires a controlling terminal to report errors before it exits)
        void removeLockFile(void) const; // releases the lock, closes the lockFd and removes the lock file
        void daemonize(void) const;
        void handleMessage(Client &client) const;
        void createSecureSessionKey(Client &client, const std::string &rsa_public_key) const;
};

#endif
