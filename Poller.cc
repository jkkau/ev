#include "Poller.hh"
#include "Utilities.hh"

#include <unistd.h>       //close()
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <assert.h>

#include <iostream>
#include <memory>


using namespace ev;

EPoller::EPoller() {
    fd_ = epoll_create1(0);
    check_return(fd_);
}

EPoller::~EPoller() {
    ::close(fd_);
}

void EPoller::setListener(Socket::ptr s) {
    assert(s != nullptr);
    listener_ = s;
    update(EPOLL_CTL_ADD, s->getFd(), EPOLLIN);
}

void EPoller::update(int op, int fd, uint32_t events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;
    int r = epoll_ctl(fd_, op, fd, &ev);
    check_return(r);
}

void EPoller::addRemote(int fd) {
    remotes_[fd] = std::make_shared<Socket>(fd, *this);
    update(EPOLL_CTL_ADD, fd, EPOLLIN|EPOLLERR);
}

void EPoller::onClose(Socket::ptr socket) {
    assert(socket != nullptr);
    int fd = socket->getFd();
    fprintf(stdout, "peer close, fd: %d\n", fd);
    remotes_.erase(fd);
}

void EPoller::onMessage(Socket::ptr socket, const std::string& msg) {
    fprintf(stdout, "recv http msg: %s\n", msg.c_str());
    std::string resp = "HTTP/1.1 200 OK\r\nConnection: Keep-Alive\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: 5\r\n\r\nhello";
    socket->writeStream(resp);
}

void EPoller::onAccept(int fd) {
    fprintf(stdout, "onAccept, fd:%d\n", fd);
    struct sockaddr_in raddr;
    socklen_t rsz = sizeof(raddr);
    int cfd = accept(fd, (struct sockaddr *) &raddr, &rsz);
    check_return(cfd);
    printf("accept a connection from %s\n", inet_ntoa(raddr.sin_addr));
    addRemote(cfd);
}

void EPoller::onRead(int fd) {
    fprintf(stdout, "onRead, fd:%d\n", fd);
    assert(remotes_.find(fd) != remotes_.end());
    auto socket = remotes_[fd];
    socket->readStream();
}

void EPoller::onWrite(int fd) {
    fprintf(stdout, "onWrite, fd:%d\n", fd);
    assert(remotes_.find(fd) != remotes_.end());
    auto socket = remotes_[fd];
    socket->writeStream();
}

void EPoller::loop() {
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

