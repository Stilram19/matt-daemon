#include <string>

class Shell {
    public:
    int pid;
    int master_fd;

    Shell();
    ~Shell();
    Shell(const Shell &) = delete;
    Shell &operator=(const Shell &) = delete;
};
