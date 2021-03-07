
#include "Socket.hh"
#include "Poller.hh"
#include "Utilities.hh"

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

using namespace ev;

Socket::Socket(EPoller &poller) : poller_(poller) {
    fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    check_return(fd_);
    fprintf(stdout, "listen socket, fd: %d\n", fd_);
}

Socket::Socket(int fd, EPoller& poller) : fd_(fd), poller_(poller) {
    setNonBlock();
    fprintf(stdout, "socket, fd: %d\n", fd_);
}

Socket::~Socket() {
    ::close(fd_);
}

int Socket::getFd() {
    return fd_;
}

void Socket::setNonBlock() {
    assert(fd_ >= 0);
    int flags = fcntl(fd_, F_GETFL, 0);
    check_return(flags);
    int r = fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
    check_return(r);
}

void Socket::bindAndListen(unsigned short port) {
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

void Socket::readStream() {
    char buf[4096];
    int n = 0;
    while ((n = ::read(fd_, buf, 4096)) > 0) {
        readBuffer_.append(buf, n);
        if (readBuffer_.size() > 4) {
            size_t pos = readBuffer_.find_first_of("\r\n\r\n");
            if (pos != std::string::npos) {
                std::string msg = readBuffer_.substr(0, pos);
                poller_.onMessage(shared_from_this(), msg);
                readBuffer_ = readBuffer_.substr(pos+4);  //remove last http message from readbuffer
            }
        }
    }
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        fprintf(stderr, "read failed, fd: %d error: %s\n", fd_, strerror(errno));
    }else if (n == 0) {
        poller_.onClose(shared_from_this());
    }
}

void Socket::writeStream(const std::string& msg) {
    writeBuffer_.append(msg);

    int ws = 0;
    int written = 0;
    size_t dataLen = writeBuffer_.size();
    while ((ws = ::write(fd_, writeBuffer_.data() + written, dataLen-written)) > 0) {
        written += ws;
    };
    assert(written <= dataLen);
    writeBuffer_ = writeBuffer_.substr(written);

    if (dataLen == written) {
        //all message sent, no need monitor EPOLLOUT event.
        //TODO: maybe epoll not monitor EPOLLOUT before, so here no need to udpate.
        poller_.update(EPOLL_CTL_MOD, fd_, EPOLLIN|EPOLLERR);
        return;
    }

    //dataLen > written
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        //not write all the message, use epoll to monitor when can continue write.
        poller_.update(EPOLL_CTL_MOD, fd_, EPOLLIN|EPOLLOUT|EPOLLERR);
    }else {
        fprintf(stderr, "write error, fd: %d\n", fd_);
    }
}
