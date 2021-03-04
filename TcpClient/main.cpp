#include "TcpClient.h"
#include "XmlPacket.h"
#include <future>
#include <iostream>

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

class TestClient : public TcpClient
{
public:
	TestClient() {}
	~TestClient() {}

protected:
	//�½����ӳɹ��󣬻���øú���    
	virtual void ConnectionEvent(ConnClient *conn) { 
		std::cout << "connect successed!!" << std::endl; 
		//TestStruct test;
		//test.i = 1;
		//test.j = 2;
		//ProtoContent content = CombineProtoContent(1, 0, REQUEST, XmlPacket::TestXml, test);

		ProtoContent content = CombineProtoContent(1, 0, REQUEST, XmlPacket::RegisterXml);
		conn->WriteContentToBuffer(content);
	}

	//��ȡ�����ݺ󣬻���øú���    
	virtual void ReadEvent(ConnClient *conn, ProtoContent content) {

		unsigned char begin0 = content.begin_buf[0];
		unsigned char begin1 = content.begin_buf[1];

		std::string xml = content.xml_buf;
		int len = xml.length();
		int xml_length = content.xml_length;
		unsigned char end0 = content.end_buf[0];
		unsigned char end1 = content.end_buf[1];
		std::cout << xml << std::endl;
	}

	//������ɹ��󣬻���øú�������Ϊ���������⣬���Բ�����ÿ�η��������ݶ��ᱻ���ã�    
	virtual void WriteEvent(ConnClient *conn) { }

	//�Ͽ����ӣ��ͻ��Զ��Ͽ����쳣�Ͽ����󣬻���øú���    
	virtual void CloseEvent(ConnClient *conn, short events) { std::cout << "server closed!!" << std::endl; }
};

int main()
{
	TestClient client;
	client.ConnectServer("127.0.0.1", 2111);
	client.StartRun();
	printf("done\n");
	return 0;
}