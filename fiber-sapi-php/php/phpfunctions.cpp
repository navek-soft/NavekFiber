#include "php/20170718/main/php.h"
#include <sapi/embed/php_embed.h>
#include "zend_smart_str.h"
#include "ext/standard/php_standard.h"
#include "ext/standard/dl.h"
#include "../sapi/fparameters.h"

extern fparameters parameters;
	PHP_FUNCTION(fiber_header_option)
	{
		char *option, *value = NULL;
		size_t opt_len, val_len;
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "p|p", &option, &opt_len, &value, &val_len) == FAILURE) {
			return;
		}
		RETURN_STRING(parameters.GetHeaderOption(option, value).c_str());
	}

	ZEND_FUNCTION(fiber_geturi)
	{
		if (zend_parse_parameters_none() == FAILURE) {
			return;
		}
		RETURN_STRING(parameters.GetUri().c_str());
	}

	ZEND_FUNCTION(fiber_getmsgid)
	{
		if (zend_parse_parameters_none() == FAILURE) {
			return;
		}
		RETURN_STRING(parameters.msgid().c_str());
	}

	ZEND_FUNCTION(fiber_getpayload)
	{
		if (zend_parse_parameters_none() == FAILURE) {
			return;
		}
		auto&& data = parameters.GetPayload();
		zend_string *dest = zend_string_alloc(data.size() + 1, 0);
		auto val = (unsigned char *)ZSTR_VAL(dest);
		memcpy(val, data.data(), data.size());
		ZSTR_LEN(dest) = data.size();
		ZSTR_VAL(dest)[ZSTR_LEN(dest)] = '\0';
		RETURN_NEW_STR(dest);
	}

	ZEND_FUNCTION(fiber_putresponse)
	{
		char *content = nullptr, *code = nullptr;
		size_t content_length = 0, code_length = 0;
		if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "pp", &content, &content_length, &code, &code_length) == FAILURE) {
			return;
		}
		if (!content_length && !code_length)
			parameters.PutResponse(std::string(code, code_length), std::string(content, content_length));
		else
			parameters.PutResponse(code, content);
		RETURN_TRUE;
	}

	const zend_function_entry additional_functions[] = {
		ZEND_FE(fiber_putresponse, NULL)
		ZEND_FE(fiber_geturi, 	NULL)
		ZEND_FE(fiber_getmsgid, 	NULL)
		ZEND_FE(fiber_getpayload, 	NULL)
		ZEND_FE(fiber_header_option, NULL)
		{
	nullptr, nullptr, nullptr
}
	};
	extern EMBED_SAPI_API int sapi_php_init(int argc, char **argv, const zend_function_entry additional_functions[] TSRMLS_DC);
	extern EMBED_SAPI_API void sapi_php_shutdown(void);

	void php_embed_init() {
		char *argv[] = { "" };
		sapi_php_init(1, argv, additional_functions);
	}

	void php_embed_shutdown() {
		sapi_php_shutdown();
	}
