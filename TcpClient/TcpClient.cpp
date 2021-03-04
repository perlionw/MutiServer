#include "TcpClient.h"
ConnClient::ConnClient(int fd) : m_fd(fd)
{

}

ConnClient::~ConnClient()
{
}

int ConnClient::WriteContentToBuffer(const ProtoContent& content)
{
	char buf[BUFFER_SIZE] = {};
	proto_buf.SerializeHeader(buf, PROTO_HEADER);
	proto_buf.SerializeSendSerialNumber(buf, content.send_serial_num, PROTO_SENDSERIALNUM);
	proto_buf.SerializeReceiveSerialNumber(buf, content.receive_serial_num, PROTO_RECEIVESERIALNUM);
	proto_buf.SerializeSourceFlag(buf, content.session_source_flag, PROTO_SOURCEFLAG);
	proto_buf.SerializeXmlLength(buf, content.xml_length, PROTO_XMLLENGTH);
	proto_buf.SerializeXmlContent(buf, content.xml_buf, content.xml_length, PROTO_XMLCONTENT);
	proto_buf.SerializeTail(buf, PROTO_XMLCONTENT + content.xml_length);
	return AddToWriteBuffer(buf, PROTO_XMLCONTENT + content.xml_length + 2);
}


TcpClient::TcpClient()
{
	m_ConnClient = NULL;
}

TcpClient::~TcpClient()
{
	event_base_free(m_ConnClient->m_Base);
}

void TcpClient::ConnectServer(std::string ip, int port)
{

	evthread_use_pthreads();
	m_ConnClient = new ConnClient(1);

	m_ConnClient->m_ClientPtr = this;
	m_ConnClient->m_Base = event_base_new();
	struct bufferevent *bev  = bufferevent_socket_new(m_ConnClient->m_Base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
	// init server info
	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(port);
	evutil_inet_pton(AF_INET, ip.c_str(), &serv.sin_addr.s_addr);


	//连接服务器
	bufferevent_socket_connect(bev, (struct sockaddr*)&serv, sizeof(serv));

	bufferevent_setcb(bev, ReadEventCb, WriteEventCb, CloseEventCb, m_ConnClient);

	//设置回调生效
	bufferevent_enable(bev, EV_READ | EV_WRITE | EV_PERSIST);
	std::thread* th = new std::thread(std::bind(&TcpClient::ProcessBufferCacheThFunc, this, m_ConnClient));
	th->detach();
}

void TcpClient::StartRun()
{
	event_base_dispatch(m_ConnClient->m_Base);
}

void TcpClient::ProcessBufferCacheThFunc(void *arg)
{
	ConnClient *conn = (ConnClient*)arg;
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		int buffer_size = conn->m_Buffer_Cache.size();
		if (buffer_size < PROTO_HEADER + 2)//大于解析头部所需字节
			continue;

		ProtoContent content;
		proto_buf.DeserializaHeader(conn->m_Buffer_Cache, content.begin_buf, PROTO_HEADER);
		unsigned char begin_buf0 = content.begin_buf[0];
		unsigned char begin_buf1 = content.begin_buf[1];
		if (begin_buf0 != 0xEB || begin_buf1 != 0x90)
			continue;

		if (buffer_size < PROTO_XMLLENGTH + 4)//大于解析xml长度所需字节
			continue;

		content.xml_length = proto_buf.DeserializeXmlLength(conn->m_Buffer_Cache, PROTO_XMLLENGTH);
		if (buffer_size < PROTO_XMLCONTENT + content.xml_length + 2)//大于解析尾部所需字节
			continue;

		proto_buf.DeserializaTail(conn->m_Buffer_Cache, content.end_buf, PROTO_XMLCONTENT + content.xml_length);
		unsigned char end_buf0 = content.end_buf[0];
		unsigned char end_buf1 = content.end_buf[1];
		if (end_buf0 != 0xEB || end_buf1 != 0x90)
			continue;

		content.session_source_flag = proto_buf.DeserializeSourceFlag(conn->m_Buffer_Cache, PROTO_SOURCEFLAG);
		content.send_serial_num = proto_buf.DeserializeSendSerialNumber(conn->m_Buffer_Cache, PROTO_SENDSERIALNUM);
		content.receive_serial_num = proto_buf.DeserializeReceiveSerialNumber(conn->m_Buffer_Cache, PROTO_RECEIVESERIALNUM);
		proto_buf.DeserializaXmlContent(conn->m_Buffer_Cache, content.xml_buf, content.xml_length, PROTO_XMLCONTENT);
		conn->m_ClientPtr->ReadEvent(conn, content);
		{
			std::lock_guard<decltype(conn->m_Cache_Mutex)> lock(conn->m_Cache_Mutex);
			conn->m_Buffer_Cache.erase(conn->m_Buffer_Cache.begin(), conn->m_Buffer_Cache.begin() + PROTO_XMLCONTENT + content.xml_length + 2);
		}
	}
}

void TcpClient::ReadEventCb(bufferevent * bev, void * data)
{
	ConnClient *conn = (ConnClient*)data;
	conn->m_ReadBuf = bufferevent_get_input(bev);
	conn->m_WriteBuf = bufferevent_get_output(bev);
	char buf[BUFFER_SIZE];
	int len = conn->GetReadBuffer(buf, BUFFER_SIZE);
	std::lock_guard<decltype(conn->m_Cache_Mutex)> lock(conn->m_Cache_Mutex);
	for (unsigned int i = 0; i < len; ++i)
		conn->m_Buffer_Cache.push_back(buf[i]);
}

void TcpClient::WriteEventCb(bufferevent * bev, void * data)
{
	ConnClient *conn = (ConnClient*)data;
	conn->m_ReadBuf = bufferevent_get_input(bev);
	conn->m_WriteBuf = bufferevent_get_output(bev);
	conn->m_ClientPtr->WriteEvent(conn);
}

void TcpClient::CloseEventCb(bufferevent * bev, short events, void * data)
{
	ConnClient *conn = (ConnClient*)data; 

	if (events & BEV_EVENT_CONNECTED)
	{
		conn->m_ReadBuf = bufferevent_get_input(bev);
		conn->m_WriteBuf = bufferevent_get_output(bev);
		conn->m_ClientPtr->ConnectionEvent(conn);
		return;
	}

	conn->m_ClientPtr->CloseEvent(conn, events);
	bufferevent_free(bev);
}

