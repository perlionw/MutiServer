#pragma once
#include <vector>
#define BUFFER_SIZE 30000
enum SessionSourceFlag
{
	REQUEST, RESPONSE
};

enum ProtoIndex
{
	PROTO_HEADER = 0,
	PROTO_SENDSERIALNUM = 2,
	PROTO_RECEIVESERIALNUM = 10,
	PROTO_SOURCEFLAG = 18,
	PROTO_XMLLENGTH = 19,
	PROTO_XMLCONTENT = 23,
};

struct ProtoContent
{
	char begin_buf[2];
	long long send_serial_num;
	long long receive_serial_num;
	SessionSourceFlag session_source_flag;
	int xml_length;
	char xml_buf[BUFFER_SIZE];
	char end_buf[2];
};

class ProtoBuf
{
private:
	void SerializeHeaderOrTail(char *buf, int index);
	void SerializeSerialNumber(char* buf, long long serial_number, int index);

	void DeserializeHeaderOrTail(const std::vector<char>& buf, char* dst_buf, int index);
	long long DeserializeSerialNumber(const std::vector<char>& buf, int index);
public:
	ProtoBuf();
	~ProtoBuf();

	void SerializeHeader(char *buf, int index);
	void SerializeSendSerialNumber(char* buf, long long serial_number, int index);
	void SerializeReceiveSerialNumber(char* buf, long long serial_number, int index);
	void SerializeSourceFlag(char* buf, SessionSourceFlag flag, int index);
	void SerializeXmlLength(char* buf, int xml_length, int index);
	void SerializeXmlContent(char* buf, const char* xml_buf, int xml_length, int index);
	void SerializeTail(char *buf, int index);
	
	void DeserializaHeader(const std::vector<char>& buf, char* dst_buf, int index);
	long long DeserializeSendSerialNumber(const std::vector<char>& buf, int index);
	long long DeserializeReceiveSerialNumber(const std::vector<char>& buf, int index);

	SessionSourceFlag DeserializeSourceFlag(const std::vector<char>& buf, int index);
	int DeserializeXmlLength(const std::vector<char>& buf, int index);
	void DeserializaXmlContent(const std::vector<char>& buf, char* xml_buf, int xml_length, int index);
	void DeserializaTail(const std::vector<char>& buf, char* dst_buf, int index);
};

