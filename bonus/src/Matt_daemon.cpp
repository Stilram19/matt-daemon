#include "Matt_daemon.hpp"
#include "Tintin_reporter.hpp"
#include <cerrno>
#include <csignal>
#include <cstring>
#include <netdb.h>
#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <algorithm>
#include <sys/wait.h>
#include "AES.hpp"

std::atomic<int> Matt_daemon::receivedSignal = 0;
std::atomic<int> Matt_daemon::quitRequested = 0;

// (*) constructor & destructor

Matt_daemon::Matt_daemon(const Tintin_reporter &tintin_reporter): lockFd(-1), listenFd(-1), tintin_reporter(tintin_reporter) {}

Matt_daemon::~Matt_daemon() {}


// (*) public interface

Matt_daemon &Matt_daemon::getMattDaemon(const Tintin_reporter &tintin_reporter) {
    static Matt_daemon matt_daemon(tintin_reporter);

    return (matt_daemon);
}

Matt_daemon::Client::~Client() {
    delete this->shell;
}

void Matt_daemon::start(void) {
    this->createLockFile(); // locking the lock file (to ensure we always have only one running daemon)
    this->tintin_reporter.log(Tintin_reporter::INFO, "Started");
    this->daemonize(); // creating a daemon process (fully detached from terminal)
    this->setupSignals(); // handling signals
    this->tintin_reporter.log(Tintin_reporter::INFO, "Creating server");
    this->createServer(); // create the server
    this->tintin_reporter.log(Tintin_reporter::INFO, "Server created");
    this->tintin_reporter.log(Tintin_reporter::INFO, "Entering Daemon mode");

    char buffer[32];
    snprintf(buffer, 32, "started. PID: %d", getpid());
    this->tintin_reporter.log(Tintin_reporter::INFO, buffer);

    this->eventLoop(); // event loop
    this->cleanup(); // cleanup
    this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
}

// (*) private interface

void Matt_daemon::daemonize(void) const {
    pid_t pid = fork();

    if (pid < 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "failure to create a child process");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE); // failure to create a child process
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS); // parent job done!
    }

    // child process is not a process group leader (the parent was) => we can run setsid()
    if (setsid() < 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "failure to create a new session");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE); // failure to create a new session
    }

    // at this point the process is fully detached from both the shell and tty
    // but it is the leader of the new session
    // so it can still acquire a controlling terminal by opening a tty device file (if the terminal is free)
    // so let's fork again and exit from this process, the next child process will be fully detached and not a session leader at the same time (it's the true deamon)

    pid = fork();

    if (pid < 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "failure to create daemon process");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE); // failure to create the daemon process
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS); // parent job done!
    }

    // closing all inherited file descriptors (except 0, 1, 2 and this->lockFd)
    long maxfd = sysconf(_SC_OPEN_MAX);
    int logFileFd = this->tintin_reporter.getLogFileFd();
    for (int fd = 3; fd < maxfd; ++fd) {
        if (fd != this->lockFd && fd != logFileFd) {
            close(fd);
        }
    }

    // redirecting 0, 1, 2 to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "failure to open /dev/null");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE); // failure to open /dev/null
    }
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);

    // change directory to '/' (which is always mounted)
    if (chdir("/") < 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "failure to change working directory to /");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE); // failure to change working directory to / 
    }
}

void Matt_daemon::createLockFile(void) {
    this->lockFd = open(Matt_daemon::lockFile, O_RDWR | O_CREAT, 0644);

    if (this->lockFd < 0 || flock(this->lockFd, LOCK_EX | LOCK_NB) < 0) {
        printf("Can't open :%s\n", Matt_daemon::lockFile);
        this->tintin_reporter.log(Tintin_reporter::ERROR, "Error file locked");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE);
    }
}

void Matt_daemon::removeLockFile() const {
    flock(this->lockFd, LOCK_UN);
    close(this->lockFd);
    unlink(Matt_daemon::lockFile);
}

void Matt_daemon::signalHandler(int sig) {
    Matt_daemon::receivedSignal = sig;
}

void Matt_daemon::setupSignals() const {
    struct sigaction sa{};
    sa.sa_handler = Matt_daemon::signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGHUP,  &sa, nullptr);

    signal(SIGPIPE, SIG_IGN);
}

void Matt_daemon::cleanup(void) {
    this->removeLockFile();

    // closing listenFd
    if (this->listenFd >= 0) {
        close(this->listenFd);
    }

    // closing clients sockets
    for (const Client &client : clients) {
        close(client.fd);
    }
}

void Matt_daemon::createServer(void) {
    // getaddrinfo
    struct addrinfo hints;
    struct addrinfo *res = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // socket needs binding 

    if (getaddrinfo(nullptr, Matt_daemon::PORT, &hints, &res) != 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "ERROR creating server (getaddrinfo failure)");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE);
    }

    // creating a TCP socket (to listen on connection requests)
    this->listenFd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (this->listenFd < 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "ERROR creating server (socket creation failure)");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // binding the socket
    if (bind(this->listenFd, res->ai_addr, res->ai_addrlen) < 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "ERROR creating server (socket binding failure)");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE);
    }

    if (listen(this->listenFd, Matt_daemon::MAX_CLIENTS) < 0) {
        this->tintin_reporter.log(Tintin_reporter::ERROR, "ERROR creating server (listen failure)");
        this->tintin_reporter.log(Tintin_reporter::INFO, "Quitting");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);
}

void Matt_daemon::eventLoop(void) {
    while (Matt_daemon::receivedSignal == 0 && Matt_daemon::quitRequested == 0) {
        fd_set readfds;
        FD_ZERO(&readfds);

        FD_SET(listenFd, &readfds);
        int maxFd = listenFd;

        for (const Client &client : clients) {
            FD_SET(client.fd, &readfds);
            if (client.fd > maxFd) {
                maxFd = client.fd;
            }
            if (client.shell) {
                FD_SET(client.shell->master_fd, &readfds);
                if (client.shell->master_fd > maxFd) {
                    maxFd = client.shell->master_fd;
                }
            }
        }

        int ready = select(maxFd + 1, &readfds, NULL, NULL, NULL);

        if (ready <= 0) {
            if (ready == 0 || errno == EINTR) {
                continue;
            }

            // if select failed and it wasn't interrupted we report the error and break the event loop
            this->tintin_reporter.log(Tintin_reporter::ERROR, "select failure");
            break;
        }

        // checking new connection requests
        if (FD_ISSET(this->listenFd, &readfds)) {
            int clientFd = accept(this->listenFd, NULL, NULL);

            if (clientFd < 0) {
                if (errno != EINTR) {
                    this->tintin_reporter.log(Tintin_reporter::ERROR, "accept failure");
                }
            }

            if (this->clients.size() >= MAX_CLIENTS) {
                close(clientFd);
            } else if (clientFd >= 0) {
                this->clients.emplace_back(clientFd);
            }
        }

        // handle clients activity
        for (size_t i = 0; i < this->clients.size(); ) {
            Client &client = this->clients[i];
            
            if (FD_ISSET(client.fd, &readfds)) {
                char buffer[1024];
                ssize_t bytes = read(client.fd, buffer, sizeof(buffer));

                // if read was interrupted break 
                if (bytes < 0 && errno == EINTR) {
                    break;
                }

                // if client was diconnected or read failed, close client socket and erase client from clients vector
                if (bytes <= 0) {
                    close(client.fd);
                    this->clients.erase(this->clients.begin() + i);
                    continue;
                }

                // append received bytes to the client's buffer, then extract and process each line
                client.buffer.insert(client.buffer.end(), buffer, buffer + bytes);

                    
                // get the RSA public key from the client to establish a secure session
                if (client.session_key.empty()) {
                    // we expect a string not row bytes so it is safe to convert buffer to string
                    std::string string_buffer(buffer, bytes);
                    size_t pos = string_buffer.find("-----END PUBLIC KEY-----");
                    if (pos != std::string::npos) {
                        std::string rsa_public_key = string_buffer.substr(0, pos + strlen("-----END PUBLIC KEY-----"));
                        client.buffer.clear();
    
                        this->createSecureSessionKey(client, rsa_public_key);
                    }
                } else {
                    // session is established

                    // read size header
                    if (client.size == 0) {
                        std::string string_buffer((char *)client.buffer.data(), client.buffer.size());

                        size_t pos = string_buffer.find('\n');
                        if (pos != std::string::npos) {
                            std::string line = string_buffer.substr(0, pos);
                            client.size = std::stoul(line);

                            // remove the size header from the client buffer, the rest is the remaining bytes consumed by read
                            client.buffer.erase(client.buffer.begin(), client.buffer.begin() + pos + 1);
                            bytes -= pos + 1;
                        }
                    }
                    
                    // continue reading if size != 0
                    if (client.size != 0 and client.buffer.size() >= client.size) {
                        this->handleMessage(client);
                        client.buffer.erase(client.buffer.begin(), client.buffer.begin() + client.size);
                        client.size = 0;
                    }
                }
                // }
            }

            if (client.shell && FD_ISSET(client.shell->master_fd, &readfds)) {
                char buffer[1024];
                ssize_t bytes = read(client.shell->master_fd, buffer, sizeof(buffer));
                if (bytes <= 0) {

                    auto shell_start = aes::encrypt(std::string("__SHELL_END__"), client.session_key);
                    std::string size = std::to_string(shell_start.size()) + "\n";
                    send(client.fd, size.c_str(), size.size(), 0);
                    send(client.fd, shell_start.data(), shell_start.size(), 0);

                    delete client.shell;
                    client.shell = nullptr;
                } else {
                    auto res = aes::encrypt(std::string(buffer, bytes), client.session_key);
                    
                    //Send data = size header + data
                    std::string size = std::to_string(res.size()) + "\n";
                    send(client.fd, size.c_str(), size.size(), 0);
                    send(client.fd, res.data(), res.size(), 0);
                }
            }
            if (Matt_daemon::receivedSignal || Matt_daemon::quitRequested) {
                break;
            }

            i += 1;
        }
    }

    // reporting daemon exit reason

    if (Matt_daemon::quitRequested) {
        this->tintin_reporter.log(Tintin_reporter::INFO, "Request quit");
    } else if (Matt_daemon::receivedSignal) {
        this->tintin_reporter.log(Tintin_reporter::INFO, "Signal handler");
    }
}

void Matt_daemon::handleMessage(Client &client) const {
    std::vector<unsigned char> send_data(client.buffer.begin(), client.buffer.begin() + client.size);
    send_data = aes::decrypt(send_data, client.session_key);

    //! Assuming that the client always send data that has non zero bytes
    std::string line = std::string(send_data.begin(), send_data.end());
    
    
    if (client.shell) {
        
        
        int status;
        pid_t result = waitpid(client.shell->pid, &status, WNOHANG);
    
        if (result == client.shell->pid)
        {
            // Child exited
            delete client.shell;
            client.shell = nullptr;

            auto shell_start = aes::encrypt(std::string("__SHELL_END__"), client.session_key);
            std::string size = std::to_string(shell_start.size()) + "\n";
            send(client.fd, size.c_str(), size.size(), 0);
            send(client.fd, shell_start.data(), shell_start.size(), 0);
        } 
        else {
            write(client.shell->master_fd, line.c_str(), line.size());
        }
    } else {


        client.msg.append(line);
        size_t pos = client.msg.find("\n");
        while (pos != std::string::npos) {

            std::string msg = client.msg.substr(0, pos);
            if (msg == "quit") {
                Matt_daemon::quitRequested = 1;
                return;
            } else if (msg == "shell" && client.shell == nullptr) {
                client.shell = new Shell();
                
                auto shell_start = aes::encrypt(std::string("__SHELL_START__"), client.session_key);
                std::string size = std::to_string(shell_start.size()) + "\n";
                send(client.fd, size.c_str(), size.size(), 0);
                send(client.fd, shell_start.data(), shell_start.size(), 0);

                client.msg.erase(0, pos + 1);
                write(client.shell->master_fd, client.msg.c_str(), client.msg.size());
                client.msg.clear();
                return;
            } else {
                std::string userMessage = "User input: " + msg;
                this->tintin_reporter.log(Tintin_reporter::LOG, userMessage.c_str());

            }
            client.msg.erase(0, pos + 1);
            pos = client.msg.find("\n");
        }
    }
}



void Matt_daemon::createSecureSessionKey(Client &client, const std::string &rsa_public_key) const {
    // initialize rsa with public key
    rsa::RSA_Encryption rsa;
    rsa.loadPublicKey(rsa_public_key);

    // generate session_key
    std::string session_key = generate_session_key(42);
    auto encrypted_session_key = rsa.encrypt(session_key);

    this->tintin_reporter.log(Tintin_reporter::LOG, "Secret session key generated for the client");
    client.session_key = session_key;

    //Send data = size header + data
    std::string size = std::to_string(encrypted_session_key.size()) + "\n";
    send(client.fd, size.c_str(), size.size(), 0);
    send(client.fd, encrypted_session_key.data(), encrypted_session_key.size(), 0);
}
