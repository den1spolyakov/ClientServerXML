#include "ParserWrapper.h"

#include <sstream>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace rapidxml;

std::string ParserWrapper::getValue(const std::string & tag)
{
	std::lock_guard<std::mutex> lock(fileMutex);
	xml_document<> doc;
	std::string value = errorMessage;
	std::string content = readFile();
	xml_node<> * rootNode = getRootNode(doc, content);
	
	if (rootNode == nullptr)
	{
		return value;
	}
	else
	{
		for (xml_node<> * pairNode = rootNode->first_node("pair"); pairNode != nullptr;
			pairNode = pairNode->next_sibling())
		{
			if (pairNode->first_node("tag")->value() == tag)
			{
				value = pairNode->first_node("value")->value();
				break;
			}
		}
	}
	return value;
}

void ParserWrapper::output(const std::string & out)
{
	std::ofstream file(filePath);
	if (file.is_open())
	{
		file << out;
		file.close();
	}
}

xml_node<> * ParserWrapper::getRootNode(xml_document<> & doc, std::string & content)
{	
	doc.parse<0>(&content[0]);
	return doc.first_node("pairs");
}

bool ParserWrapper::addPair(const std::string & tag, const std::string & value)
{
	std::lock_guard<std::mutex> lock(fileMutex);
	bool result = false;
	xml_document<> doc;
	std::string content = readFile();
	xml_node<> * rootNode = getRootNode(doc, content);

	if (rootNode == nullptr)
	{
		return result;
	}
	else
	{
		for (xml_node<> * pairNode = rootNode->first_node("pair"); pairNode != nullptr;
			pairNode = pairNode->next_sibling())
		{
			if (pairNode->first_node("tag")->value() == tag)
			{
				return result;
			}
		}
	
		xml_node<> * newPair = doc.allocate_node(node_element, "pair", " ");
		newPair->append_node(doc.allocate_node(node_element, "tag", &tag[0]));
		newPair->append_node(doc.allocate_node(node_element, "value", &value[0]));
		rootNode->append_node(newPair);
		result = true;

		std::string xml;
		print(std::back_inserter(xml), doc);
		output(xml);
		return result;
	}
}

bool ParserWrapper::changePair(const std::string & tag, const std::string & value)
{
	std::lock_guard<std::mutex> lock(fileMutex);
	xml_document<> doc;
	bool result = false;
	std::string content = readFile();
	doc.parse<parse_no_data_nodes>(&content[0]);
	xml_node<> * rootNode = doc.first_node("pairs");
	
	if (rootNode == nullptr)
	{
		return result;
	}
	else
	{
		for (xml_node<> * pairNode = rootNode->first_node("pair"); pairNode != nullptr &&
			result != true;	pairNode = pairNode->next_sibling())
		{
			if (pairNode->first_node("tag")->value() == tag)
			{
				pairNode->first_node("value")->value(&value[0]);
				result = true;
			}
		}
		std::string xml;
		print(std::back_inserter(xml), doc);
		output(xml);
		return result;
	}
}

std::string ParserWrapper::readFile()
{
	std::ifstream file(filePath);
	if (file.is_open())
	{
		std::stringstream temp;
		temp << file.rdbuf();
		file.close();
		return std::string(temp.str());
	}
	return "";
}

void ParserWrapper::setPath(const std::string & path)
{
	filePath = path;
	prepareFile();
}

std::string ParserWrapper::processQuery(const std::string & query)
{
	std::string result;
	std::istringstream iss(query);
	std::vector<std::string> tokens;
	std::copy(std::istream_iterator<std::string>(iss),
		std::istream_iterator<std::string>(),
		std::back_inserter<std::vector<std::string>>(tokens));

	if (tokens[OPERATION] == "get")
	{
		return tokens.size() == 2 ? getValue(tokens[TAG]) : errorMessage;
	} 
	else if (tokens[OPERATION] == "add")
	{
		return tokens.size() == 3 ? ( addPair(tokens[TAG], tokens[VALUE]) 
			? successfulInsertion : failedInsertion ) : errorMessage;
	}
	else if (tokens[OPERATION] == "set")
	{
		return tokens.size() == 3 ? (changePair(tokens[TAG], tokens[VALUE])
			? successfulChange : failedChange) : errorMessage;
	}
	return errorMessage;
}

void ParserWrapper::prepareFile()
{
	std::lock_guard<std::mutex> lock(fileMutex);
	xml_document<> doc;
	std::string content = readFile();
	if (getRootNode(doc, content) == nullptr)
	{
		output("<pairs></pairs>");
	}
}
