epoll:
	g++ epoll.cc Poller.cc Socket.cc -std=c++17
test:
	g++ queue_test.cc -std=c++17 -lpthread
