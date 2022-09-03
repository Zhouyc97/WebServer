server: main.cpp ./block_que/block_que.hpp ./http/http_conn.cpp ./lock/locker.cpp ./log/log.cpp ./mysql/sql_connection_pool.cpp ./threadpool/threadpool.hpp ./timer/timer.cpp webserver.cpp
	$(CXX) $^ -o $@ -w -pthread -lmysqlclient -std=c++11

clean:
	rm  -rf server

