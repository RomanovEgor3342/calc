#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include "calc.hpp"

constexpr int BUFFER_SIZE = 1024;
constexpr int MAX_EVENTS = 100;

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

std::string generate_expression(int n) {
    // создание арифметического выражения
    std::ostringstream oss;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> op_dis(0, 3);
    std::uniform_int_distribution<> num_dis(1, 100); // используются числа до 100
    const char ops[] = {'+', '-', '*', '/'};

    oss << num_dis(gen);
    for (int i = 1; i < n; ++i) {
        oss << ops[op_dis(gen)] << num_dis(gen);
    }
    return oss.str();
}

std::string prepare_expression(const std::string& expr) {
    // символ \n используется как маркер окончания выражения
    std::string with_newline = expr + "\n";
    return with_newline;
}

int connect_to_server(int port, const char* server_addr) {
    // функция подключения к серверу
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    // если создать дескриптор не удалось
    assert(fd >= 0);
    set_nonblocking(fd);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, server_addr, &addr.sin_addr);

    // устанавливаем соединение
    connect(fd, (sockaddr*)&addr, sizeof(addr));
    return fd;
}

int main(int argc, char* argv[]) {
    // проверяем аргументы запуска
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <n_nums> <connections> <server_addr> <server_port>\n";
        return 1;
    }
    int N_NUMBERS, CONNECTIONS, SERVER_PORT;
    const char* SERVER_ADDR = argv[3];
    
    N_NUMBERS = std::stoi(argv[1]);
    if (N_NUMBERS < 1) {
        std::cerr << "Error: <n> must be a positive integer\n";
        return 1;
    }

    CONNECTIONS = std::stoi(argv[2]);
    if (CONNECTIONS < 1) {
        std::cerr << "Error: <connections> must be a positive integer\n";
        return 1;
    }

    SERVER_PORT = std::stoi(argv[4]);
    if (SERVER_PORT <= 0 || SERVER_PORT > 65535) {
        std::cerr << "Error: <server_port> must be in range 1–65535\n";
        return 1;
    }

    // создаем экземпляр epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        return 1;
    }

    // создаем буферы для данных
    std::unordered_map<int, std::string> send_buffers;
    std::unordered_map<int, std::string> recv_buffers;
    std::unordered_map<int, std::string> expr_map;
    std::unordered_map<int, size_t> send_offsets;

    // подготавливаем данные и устанавливаем соединение с сервером
    for (int i = 0; i < CONNECTIONS; ++i) {

        std::string expr = generate_expression(N_NUMBERS);
        std::string full_expr = prepare_expression(expr);
        int fd = connect_to_server(SERVER_PORT, SERVER_ADDR);

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

        send_buffers[fd] = full_expr;
        recv_buffers[fd] = "";
        expr_map[fd] = expr;
        send_offsets[fd] = 0;
    }

    epoll_event events[MAX_EVENTS];

    while (!send_buffers.empty()) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);

        // обходим дескрипторы
        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            // можно ли на сокете безопасно вызвать send
            if (events[i].events & EPOLLOUT) {
                std::string& buffer = send_buffers[fd];
                size_t& offset = send_offsets[fd];

                if (offset < buffer.size()) {
                    ssize_t n = send(fd, buffer.data() + offset, buffer.size() - offset, MSG_NOSIGNAL); // отправляем
                    if (n > 0) {
                        offset += n;
                        // Если отправлять больше нечего, то можно отключить EPOLLOUT
                        if (offset >= buffer.size()) {
                            epoll_event ev{};
                            ev.events = EPOLLIN | EPOLLET;
                            ev.data.fd = fd;
                            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
                        }
                    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        // очищаем буферы и закрываем дескриптор, если произошла ошибка
                        close(fd);
                        send_buffers.erase(fd);
                        recv_buffers.erase(fd);
                        expr_map.erase(fd);
                        send_offsets.erase(fd);
                    }
                }
            }
            // Если можно прочитать с сокета
            if (events[i].events & EPOLLIN) {
                char buf[BUFFER_SIZE];
                ssize_t n = recv(fd, buf, BUFFER_SIZE - 1, 0); // запрашиваем

                if (n > 0) {
                    recv_buffers[fd].append(buf, n);
                    size_t pos = recv_buffers[fd].find('\n'); // находим символ окончания выражения
                    if (pos != std::string::npos) {
                        std::string response = recv_buffers[fd].substr(0, pos);
                        std::string expr = expr_map[fd];

                        double expected = eval_expr(expr);
                        double actual = std::stod(response);
                        double abs_err = std::abs(expected - actual);
                        double rel_err = abs_err / std::max(1.0, std::abs(expected));

                        // В задании не было указано, нужно ли что-то выводить в случае, если все верно, но я решил всё же добавить этот вывод
                        // Теперь, в случае ошибки вывод производится в std::cerr, иначе в std::cout
                        auto& output_stream = rel_err > 1e-4 ? std::cerr : std::cout; // выбираем поток, куда запишем результат
                        std::string status = rel_err > 1e-4 ? "FAIL" : "CORRECT"; 

                        output_stream << status << ": " << expr
                                      << " => server: " << actual
                                      << ", expected: " << expected << "\n";

                        close(fd);
                        send_buffers.erase(fd);
                        recv_buffers.erase(fd);
                        expr_map.erase(fd);
                        send_offsets.erase(fd);
                    }
                } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    close(fd);
                    send_buffers.erase(fd);
                    recv_buffers.erase(fd);
                    expr_map.erase(fd);
                    send_offsets.erase(fd);
                }
            }
        }
    }

    close(epoll_fd);
    return 0;
}
