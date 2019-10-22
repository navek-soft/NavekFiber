#include "cchannel.h"
#include <curl/curl.h>
#include <cstring>


using namespace fiber;

ssize_t cchannel::external_post(const std::string& url, const crequest::payload& data, size_t data_length, const crequest::response_headers& headers) {
	curl_global_init(CURL_GLOBAL_ALL);

	auto hcurl = curl_easy_init();
	struct curl_slist* hheaders = NULL;


	curl_easy_setopt(hcurl, CURLOPT_URL, url.c_str());
	//curl_easy_setopt(hcurl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(hcurl, CURLOPT_USERAGENT, "NavekFiber");
	curl_easy_setopt(hcurl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(hcurl, CURLOPT_POST, 1L);

	for (auto&& h : headers) {
		std::string sheader(h.first);
		sheader.append(": ").append(h.second);
		hheaders = curl_slist_append(hheaders, sheader.c_str());
	}
	curl_easy_setopt(hcurl, CURLOPT_HTTPHEADER, hheaders);

	auto buffer = (uint8_t*)alloca(data_length + 1);
	size_t offset = 0;
	for (auto&& p : data) {
		memcpy(buffer + offset, p.data(), p.length());
		offset += p.length();
	}
	buffer[data_length] = '\0';

	curl_easy_setopt(hcurl, CURLOPT_POSTFIELDS, buffer);

	CURLcode res = curl_easy_perform(hcurl);
	curl_slist_free_all(hheaders);


	if (res == CURLE_OK) {
		long response_code;
		curl_easy_getinfo(hcurl, CURLINFO_RESPONSE_CODE, &response_code);
		curl_easy_cleanup(hcurl);
		printf("CALL(%s) HTTP (%ld) <%s> - %d (%s)\n", url.c_str(), response_code, buffer, res, curl_easy_strerror(res));

		return response_code;
	}
	else {
		printf("CALL(%s) <%s> - %d (%s)\n", url.c_str(), buffer, res, curl_easy_strerror(res));

	}
	curl_easy_cleanup(hcurl);
	return -res;
}


crequest::response_headers cchannel::make_headers(const cmsgid& msg_id, std::time_t ctime, std::time_t mtime, uint16_t status, const std::string name, const crequest::response_headers& additional, const crequest::headers& reqeaders)
{
	tm ctm, mtm;
	localtime_r(&ctime, &ctm);
	localtime_r(&mtime, &mtm);

	crequest::response_headers hdrslist({
		{ "X-FIBER-MSG-QUEUE", name },
		{ "X-FIBER-MSG-ID",msg_id.str() },
		{ "X-FIBER-MSG-CTIME",ci::cstringformat("%04d-%02d-%02d %02d:%02d:%02d",ctm.tm_year + 1900,ctm.tm_mon,ctm.tm_mday,ctm.tm_hour,ctm.tm_min,ctm.tm_sec).str() },
		{ "X-FIBER-MSG-MTIME",ci::cstringformat("%04d-%02d-%02d %02d:%02d:%02d",mtm.tm_year + 1900,mtm.tm_mon,mtm.tm_mday,mtm.tm_hour,mtm.tm_min,mtm.tm_sec).str() },
	});
	
	if (!additional.empty()) {
		hdrslist.insert(additional.begin(), additional.end());
	}

	if (status) {
		std::string statusText;
		if (status & crequest::accepted) { statusText.append("accepted,"); }
		if (status & crequest::enqueued) { statusText.append("enqueued,"); }
		if (status & crequest::pushed) { statusText.append("pushed,"); }
		if (status & crequest::execute) { statusText.append("execute,"); }
		if (status & crequest::bad) { statusText.append("bad,"); }
		if (status & crequest::again) { statusText.append("again,"); }
		if (status & crequest::answering) { statusText.append("answering,"); }
		if (status & crequest::stored) { statusText.append("stored,"); }
		if (status & crequest::complete) { statusText.append("complete,"); }
		statusText.pop_back();
		hdrslist.emplace("X-FIBER-MSG-STATUS", statusText);
	}

	if (!reqeaders.empty()) {
		for (auto&& h : reqeaders) {
			if (!h.first.empty()) {
				hdrslist.emplace(h.first.trim().str(), h.second.trim().str());
			}
		}
	}
	return hdrslist;
}