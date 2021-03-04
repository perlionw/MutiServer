#include "ProtoBuf.h"
#include <iostream>
#include <string>
#include <memory>
#include <string.h>
#include <vector>


void ProtoBuf::SerializeHeaderOrTail(char*buf, int index)
{
	buf[index] = 0xEB;
	buf[index + 1] = 0x90;
}

void ProtoBuf::SerializeSerialNumber(char * buf, long long serial_number, int index)
{
	buf[index] = (unsigned char)serial_number;
	buf[index + 1] = (unsigned char)(serial_number >> 8);
	buf[index + 2] = (unsigned char)(serial_number >> 16);
	buf[index + 3] = (unsigned char)(serial_number >> 24);
	buf[index + 4] = (unsigned char)(serial_number >> 32);
	buf[index + 5] = (unsigned char)(serial_number >> 40);
	buf[index + 6] = (unsigned char)(serial_number >> 48);
	buf[index + 7] = (unsigned char)(serial_number >> 56);
}

void ProtoBuf::DeserializeHeaderOrTail(const std::vector<char>& buf, char * dst_buf, int index)
{
	dst_buf[0] = buf[index];
	dst_buf[1] = buf[index + 1];
}

long long ProtoBuf::DeserializeSerialNumber(const std::vector<char>& buf, int index)
{
	long long serial_number;
	char* temp = (char*)&serial_number;
	temp[0] = buf[index];
	temp[1] = buf[index + 1];
	temp[2] = buf[index + 2];
	temp[3] = buf[index + 3];
	temp[4] = buf[index + 4];
	temp[5] = buf[index + 5];
	temp[6] = buf[index + 6];
	temp[6] = buf[index + 7];
	return serial_number;
}

ProtoBuf::ProtoBuf()
{
}


ProtoBuf::~ProtoBuf()
{
}

void ProtoBuf::SerializeHeader(char * buf, int index)
{
	SerializeHeaderOrTail(buf, index);
}

void ProtoBuf::SerializeSendSerialNumber(char * buf, long long serial_number, int index)
{
	SerializeSerialNumber(buf, serial_number, index);
}

void ProtoBuf::SerializeReceiveSerialNumber(char * buf, long long serial_number, int index)
{
	SerializeSerialNumber(buf, serial_number, index);
}

void ProtoBuf::SerializeSourceFlag(char * buf, SessionSourceFlag flag, int index)
{
	if (flag == SessionSourceFlag::REQUEST)
		buf[index] = 0x00;
	else
		buf[index] = 0x01;
}

void ProtoBuf::SerializeXmlLength(char * buf, int xml_length, int index)
{
	buf[index] = (unsigned char)xml_length;
	buf[index + 1] = (unsigned char)(xml_length >> 8);
	buf[index + 2] = (unsigned char)(xml_length >> 16);
	buf[index + 3] = (unsigned char)(xml_length >> 24);
}

void ProtoBuf::SerializeXmlContent(char * buf,const char * xml_buf, int xml_length, int index)
{
	memcpy(buf + index, xml_buf, xml_length);
}

void ProtoBuf::SerializeTail(char * buf, int index)
{
	SerializeHeaderOrTail(buf, index);
}

void ProtoBuf::DeserializaHeader(const std::vector<char>& buf , char * dst_buf, int index)
{
	DeserializeHeaderOrTail(buf, dst_buf, index);
}

long long ProtoBuf::DeserializeSendSerialNumber(const std::vector<char>& buf, int index)
{
	return DeserializeSerialNumber(buf, index);
}

long long ProtoBuf::DeserializeReceiveSerialNumber(const std::vector<char>& buf, int index)
{
	return DeserializeSerialNumber(buf, index);
}

SessionSourceFlag ProtoBuf::DeserializeSourceFlag(const std::vector<char>& buf, int index)
{
	return buf[index] == 0x00 ? REQUEST : RESPONSE;
}

int ProtoBuf::DeserializeXmlLength(const std::vector<char>& buf, int index)
{
	int xml_length;
	char*temp = (char*)&xml_length;
	temp[0] = buf[index];
	temp[1] = buf[index + 1];
	temp[2] = buf[index + 2];
	temp[3] = buf[index + 3];
	return xml_length;
}

void ProtoBuf::DeserializaXmlContent(const std::vector<char>& buf, char* xml_buf, int xml_length, int index)
{
	for (int i = index,j = 0; i < index + xml_length; ++i, ++j)
	{
		xml_buf[j] = buf[i];
	}
	//memcpy(xml_buf, buf + index, xml_length);
}

void ProtoBuf::DeserializaTail(const std::vector<char>& buf, char * dst_buf, int index)
{
	DeserializeHeaderOrTail(buf, dst_buf, index);
}
