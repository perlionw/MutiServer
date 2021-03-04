#pragma once
#include <iostream>
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
#include "ProtoBuf.h"
#include <vector>
#include <string>
#include <algorithm>
#include <mutex>
#include <thread>
static ProtoBuf proto_buf;
class TcpClient;
class ConnClient
{
	friend class TcpClient;
private:
	const int m_fd;             //socket的ID    
	evbuffer *m_ReadBuf;        //读数据的缓冲区    
	evbuffer *m_WriteBuf;       //写数据的缓冲区    
	TcpClient* m_ClientPtr;
	struct event_base* m_Base;
	std::mutex m_Cache_Mutex;
	std::vector<char> m_Buffer_Cache;
public:
	ConnClient(int fd = 0);
	~ConnClient();
	int GetFd() { return m_fd; }
	
	int WriteContentToBuffer(const ProtoContent& content);

private:
	//获取可读数据的长度    
	int GetReadBufferLen()
	{
		return evbuffer_get_length(m_ReadBuf);
	}

	//从读缓冲区中取出len个字节的数据，存入buffer中，若不够，则读出所有数据    
	//返回读出数据的字节数    
	int GetReadBuffer(char *buffer, int len)
	{
		return evbuffer_remove(m_ReadBuf, buffer, len);
	}

	//从读缓冲区中复制出len个字节的数据，存入buffer中，若不够，则复制出所有数据    
	//返回复制出数据的字节数    
	//执行该操作后，数据还会留在缓冲区中，buffer中的数据只是原数据的副本    
	int CopyReadBuffer(char *buffer, int len)
	{
		return evbuffer_copyout(m_ReadBuf, buffer, len);
	}

	//获取可写数据的长度    
	int GetWriteBufferLen()
	{
		return evbuffer_get_length(m_WriteBuf);
	}

	//将数据加入写缓冲区，准备发送    
	int AddToWriteBuffer(char *buffer, int len)
	{
		return evbuffer_add(m_WriteBuf, buffer, len);
	}

	//将读缓冲区中的数据移动到写缓冲区    
	void MoveBufferData()
	{
		evbuffer_add_buffer(m_WriteBuf, m_ReadBuf);
	}
};

class TcpClient
{
public:
	void ConnectServer(std::string, int port);
	void StartRun();	
	TcpClient();
	~TcpClient();
private:

	ConnClient* m_ConnClient;
	
	void ProcessBufferCacheThFunc(void *arg);

	static void ReadEventCb(struct bufferevent *bev, void *data);
	static void WriteEventCb(struct bufferevent *bev, void *data);
	static void CloseEventCb(struct bufferevent *bev, short events, void *data);

	

protected:
	//这五个虚函数，一般是要被子类继承，并在其中处理具体业务的    

	//新建连接成功后，会调用该函数    
	virtual void ConnectionEvent(ConnClient *conn) { }

	//读取完数据后，会调用该函数    
	virtual void ReadEvent(ConnClient *conn, ProtoContent content) { }

	//发送完成功后，会调用该函数（因为串包的问题，所以并不是每次发送完数据都会被调用）    
	virtual void WriteEvent(ConnClient *conn) { }

	//断开连接（客户自动断开或异常断开）后，会调用该函数    
	virtual void CloseEvent(ConnClient *conn, short events) { }

	

};

