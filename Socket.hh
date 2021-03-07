#ifndef EV_SOCKET_HH_
#define EV_SOCKET_HH_

#include <string>
#include <memory>

namespace ev {

class EPoller;
class Socket : public std::enable_shared_from_this<Socket> {
public:
    using ptr = std::shared_ptr<Socket>;

    Socket(EPoller& poller);
    Socket(int fd, EPoller& poller);
    ~Socket();
    int getFd();
    void setNonBlock();
    void bindAndListen(unsigned short port);
    void readStream();
    void writeStream(const std::string& msg = std::string{});
private:
    int fd_ = -1;
    EPoller &poller_;
    std::string readBuffer_;
    std::string writeBuffer_;
};

}

#endif