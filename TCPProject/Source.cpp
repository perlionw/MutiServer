/*
����һ�������õķ�������ֻ���������ܣ�
1������ÿ�������ӿͻ��ˣ�ÿ10�����䷢��һ��hello, world
2�����ͻ�����������������ݣ��������յ����ٽ����ݻط����ͻ���
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

	using RetType = decltype(f(args...)); // typename std::result_of<F(Args...)>::type, ���� f �ķ���ֵ����
	auto task = std::make_shared<std::packaged_task<RetType()>>(
		bind(std::forward<F>(f), std::forward<Args>(args)...)
		); // �Ѻ�����ڼ�����,���(��)

	(*task)();
	std::future<RetType> ret = task->get_future();
	std::string xml = ret.get();
	content.xml_length = xml.length();
	memcpy(content.xml_buf, xml.c_str(), xml.length());
	return content;
}

//����ʾ��    
class TestServer : public MultiServer
{
private:
	vector<Conn*> vec;
protected:
	//���ظ�������ҵ����麯��    
	void ReadEvent(Conn *conn, ProtoContent content);
	void WriteEvent(Conn *conn);
	void ConnectionEvent(Conn *conn);
	void CloseEvent(Conn *conn, short events);
public:
	TestServer(int count) : MultiServer(count) { }
	~TestServer() { }

	//�˳��¼�����ӦCtrl+C    
	static void QuitCb(int sig, short events, void *data);
	//��ʱ���¼���ÿ10�������пͻ��˷�һ��hello, world    
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