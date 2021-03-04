//MultiServer.cpp    
#include "MutiServer.h"    

Conn::Conn(int fd) : m_fd(fd)
{
	m_Prev = NULL;
	m_Next = NULL;
}

Conn::~Conn()
{

}

int Conn::WriteContentToBuffer(const ProtoContent & content)
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

ConnQueue::ConnQueue()
{
	//建立头尾结点，并调整其指针    
	m_head = new Conn(0);
	m_tail = new Conn(0);
	m_head->m_Prev = m_tail->m_Next = NULL;
	m_head->m_Next = m_tail;
	m_tail->m_Prev = m_head;
}

ConnQueue::~ConnQueue()
{
	Conn *tcur, *tnext;
	tcur = m_head;
	//循环删除链表中的各个结点    
	while (tcur != NULL)
	{
		tnext = tcur->m_Next;
		delete tcur;
		tcur = tnext;
	}
}

Conn *ConnQueue::InsertConn(int fd, LibeventThread *t)
{
	Conn *c = new Conn(fd);

	c->m_Thread = t;
	Conn *next = m_head->m_Next;

	c->m_Prev = m_head;
	c->m_Next = m_head->m_Next;
	m_head->m_Next = c;
	next->m_Prev = c;
	return c;
}

void ConnQueue::DeleteConn(Conn *c)
{
	c->m_Prev->m_Next = c->m_Next;
	c->m_Next->m_Prev = c->m_Prev;
	delete c;
}

/*
void ConnQueue::PrintQueue()
{
Conn *cur = m_head->m_Next;
while( cur->m_Next != NULL )
{
printf("%d ", cur->m_fd);
cur = cur->m_Next;
}
printf("\n");
}
*/

MultiServer::MultiServer(int count): m_MainBase(NULL), m_Threads(NULL), m_StopFlag(false)
{
	evthread_use_pthreads();
	//初始化各项数据    
	m_ThreadCount = count;
	m_Port = -1;
	m_MainBase = new LibeventThread;
	m_Threads = new LibeventThread[m_ThreadCount];
	m_MainBase->tid = pthread_self();
	m_MainBase->base = event_base_new();
	memset(m_SignalEvents, 0, sizeof(m_SignalEvents));

	//初始化各个子线程的结构体    
	for (int i = 0; i < m_ThreadCount; i++)
	{
		SetupThread(&m_Threads[i]);
	}
}

MultiServer::~MultiServer()
{
	//停止事件循环（如果事件循环没开始，则没效果）    
	StopRun(NULL);
	m_StopFlag = true;
	//释放内存    
	event_base_free(m_MainBase->base);
	for (int i = 0; i < m_ThreadCount; i++)
	{
		event_free(m_Threads[i].notifyEvent);
		event_base_free(m_Threads[i].base);
	}


	delete m_MainBase;
	delete[] m_Threads;
}

void MultiServer::SetupThread(LibeventThread *me)
{
	int res;
	evthread_use_pthreads();
	//建立libevent事件处理机制    
	me->tcpConnect = this;
	me->base = event_base_new();
	assert(me->base != NULL);

	//在主线程和子线程之间建立管道    
	int fds[2];
	res = pipe(fds);
	assert(res == 0);
	me->notifyReceiveFd = fds[0];
	me->notifySendFd = fds[1];

	//让子线程的状态机监听管道    
	me->notifyEvent = event_new(me->base, me->notifyReceiveFd,
		EV_READ | EV_PERSIST, ThreadProcess, me);
	//int ret = event_base_set(me->base, &me->notifyEvent); //使用栈区空间会assert
	res = event_add(me->notifyEvent, 0);
	assert(res == 0);
}

void MultiServer::ProcessBufferCacheThFunc()
{
	while (!m_StopFlag)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		std::lock_guard<decltype(m_Cache_Mutex)> lock(m_Cache_Mutex);
		std::map<int, ConnBufferCache >::iterator it = m_Conn_Buffer_Cache.begin();
		for ( ;it != this->m_Conn_Buffer_Cache.end(); ++it)
		{
			int buffer_size = it->second.cache.size();
			if (buffer_size < PROTO_HEADER + 2)//大于解析头部所需字节
				continue;

			ProtoContent content;
			proto_buf.DeserializaHeader(it->second.cache, content.begin_buf, PROTO_HEADER);
			unsigned char begin_buf0 = content.begin_buf[0];
			unsigned char begin_buf1 = content.begin_buf[1];
			if (begin_buf0 != 0xEB || begin_buf1 != 0x90)
				continue;

			if (buffer_size < PROTO_XMLLENGTH + 4)//大于解析xml长度所需字节
				continue;

			content.xml_length = proto_buf.DeserializeXmlLength(it->second.cache, PROTO_XMLLENGTH);
			if (buffer_size < PROTO_XMLCONTENT + content.xml_length + 2)//大于解析尾部所需字节
				continue;

			proto_buf.DeserializaTail(it->second.cache, content.end_buf, PROTO_XMLCONTENT + content.xml_length);
			unsigned char end_buf0 = content.end_buf[0];
			unsigned char end_buf1 = content.end_buf[1];
			if (end_buf0 != 0xEB || end_buf1 != 0x90)
				continue;

			content.session_source_flag = proto_buf.DeserializeSourceFlag(it->second.cache, PROTO_SOURCEFLAG);
			content.send_serial_num = proto_buf.DeserializeSendSerialNumber(it->second.cache, PROTO_SENDSERIALNUM);
			content.receive_serial_num = proto_buf.DeserializeReceiveSerialNumber(it->second.cache, PROTO_RECEIVESERIALNUM);
			proto_buf.DeserializaXmlContent(it->second.cache, content.xml_buf, content.xml_length, PROTO_XMLCONTENT);
			it->second.conn->m_Thread->tcpConnect->ReadEvent(it->second.conn, content);
			{
				it->second.cache.erase(it->second.cache.begin(), it->second.cache.begin() + PROTO_XMLCONTENT + content.xml_length + 2);
			}
		}
	}
}

void MultiServer::InsertConnBufferCache(std::map< int, ConnBufferCache>& conn_cache_map, ConnBufferCache conn_buffer_cache)
{
	auto it = conn_cache_map.find(conn_buffer_cache.conn->m_fd);
	if (it != conn_cache_map.end())
	{
		it->second = conn_buffer_cache;
	}
	else
	{
		conn_cache_map.insert({ conn_buffer_cache.conn->m_fd, conn_buffer_cache });
	}
}

void MultiServer::DeleteConnBufferCache(std::map<int, ConnBufferCache>& conn_cache_map, int fd)
{
	conn_cache_map.erase(fd);
}

void MultiServer::InsertDataToConnBufferCache(std::map<int, ConnBufferCache>& conn_cache_map, int fd, const char * buf, int len)
{
	auto it = conn_cache_map.find(fd);
	if (it != conn_cache_map.end())
	{
		for (unsigned int i = 0; i < len; ++i)
			it->second.cache.push_back(buf[i]);
	}
}

void *MultiServer::WorkerLibevent(void *arg)
{
	//开启libevent的事件循环，准备处理业务    
	LibeventThread *me = (LibeventThread*)arg;
	//printf("thread %u started\n", (unsigned int)me->tid);    
	event_base_dispatch(me->base);
	//printf("subthread done\n");    
}

bool MultiServer::StartRun()
{
	evconnlistener *listener;
	
	//如果端口号不是EXIT_CODE，就监听该端口号    
	if (m_Port != EXIT_CODE)
	{
		sockaddr_in sin;
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(m_Port);
		listener = evconnlistener_new_bind(m_MainBase->base,
			ListenerEventCb, (void*)this,
			LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE, -1,
			(sockaddr*)&sin, sizeof(sockaddr_in));
		if (NULL == listener)
		{
			fprintf(stderr, "listen error: %s\n", strerror(errno));
			exit(1);
		}
	}

	std::thread *th = new std::thread(&MultiServer::ProcessBufferCacheThFunc, this);
	th->detach();

	//开启各个子线程    
	for (int i = 0; i < m_ThreadCount; i++)
	{
		pthread_create(&m_Threads[i].tid, NULL,
			WorkerLibevent, (void*)&m_Threads[i]);
	}

	//开启主线程的事件循环    
	event_base_dispatch(m_MainBase->base);

	//事件循环结果，释放监听者的内存    
	if (m_Port != EXIT_CODE)
	{
		//printf("free listen\n");    
		evconnlistener_free(listener);
	}
}

void MultiServer::StopRun(timeval *tv)
{
	int contant = EXIT_CODE;
	m_StopFlag = true;
	//向各个子线程的管理中写入EXIT_CODE，通知它们退出    
	for (int i = 0; i < m_ThreadCount; i++)
	{
		write(m_Threads[i].notifySendFd, &contant, sizeof(int));
	}
	//结果主线程的事件循环    
	event_base_loopexit(m_MainBase->base, tv);
}

void MultiServer::ListenerEventCb(struct evconnlistener *listener,
	evutil_socket_t fd,
struct sockaddr *sa,
	int socklen,
	void *user_data)
{
	MultiServer *server = (MultiServer*)user_data;

	//随机选择一个子线程，通过管道向其传递socket描述符    
	int num = rand() % server->m_ThreadCount;
	int sendfd = server->m_Threads[num].notifySendFd;
	write(sendfd, &fd, sizeof(evutil_socket_t));
}

void MultiServer::ThreadProcess(int fd, short which, void *arg)
{
	LibeventThread *me = (LibeventThread*)arg;

	//从管道中读取数据（socket的描述符或操作码）    
	int pipefd = me->notifyReceiveFd;
	evutil_socket_t confd;
	read(pipefd, &confd, sizeof(evutil_socket_t));

	//如果操作码是EXIT_CODE，则终于事件循环    
	if (EXIT_CODE == confd)
	{
		event_base_loopbreak(me->base);
		return;
	}

	//新建连接    
	struct bufferevent *bev;
	bev = bufferevent_socket_new(me->base, confd, BEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE);
	if (!bev)
	{
		fprintf(stderr, "Error constructing bufferevent!");
		event_base_loopbreak(me->base);
		return;
	}

	//将该链接放入队列    
	Conn *conn = me->connectQueue.InsertConn(confd, me);

	std::vector<char> cache;
	ConnBufferCache conn_buffer_cache;
	conn_buffer_cache.conn = conn;
	conn_buffer_cache.cache = cache;
	std::lock_guard<decltype(conn->m_Thread->tcpConnect->m_Cache_Mutex)> lock(conn->m_Thread->tcpConnect->m_Cache_Mutex);
	me->tcpConnect->InsertConnBufferCache(me->tcpConnect->m_Conn_Buffer_Cache, conn_buffer_cache);

	//准备从socket中读写数据    
	bufferevent_setcb(bev, ReadEventCb, WriteEventCb, CloseEventCb, conn);
	bufferevent_enable(bev, EV_WRITE);
	bufferevent_enable(bev, EV_READ);

	//调用用户自定义的连接事件处理函数    
	me->tcpConnect->ConnectionEvent(conn);
}

void MultiServer::ReadEventCb(struct bufferevent *bev, void *data)
{
	Conn *conn = (Conn*)data;
	conn->m_ReadBuf = bufferevent_get_input(bev);
	conn->m_WriteBuf = bufferevent_get_output(bev);

	char buf[BUFFER_SIZE];
	int len = conn->GetReadBuffer(buf, BUFFER_SIZE);
	{
		std::lock_guard<decltype(conn->m_Thread->tcpConnect->m_Cache_Mutex)> lock(conn->m_Thread->tcpConnect->m_Cache_Mutex);
		conn->m_Thread->tcpConnect->InsertDataToConnBufferCache(conn->m_Thread->tcpConnect->m_Conn_Buffer_Cache, conn->m_fd, buf, len);
	}
}

void MultiServer::WriteEventCb(struct bufferevent *bev, void *data)
{
	Conn *conn = (Conn*)data;
	conn->m_ReadBuf = bufferevent_get_input(bev);
	conn->m_WriteBuf = bufferevent_get_output(bev);
	//调用用户自定义的写入事件处理函数    
	conn->m_Thread->tcpConnect->WriteEvent(conn);

}

void MultiServer::CloseEventCb(struct bufferevent *bev, short events, void *data)
{
	Conn *conn = (Conn*)data;
	//调用用户自定义的断开事件处理函数    
	conn->m_Thread->tcpConnect->CloseEvent(conn, events);
	{
		std::lock_guard<decltype(conn->m_Thread->tcpConnect->m_Cache_Mutex)> lock(conn->m_Thread->tcpConnect->m_Cache_Mutex);
		conn->m_Thread->tcpConnect->DeleteConnBufferCache(conn->m_Thread->tcpConnect->m_Conn_Buffer_Cache, conn->m_fd);
	}
	conn->m_Thread->connectQueue.DeleteConn(conn);
	
	bufferevent_free(bev);
}

bool MultiServer::AddSignalEvent(int sig, void(*ptr)(int, short, void*))
{
	if (sig >= MAX_SIGNAL)
		return false;

	//新建一个信号事件    
	event *ev = evsignal_new(m_MainBase->base, sig, ptr, (void*)this);
	if (!ev ||
		event_add(ev, NULL) < 0)
	{
		event_del(ev);
		return false;
	}

	//删除旧的信号事件（同一个信号只能有一个信号事件）   
	if (NULL != m_SignalEvents[sig])
		DeleteSignalEvent(sig);
	m_SignalEvents[sig] = ev;

	return true;
}

bool MultiServer::DeleteSignalEvent(int sig)
{
	event *ev = m_SignalEvents[sig];
	if (sig >= MAX_SIGNAL || NULL == ev)
		return false;

	event_del(ev);
	ev = NULL;
	return true;
}

event *MultiServer::AddTimerEvent(void(*ptr)(int, short, void *),
	timeval tv, bool once)
{
	int flag = 0;
	if (!once)
		flag = EV_PERSIST;

	//新建定时器信号事件    
	event *ev = event_new(m_MainBase->base, -1, flag, ptr, this);
	if (event_add(ev, &tv) < 0)
	{
		event_del(ev);
		return NULL;
	}
	return ev;
}

bool MultiServer::DeleteTImerEvent(event *ev)
{
	int res = event_del(ev);
	return (0 == res);
}