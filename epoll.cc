#include "Socket.hh"
#include "Poller.hh"

using namespace ev;

int main() {
    EPoller p;
    auto listener = std::make_shared<Socket>(p);
    listener->bindAndListen(80/*port*/);
    p.setListener(listener);
    p.loop();
}