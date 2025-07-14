#include <iostream>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "calc.hpp"

constexpr int MAX_EVENTS = 100;
constexpr int BUFFER_SIZE = 1024;

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char* argv[]) {
    // проверям задал ли пользователь порт
    if (argc != 2) {
        std::cerr << "specify the port" << std::endl;
        return 1;
    }

    int PORT = std::stoi(argv[1]);
    if (PORT <= 0 || PORT > 65535) {
        std::cerr << "Error: port must be in range 1–65535\n";
        return 1;
    }

    // создаем сокет
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
 
    // Привязываем сокет к адресу и порту
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    // Ожидаем подключений
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    set_nonblocking(server_fd);

    // создаем экземпляр epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    // создаем буферы для отправки и чтения данных
    std::unordered_map<int, std::string> recv_buffers;
    std::unordered_map<int, std::string> send_buffers;

    epoll_event events[MAX_EVENTS];

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd) {
                // обработка входящего подключения
                while (true) {
                    sockaddr_in client_addr{};
                    socklen_t len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &len);
                    if (client_fd < 0) break;

                    set_nonblocking(client_fd);
                    epoll_event client_event{};
                    client_event.events = EPOLLIN | EPOLLET;
                    client_event.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);

                    recv_buffers[client_fd] = "";
                    send_buffers[client_fd] = "";
                }
            }
            else if (events[i].events & EPOLLIN) {
                char buf[BUFFER_SIZE];
                ssize_t n;
                while ((n = recv(fd, buf, BUFFER_SIZE, 0)) > 0) { // читаем данные с сокета
                    recv_buffers[fd].append(buf, n);
                    size_t pos;

                    // обрабатываем полученное арифметическое выражение
                    while ((pos = recv_buffers[fd].find(' ')) != std::string::npos) {
                        std::string expr = recv_buffers[fd].substr(0, pos);
                        recv_buffers[fd].erase(0, pos + 1);
                        double result = eval_expr(expr);
                        std::ostringstream oss;
                        oss << result << " ";

                        send_buffers[fd] += oss.str();

                        epoll_event out_event{};
                        out_event.events = EPOLLIN | EPOLLOUT | EPOLLET;
                        out_event.data.fd = fd;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &out_event);
                    }
                }
                if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    close(fd);
                    recv_buffers.erase(fd);
                    send_buffers.erase(fd);
                }
            }
            // отправляем результат вычислений клиенту
            else if (events[i].events & EPOLLOUT) {
                std::string& buffer = send_buffers[fd];
                ssize_t n = send(fd, buffer.data(), buffer.size(), MSG_NOSIGNAL);
                if (n > 0) {
                    buffer.erase(0, n);
                    if (buffer.empty()) {
                        epoll_event in_event{};
                        in_event.events = EPOLLIN | EPOLLET;
                        in_event.data.fd = fd;
                        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &in_event);
                    }
                } else {
                    close(fd);
                    recv_buffers.erase(fd);
                    send_buffers.erase(fd);
                }
            }
        }
    }
    close(server_fd);
    return 0;
}
