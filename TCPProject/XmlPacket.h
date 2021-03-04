#pragma once
#include <iostream>
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_iterators.hpp"
#include "rapidxml/rapidxml_print.hpp"
#include "rapidxml/rapidxml_utils.hpp"'

struct TestStruct
{
	int i;
	int j;
};
class XmlPacket
{

private:
	static std::string GeneralXml();

public:
	XmlPacket();
	~XmlPacket();

	static std::string RegisterXml();

	static std::string TestXml(TestStruct test);
};

