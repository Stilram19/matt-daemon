#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <termios.h>
#include <sys/select.h>
#include <cstring>
#include <signal.h>
#include "RSA_Encryption.hpp"
#include "AES.hpp"
#include <iostream>


#define PORT 4242
#define BUFFER_SIZE 4096

struct termios original_termios;

void restore_terminal()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

void enable_raw_mode()
{
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(restore_terminal);

    struct termios raw = original_termios;

    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}


#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>

bool receiveMessage(int socketFd, std::vector<unsigned char> &out)
{
    static std::vector<unsigned char> buffer;
    static size_t expectedSize = 0;
    static bool headerParsed = false;

    unsigned char temp[4096];

    ssize_t bytesRead = recv(socketFd, temp, sizeof(temp), 0);
    if (bytesRead <= 0)
        exit(1);

    buffer.insert(buffer.end(), temp, temp + bytesRead);

    // parse header if not already done
    if (!headerParsed)
    {
        std::string bufStr(reinterpret_cast<char*>(buffer.data()), buffer.size());
        auto headerEndPos = bufStr.find('\n');

        if (headerEndPos != std::string::npos)
        {
            std::string sizeStr = bufStr.substr(0, headerEndPos);
            expectedSize = std::stoul(sizeStr);

            buffer.erase(buffer.begin(), buffer.begin() + headerEndPos + 1);
            headerParsed = true;
        }
    }


    if (headerParsed && buffer.size() >= expectedSize)
    {

        out.clear();
        out.assign(buffer.begin(), buffer.begin() + expectedSize);

        buffer.erase(buffer.begin(), buffer.begin() + expectedSize);

        // reset
        headerParsed = false;
        expectedSize = 0;

        return true;
    }

    return false;
}

int main(int argc, char* argv[])
{
    std::string secret_key;
    rsa::RSA_Encryption rsa;

    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <server_ip>\n";
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        return 1;
    }

    if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        return 1;
    }

    enable_raw_mode();
    restore_terminal();

    char buffer[BUFFER_SIZE];


    // send RSA public key to the server
    rsa.generateKeys(2048);
    std::string public_key = rsa.getPublicKeyPEM();
    send(sockfd, public_key.c_str(), public_key.size(), 0);

 

    while (true)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(sockfd, &fds);

        int maxfd = std::max(STDIN_FILENO, sockfd) + 1;

        if (select(maxfd, &fds, nullptr, nullptr, nullptr) < 0)
        {
            perror("select");
            break;
        }

        if (secret_key.empty()) {

            std::vector<unsigned char> tmp;
            if (receiveMessage(sockfd, tmp)) {
                tmp = rsa.decrypt(tmp);
                secret_key = std::string((char *)tmp.data(), tmp.size());
            }
            
        } else {
            if (FD_ISSET(0, &fds))
            {
                ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
                if (bytes <= 0 && errno == EINTR) {
                    break;
                }
                if (bytes < 0) {
                    exit(1);
                }
    
                auto res = aes::encrypt(std::string(buffer, bytes), secret_key);
                std::string size = std::to_string(res.size()) + "\n";
                send(sockfd, size.data(), size.size(), 0);
                send(sockfd, res.data(), res.size(), 0);
            }
    
            if (FD_ISSET(sockfd, &fds))
            {
                std::vector<unsigned char> tmp;
                if (receiveMessage(sockfd, tmp)) {
                    tmp = aes::decrypt(tmp, secret_key);

                    std::string msg(tmp.begin(), tmp.end());
                    
                    if (msg == "__SHELL_START__")
                    {
                        enable_raw_mode();
                        continue;
                    }
                    else if (msg == "__SHELL_END__")
                    {
                        restore_terminal();
                        continue;
                    }
                    write(1, tmp.data(), tmp.size());
                }
            }
        }
    }

    return 0;
}