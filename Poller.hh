#ifndef EV_POLLER_HH_
#define EV_POLLER_HH_

#include "Socket.hh"
#include <map>
#include <string>

namespace ev {

class EPoller {
public:
    explicit EPoller();
    EPoller(const EPoller&) = delete;
    EPoller(EPoller&&) = delete;
    EPoller& operator=(const EPoller&) = delete;
    ~EPoller();
    
    void setListener(Socket::ptr s);
    void update(int op, int fd, uint32_t events);
    void addRemote(int fd);
    void onClose(Socket::ptr socket);
    void onMessage(Socket::ptr socket, const std::string& msg);
    void onAccept(int fd);
    void onRead(int fd);
    void onWrite(int fd);
    void loop();
private:
    int fd_ = -1;  //epoll fd
    Socket::ptr listener_;
    std::map<int, Socket::ptr> remotes_;  //key:fd value:Socket
};

}

#endif