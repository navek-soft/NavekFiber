#include "fparameters.h"

fparameters parameters;

fparameters::fparameters(std::unordered_map<ci::cstringview, ci::cstringview> hmap, std::string URI, std::string msgId, std::deque<ci::cstringview> payload) : headers(hmap), URI(URI), msgId(msgId), payload(payload)
{
}

fparameters::fparameters()
{
}
	
fparameters::~fparameters()
{
}

std::string fparameters::GetHeaderOption(char* option, char* def)
{
	ci::cstringview copt(option, strlen(option));
	return headers.at(copt).trim().str();
}

std::string fparameters::GetUri()
{
	return URI;
}

std::string fparameters::msgid()
{
	return msgId;
}

std::string fparameters::GetPayload()
{
	std::string pload;
	for (auto&& s : payload) {
		pload.append(s.str());
	}
	return pload;
}

void fparameters::PutResponse(std::string code, std::string response)
{
	this->responseCode = code;
	this->response = response;
}

std::string fparameters::GetResponse()
{
	return response;
}

std::string fparameters::GetResponseCode()
{
	return responseCode;
}

void fparameters::init(std::unordered_map<ci::cstringview, ci::cstringview> hmap, std::string URI, std::string msgId, std::deque<ci::cstringview> payload)
{
	this->headers = hmap;
	this->URI = URI;
	this->msgId = msgId;
	this->payload = payload;
}
