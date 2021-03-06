#include <stdio.h>
#include <stdlib.h>       //exit()
#include <errno.h>
#include <string.h>       //strerror()
#include <unistd.h>       //close()
#include <sys/epoll.h>
#include <sys/types.h>    //socket(),bind()
#include <sys/socket.h>   //socket(),bind()
#include <netinet/in.h>   //struct sockaddr_in, INADDR_ANY
#include <unistd.h>       //fcntl()
#include <fcntl.h>        //fcntl()
#include <arpa/inet.h>
#include <assert.h>

#include <iostream>
#include <memory>
#include <map>
#include <string>

#define check_return(r) { \
    if (r < 0) { \
        fprintf(stderr, "%s:%d -- ret:%d, error msg %s\n", __FILE__, __LINE__, r, strerror(errno)); \
        exit(0); \
    } \
}

class Socket {
public:
    using ptr = std::shared_ptr<Socket>;

    Socket() {
        fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        check_return(fd_);
        fprintf(stdout, "socket, fd: %d\n", fd_);
    }
    Socket(int fd) : fd_(fd) {
        setNonBlock();
    }
    ~Socket() {
        close(fd_);
    }
    int getFd() {
        return fd_;
    }
    void setNonBlock() {
        assert(fd_ >= 0);
        int flags = fcntl(fd_, F_GETFL, 0);
        check_return(flags);
        int r = fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        check_return(r);
    }
    void bindAndListen(unsigned short port) {
        assert(fd_ >= 0);
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        int r = bind(fd_, (struct sockaddr* )&addr, sizeof(addr));
        check_return(r);
        r = listen(fd_, 1024);
        check_return(r);
        fprintf(stdout, "listen port %d success.\n", port);
    }
private:
    int fd_ = -1;
public:
    std::string readBuffer_;
};

class EPoller {
public:
    explicit EPoller() {
        fd_ = epoll_create1(0);
        check_return(fd_);
    }
    EPoller(const EPoller&) = delete;
    EPoller(EPoller&&) = delete;
    EPoller& operator=(const EPoller&) = delete;
    ~EPoller() {
        close(fd_);
    }
    
    void setListener(Socket::ptr s) {
        assert(s != nullptr);
        listener_ = s;
        update(EPOLL_CTL_ADD, s->getFd(), EPOLLIN);
    }
    void update(int op, int fd, uint32_t events) {
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = events;
        ev.data.fd = fd;
        int r = epoll_ctl(fd_, op, fd, &ev);
        check_return(r);
    }
    void addRemote(int fd) {
        remotes[fd] = std::make_shared<Socket>(fd);
        update(EPOLL_CTL_ADD, fd, EPOLLIN|EPOLLOUT|EPOLLERR);
    }
    void onAccept(int fd) {
        fprintf(stdout, "onAccept\n");
        struct sockaddr_in raddr;
        socklen_t rsz = sizeof(raddr);
        int cfd = accept(fd, (struct sockaddr *) &raddr, &rsz);
        check_return(cfd);
        printf("accept a connection from %s\n", inet_ntoa(raddr.sin_addr));
        addRemote(cfd);
    }
    void onRead(int fd) {
        fprintf(stdout, "onRead\n");
        assert(remotes.find(fd) != remotes.end());
        auto socket = remotes[fd];

        char buf[4096];
        int n = 0;
        while ((n = read(fd, buf, 4096)) > 0) {
            socket->readBuffer_.append(buf, n);
            if (socket->readBuffer_.size() > 4) {
                size_t pos = socket->readBuffer_.find_first_of("\r\n\r\n");
                if (pos != std::string::npos) {
                    std::string msg = socket->readBuffer_.substr(0, pos);
                    fprintf(stdout, "recv http msg: %s\n", msg.c_str());
                    socket->readBuffer_ = socket->readBuffer_.substr(pos+4);  //remove last http message from readbuffer
                    std::string resp = "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: 5\r\n\r\nhello";
                    write(fd, resp.c_str(), resp.size());
                }
            }
        }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            fprintf(stderr, "read failed, fd: %d error: %s\n", fd, strerror(errno));
        }else if (n == 0) {
            fprintf(stdout, "close fd: %d\n", fd);
            update(EPOLL_CTL_DEL, fd, EPOLLIN|EPOLLOUT|EPOLLERR);
            remotes.erase(fd);
            close(fd);
        }
    }

    void onWrite(int fd) {
        fprintf(stdout, "onWrite\n");

    }
    void loop() {
        assert(listener_ != nullptr);
        int listenFd = listener_->getFd();

        while (true) {
            struct epoll_event actives[100];
            int n = epoll_wait(fd_, actives, 100, -1);
            for (int i = 0; i < n; i++) {
                int fd = actives[i].data.fd;
                int events = actives[i].events;
                if (events & (EPOLLIN | EPOLLERR)) {
                    if (fd == listenFd) {
                        onAccept(fd);
                    } else {
                        onRead(fd);
                    }
                } else if (events & EPOLLOUT) {
                    onWrite(fd);
                } else {
                    fprintf(stderr, "unknow event: %d\n", events);
                }
            }
        }
    }
private:
    int fd_ = -1;  //epoll fd
    Socket::ptr listener_;
    std::map<int, Socket::ptr> remotes;  //key:fd value:Socket
};


int main() {
    EPoller p;
    auto listener = std::make_shared<Socket>();
    listener->bindAndListen(80/*port*/);
    p.setListener(listener);
    p.loop();
}