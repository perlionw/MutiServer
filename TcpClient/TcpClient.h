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
	const int m_fd;             //socket��ID    
	evbuffer *m_ReadBuf;        //�����ݵĻ�����    
	evbuffer *m_WriteBuf;       //д���ݵĻ�����    
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
	//������麯����һ����Ҫ������̳У��������д������ҵ���    

	//�½����ӳɹ��󣬻���øú���    
	virtual void ConnectionEvent(ConnClient *conn) { }

	//��ȡ�����ݺ󣬻���øú���    
	virtual void ReadEvent(ConnClient *conn, ProtoContent content) { }

	//������ɹ��󣬻���øú�������Ϊ���������⣬���Բ�����ÿ�η��������ݶ��ᱻ���ã�    
	virtual void WriteEvent(ConnClient *conn) { }

	//�Ͽ����ӣ��ͻ��Զ��Ͽ����쳣�Ͽ����󣬻���øú���    
	virtual void CloseEvent(ConnClient *conn, short events) { }

	

};

