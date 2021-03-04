//MultiServer.h    
#ifndef MULTISERVER_H_    
#define MULTISERVER_H_    

#include <stdio.h>    
#include <stdlib.h>    
#include <unistd.h>    
#include <string.h>    
#include <errno.h>    
#include <signal.h>    
#include <time.h>    
#include <pthread.h>    
#include <fcntl.h>    
#include <assert.h>  

#include <event2/event.h>    
#include <event2/event_compat.h>
#include <event2/bufferevent.h>    
#include <event2/buffer.h>    
#include <event2/listener.h>    
#include <event2/util.h>    
#include <event2/event.h>  
#include <event2/thread.h>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include "ProtoBuf.h"

class MultiServer;
class Conn;
class ConnQueue;
struct LibeventThread;
static ProtoBuf proto_buf;
//�����һ������Ľ���࣬�����洢�������ӵ���Ϣ��    
//���ṩ�˶�д���ݵĽӿ�    
class Conn
{
	//����ֻ����TcpBaseServer������    
	//����ConnQueue�����    
	friend class ConnQueue;
	friend class MultiServer;
	friend class TcpClient;

private:
	const int m_fd;             //socket��ID    
	evbuffer *m_ReadBuf;        //�����ݵĻ�����    
	evbuffer *m_WriteBuf;       //д���ݵĻ�����    

	Conn *m_Prev;               //ǰһ������ָ��    
	Conn *m_Next;               //��һ������ָ��    
	LibeventThread *m_Thread;

	
	Conn(int fd = 0);
	~Conn();
public:

	int WriteContentToBuffer(const ProtoContent& content);
	LibeventThread *GetThread() { return m_Thread; }
	int GetFd() { return m_fd; }
private:

	//��ȡ�ɶ����ݵĳ���    
	int GetReadBufferLen()
	{
		return evbuffer_get_length(m_ReadBuf);
	}

	//�Ӷ���������ȡ��len���ֽڵ����ݣ�����buffer�У����������������������    
	//���ض������ݵ��ֽ���    
	int GetReadBuffer(char *buffer, int len)
	{
		return evbuffer_remove(m_ReadBuf, buffer, len);
	}

	//�Ӷ��������и��Ƴ�len���ֽڵ����ݣ�����buffer�У������������Ƴ���������    
	//���ظ��Ƴ����ݵ��ֽ���    
	//ִ�иò��������ݻ������ڻ������У�buffer�е�����ֻ��ԭ���ݵĸ���    
	int CopyReadBuffer(char *buffer, int len)
	{
		return evbuffer_copyout(m_ReadBuf, buffer, len);
	}

	//��ȡ��д���ݵĳ���    
	int GetWriteBufferLen()
	{
		return evbuffer_get_length(m_WriteBuf);
	}

	//�����ݼ���д��������׼������    
	int AddToWriteBuffer(char *buffer, int len)
	{
		return evbuffer_add(m_WriteBuf, buffer, len);
	}

	//�����������е������ƶ���д������    
	void MoveBufferData()
	{
		evbuffer_add_buffer(m_WriteBuf, m_ReadBuf);
	}

};

//��ͷβ����˫�����࣬ÿ�����洢һ�����ӵ�����    
class ConnQueue
{
private:
	Conn *m_head;
	Conn *m_tail;
public:
	ConnQueue();
	~ConnQueue();
	Conn *InsertConn(int fd, LibeventThread *t);
	void DeleteConn(Conn *c);
	//void PrintQueue();    
};

//ÿ�����̵߳��߳���Ϣ    
struct LibeventThread
{
	pthread_t tid;              //�̵߳�ID    
	struct event_base *base;    //libevent���¼������    
	struct event* notifyEvent;   //����������¼���    
	int notifyReceiveFd;        //����Ľ��ն�    
	int notifySendFd;           //�ܵ��ķ��Ͷ�    
	ConnQueue connectQueue;     //socket���ӵ�����    

	//��libevent���¼�������Ҫ�õ��ܶ�ص�����������ʹ����������thisָ��    
	//������������ʽ��TcpBaseServer����ָ�봫��ȥ    
	MultiServer *tcpConnect;  //TcpBaseServer���ָ��    
};

struct ConnBufferCache
{
	Conn* conn;
	std::vector<char> cache;
};

class MultiServer
{
private:
	static const int EXIT_CODE = -1;
	static const int MAX_SIGNAL = 256;

private:
	int m_ThreadCount;                  //���߳���    
	int m_Port;                         //�����Ķ˿�    
	LibeventThread *m_MainBase;         //���̵߳�libevent�¼������    
	LibeventThread *m_Threads;          //�洢�������߳���Ϣ������    
	event *m_SignalEvents[MAX_SIGNAL];  //�Զ�����źŴ���    

	std::mutex m_Cache_Mutex;
	std::map< int, ConnBufferCache> m_Conn_Buffer_Cache; //�������ӵĻ�����
	bool m_StopFlag;
private:
	//��ʼ�����̵߳�����    
	void SetupThread(LibeventThread *thread);

	void ProcessBufferCacheThFunc();

	void InsertConnBufferCache(std::map< int, ConnBufferCache>& conn_cache_map, ConnBufferCache conn_buffer_cache);
	void DeleteConnBufferCache(std::map< int, ConnBufferCache>& conn_cache_map, int fd);
	void InsertDataToConnBufferCache(std::map< int, ConnBufferCache>& conn_cache_map, int fd, const char* buf, int len);
	//���̵߳����ź���    
	static void *WorkerLibevent(void *arg);
	//�����߳��յ�����󣩣���Ӧ���̵߳Ĵ�����    
	static void ThreadProcess(int fd, short which, void *arg);
	//��libevent�ص��ĸ�����̬����    
	static void ListenerEventCb(evconnlistener *listener, evutil_socket_t fd,
		sockaddr *sa, int socklen, void *user_data);
	static void ReadEventCb(struct bufferevent *bev, void *data);
	static void WriteEventCb(struct bufferevent *bev, void *data);
	static void CloseEventCb(struct bufferevent *bev, short events, void *data);


protected:
	//������麯����һ����Ҫ������̳У��������д������ҵ���    

	//�½����ӳɹ��󣬻���øú���    
	virtual void ConnectionEvent(Conn *conn) { }

	//��ȡ�����ݺ󣬻���øú���    
	virtual void ReadEvent(Conn *conn, ProtoContent content) { }

	//������ɹ��󣬻���øú�������Ϊ���������⣬���Բ�����ÿ�η��������ݶ��ᱻ���ã�    
	virtual void WriteEvent(Conn *conn) { }

	//�Ͽ����ӣ��ͻ��Զ��Ͽ����쳣�Ͽ����󣬻���øú���    
	virtual void CloseEvent(Conn *conn, short events) { }

public:
	MultiServer(int count);
	~MultiServer();

	//���ü����Ķ˿ںţ��������Ҫ�������뽫������ΪEXIT_CODE    
	void SetPort(int port)
	{
		m_Port = port;
	}

	//��ʼ�¼�ѭ��    
	bool StartRun();
	//��tvʱ��������¼�ѭ��    
	//��tvΪ�գ�������ֹͣ    
	void StopRun(timeval *tv);

	//��Ӻ�ɾ���źŴ����¼�    
	//sig���źţ�ptrΪҪ�ص��ĺ���    
	bool AddSignalEvent(int sig, void(*ptr)(int, short, void*));
	bool DeleteSignalEvent(int sig);

	//��Ӻ�ɾ����ʱ�¼�    
	//ptrΪҪ�ص��ĺ�����tv�Ǽ��ʱ�䣬once�����Ƿ�ִֻ��һ��    
	event *AddTimerEvent(void(*ptr)(int, short, void*),
		timeval tv, bool once);
	bool DeleteTImerEvent(event *ev);
};

#endif