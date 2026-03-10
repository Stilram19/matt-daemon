#include <gtkmm.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

class ClientWindow : public Gtk::Window
{
private:
    Gtk::Entry entry;
    int sockfd = -1;
    std::thread monitor_thread;

public:
    ClientWindow()
    {
        set_title("Client");
        set_default_size(300, 100);

        add(entry);
        entry.set_placeholder_text("Type message and press Enter");

        entry.signal_activate().connect(sigc::mem_fun(*this,
                                &ClientWindow::on_enter_pressed));

        show_all_children();

        connect_to_server();

        monitor_thread = std::thread(&ClientWindow::monitor_server_exit, this);
    }

    ~ClientWindow()
    {
        if (sockfd != -1)
            ::close(sockfd);
        if (monitor_thread.joinable())
            monitor_thread.join();
    }

    void connect_to_server()
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(4242);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        {
            std::cerr << "Connection failed\n";
            exit(1);
        }
    }

    void on_enter_pressed()
    {
        std::string msg = entry.get_text();

        if (!msg.empty())
        {
            msg += "\n";
            send(sockfd, msg.c_str(), msg.size(), 0);
            entry.set_text("");
        }
    }


    void monitor_server_exit()
    {
        char buffer[1];

        while (true)
        {
            int r = recv(sockfd, buffer, sizeof(buffer), MSG_PEEK);

            if (r <= 0)
            {
                std::cout << "Server disconnected\n";
                Glib::signal_idle().connect_once([this]() {
                    hide();
                });
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
};

int main(int argc, char *argv[])
{
    auto app = Gtk::Application::create(argc, argv, "Ben_AFK");

    ClientWindow window;

    return app->run(window);
}