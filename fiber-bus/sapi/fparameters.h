#pragma once
#include <string>
#include <unordered_map>
#include "../ci/cstring.h"
//#include "../sapi/csapi.h"

//using namespace fiber;

class fparameters
{
private:
	/*fiber::crequest::headers*/ std::unordered_map<ci::cstringview, ci::cstringview> headers;
	std::string URI;
	std::string msgId;
	/*fiber::crequest::payload*/ std::deque<ci::cstringview> payload;
	std::string responseCode = "";
	std::string response = "";
public:
	fparameters(std::unordered_map<ci::cstringview, ci::cstringview> hmap, std::string URI, std::string msgId, std::deque<ci::cstringview> payload);
	fparameters();
	~fparameters();
	std::string GetHeaderOption(char *option, char* def);
	std::string GetUri();
	std::string msgid();
	std::string GetPayload();
	void PutResponse(std::string code, std::string response);
	std::string GetResponse();
	std::string GetResponseCode();
	void init(std::unordered_map<ci::cstringview, ci::cstringview> hmap, std::string URI, std::string msgId, std::deque<ci::cstringview> payload);
};

extern fparameters parameters;