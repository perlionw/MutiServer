/*
这是一个测试用的服务器，只有两个功能：
1：对于每个已连接客户端，每10秒向其发送一句hello, world
2：若客户端向服务器发送数据，服务器收到后，再将数据回发给客户端
*/
//test.cpp    
#include "MutiServer.h"  
#include <set>    
#include <vector>   
#include <iostream>
#include "XmlPacket.h"
#include <future>


using namespace std;

template<typename F, typename ...Args>
ProtoContent CombineProtoContent(long long send_serial_num, long long receive_serial_num, SessionSourceFlag session_source_flag, F&& f, Args&& ...args)
{
	ProtoContent content;
	content.send_serial_num = send_serial_num;
	content.receive_serial_num = receive_serial_num;
	content.session_source_flag = session_source_flag;

	using RetType = decltype(f(args...)); // typename std::result_of<F(Args...)>::type, 函数 f 的返回值类型
	auto task = std::make_shared<std::packaged_task<RetType()>>(
		bind(std::forward<F>(f), std::forward<Args>(args)...)
		); // 把函数入口及参数,打包(绑定)

	(*task)();
	std::future<RetType> ret = task->get_future();
	std::string xml = ret.get();
	content.xml_length = xml.length();
	memcpy(content.xml_buf, xml.c_str(), xml.length());
	return content;
}

//测试示例    
class TestServer : public MultiServer
{
private:
	vector<Conn*> vec;
protected:
	//重载各个处理业务的虚函数    
	void ReadEvent(Conn *conn, ProtoContent content);
	void WriteEvent(Conn *conn);
	void ConnectionEvent(Conn *conn);
	void CloseEvent(Conn *conn, short events);
public:
	TestServer(int count) : MultiServer(count) { }
	~TestServer() { }

	//退出事件，响应Ctrl+C    
	static void QuitCb(int sig, short events, void *data);
	//定时器事件，每10秒向所有客户端发一句hello, world    
	static void TimeOutCb(int id, int short events, void *data);
};

void TestServer::ReadEvent(Conn *conn, ProtoContent content)
{
	std::cout << content.xml_buf << std::endl;
	conn->WriteContentToBuffer(content);
}

void TestServer::WriteEvent(Conn *conn)
{
	
}

void TestServer::ConnectionEvent(Conn *conn)
{
	TestServer *me = (TestServer*)conn->GetThread()->tcpConnect;
	printf("new connection: %d\n", conn->GetFd());
	me->vec.push_back(conn);
}

void TestServer::CloseEvent(Conn *conn, short events)
{
	printf("connection closed: %d\n", conn->GetFd());
	TestServer *me = (TestServer*)conn->GetThread()->tcpConnect;

	auto it = me->vec.begin();
	for (; it != me->vec.end(); )
	{
		if ((*it)->GetFd() == conn->GetFd())
		{
			it = me->vec.erase(it);
		}
		else
			it++;
	}

	//for (int i = 0; i < me->vec.size(); i++)
	//{
	//	if (me->vec[i]->GetFd() == conn->GetFd())
	//	{
	//		me->vec.erase(me->vec.begin() + i);
	//	}
	//}

}

void TestServer::QuitCb(int sig, short events, void *data)
{
	printf("Catch the SIGINT signal, quit in one second\n");
	TestServer *me = (TestServer*)data;
	timeval tv = { 1, 0 };
	me->StopRun(&tv);
}

void TestServer::TimeOutCb(int id, short events, void *data)
{
	TestServer *me = (TestServer*)data;
	ProtoContent content = CombineProtoContent(1, 0, REQUEST, XmlPacket::RegisterXml);
	int len;
	for (int i = 0; i < me->vec.size(); i++)
		len = me->vec[i]->WriteContentToBuffer(content);
}

int main()
{
	printf("pid: %d\n", getpid());
	TestServer server(3);
	server.AddSignalEvent(SIGINT, TestServer::QuitCb);
	struct timeval tv;
	evutil_timerclear(&tv);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	server.AddTimerEvent(TestServer::TimeOutCb, tv, false);
	server.SetPort(2111);
	server.StartRun();
	printf("done\n");
	//struct T
	//{
	//	int f;
	//	vector<char> v;
	//};
	//std::map<int, T> test;
	//vector<char> vec;
	//T t;
	//t.f = 2;
	//for (int i = 0; i < 10; ++i)
	//{
	//	t.v.push_back(i);
	//}
	//test[1] = t;

	//auto it = test.begin();
	//for (; it != test.end(); ++it)
	//{
	//	it->second.v.erase(it->second.v.begin(), it->second.v.begin() + 10);
	//}

	


	return 0;
}