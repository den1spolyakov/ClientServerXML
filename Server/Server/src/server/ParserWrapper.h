#pragma once

#include <mutex>
#include <string>

#include "../rapidxml/rapidxml_print.hpp"

const std::string errorMessage = "Wrong query! Try again.";
const std::string successfulInsertion = "The pair was successfully inserted.";
const std::string failedInsertion = "Pair with this tag already exists.";
const std::string successfulChange = "Value was successfully changed.";
const std::string failedChange = "Pair with such tag doesn't exist.";


class ParserWrapper
{
private:
	std::string readFile();
	rapidxml::xml_node<> * getRootNode(rapidxml::xml_document<> & doc, std::string & content);
	void output(const std::string & out);

	enum {OPERATION, TAG, VALUE};
	std::mutex fileMutex;
	std::string filePath;
public:
	ParserWrapper() {}
	~ParserWrapper() {}

	void setPath(const std::string & path);
	bool addPair(const std::string & tag, const std::string & value);
	bool changePair(const std::string & tag, const std::string & value);
	std::string getValue(const std::string & tag);
	std::string processQuery(const std::string & query);
	void prepareFile();
};