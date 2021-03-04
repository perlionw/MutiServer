#include "XmlPacket.h"

std::string get_localtime()
{
	struct tm *t;
	time_t tt;
	time(&tt);
	t = localtime(&tt);
	char time[1024] = {};
	sprintf(time, "%4d-%02d-%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	return time;
}

std::string XmlPacket::GeneralXml()
{
	rapidxml::xml_document<> createXML;
	auto nodeDecl = createXML.allocate_node(rapidxml::node_declaration);
	nodeDecl->append_attribute(createXML.allocate_attribute("version", "1.0"));
	nodeDecl->append_attribute(createXML.allocate_attribute("encoding", "UTF-8"));
	createXML.append_node(nodeDecl);
	auto nodeRoot = createXML.allocate_node(rapidxml::node_element, "robot");
	auto nodeLangrage = createXML.allocate_node(rapidxml::node_element, "SendCode", "111");
	nodeRoot->append_node(nodeLangrage);
	nodeLangrage = createXML.allocate_node(rapidxml::node_element, "ReceiveCode", "testreceivecode");
	nodeRoot->append_node(nodeLangrage);
	nodeLangrage = createXML.allocate_node(rapidxml::node_element, "Type", "251");
	nodeRoot->append_node(nodeLangrage);
	nodeLangrage = createXML.allocate_node(rapidxml::node_element, "Code", "");
	nodeRoot->append_node(nodeLangrage);
	nodeLangrage = createXML.allocate_node(rapidxml::node_element, "Command", "1");
	nodeRoot->append_node(nodeLangrage);
	std::string localTime = get_localtime();
	nodeLangrage = createXML.allocate_node(rapidxml::node_element, "Time", localTime.c_str());
	nodeRoot->append_node(nodeLangrage);
	nodeLangrage = createXML.allocate_node(rapidxml::node_element, "Items", "");
	nodeRoot->append_node(nodeLangrage);

	createXML.append_node(nodeRoot);
	std::string strXML = "";
	rapidxml::print(std::back_inserter(strXML), createXML, 0);
	return strXML;
}

XmlPacket::XmlPacket()
{
}


XmlPacket::~XmlPacket()
{
}

std::string XmlPacket::RegisterXml()
{
	return GeneralXml();
}

std::string XmlPacket::TestXml(TestStruct test)
{
	return "fsaagasg";
}
