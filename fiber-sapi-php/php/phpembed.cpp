#include <TSRM/TSRM.h>
#include <main/php.h>
#include <main/SAPI.h>
#include <main/php_main.h>
#include <main/php_variables.h>
#include <main/php_ini.h>
#include <zend_ini.h>
#include "zend_smart_str.h"
#include "ext/standard/php_standard.h"
#include "ext/standard/dl.h"
#include "../sapi/fparameters.h"
#include "sapi/embed/php_embed.h"

static const char HARDCODED_INI[] =
"html_errors=0\n"
"register_argc_argv=1\n"
"implicit_flush=1\n"
"output_buffering=0\n"
"max_execution_time=0\n"
"max_input_time=-1\n\0";

#ifndef PHP_WIN32
#define EMBED_SAPI_API SAPI_API
#else
#define EMBED_SAPI_API
#endif

#ifdef ZTS
ZEND_TSRMLS_CACHE_EXTERN()
#endif

void php_embed_param_init(std::unordered_map<ci::cstringview, ci::cstringview> hmap, std::string URI, std::string msgId, std::deque<ci::cstringview> payload) {
	parameters.init(hmap, URI, msgId, payload);
}

int php_embed_execute(ci::cstringformat& result, const std::string& msgid, const std::string& method, const std::string& url, const std::string& script, const std::deque<ci::cstringview>& payload) {
	try {
		zend_file_handle zscript;
		zscript.type = ZEND_HANDLE_FP;
		zscript.opened_path = NULL;
		zscript.free_filename = 0;
		zscript.filename = script.c_str();

		if (zscript.handle.fp = fopen(zscript.filename, "r"); zscript.handle.fp != NULL) {
			result.append("%s", parameters.GetResponse().c_str());
			//char *argv[] = {""};
			//sapi_php_init(1, argv, additional_functions);
			zend_first_try
			{
				php_execute_script(&zscript TSRMLS_CC);
			}
				zend_catch{}
			zend_end_try();
			//sapi_php_shutdown();
			result.append(parameters.GetResponse());
			return 200;
		}
		else
		{
			return 404;
		}
	}
	catch (...) {
		printf("csapi_php: exception(%s).\n", "internal php error");
	}
	return 404;

}


static char* php_embed_read_cookies(void)
{
	return NULL;
}

static inline size_t php_embed_single_write(const char *str, size_t str_length)
{
#ifdef PHP_WRITE_STDOUT
	zend_long ret;

	ret = write(STDOUT_FILENO, str, str_length);
	if (ret <= 0) return 0;
	return ret;
#else
	size_t ret;

	ret = fwrite(str, 1, MIN(str_length, 16384), stdout);
	return ret;
#endif
}

static size_t php_embed_ub_write(const char *str, size_t str_length)
{
	const char *ptr = str;
	size_t remaining = str_length;
	size_t ret;

	while (remaining > 0) {
		ret = php_embed_single_write(ptr, remaining);
		if (!ret) {
			php_handle_aborted_connection();
		}
		ptr += ret;
		remaining -= ret;
	}

	return str_length;
}

static void php_embed_flush(void *server_context)
{
	if (fflush(stdout) == EOF) {
		php_handle_aborted_connection();
	}
}

static void php_embed_send_header(sapi_header_struct *sapi_header, void *server_context)
{
}

static void php_embed_log_message(char *message, int syslog_type_int)
{
	fprintf(stderr, "%s\n", message);
}

static void php_embed_register_variables(zval *track_vars_array)
{
	php_import_environment_variables(track_vars_array);
}

static int php_embed_startup(sapi_module_struct *sapi_module)
{
	if (php_module_startup(sapi_module, NULL, 0) == FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

static int php_embed_deactivate(void)
{
	return SUCCESS;
}
bool engine;


EMBED_SAPI_API sapi_module_struct php_embed_module = {
	"fiber-sapi-php",                       /* name */
	"PHP Fiber SAPI",        /* pretty name */

	php_embed_startup,              /* startup */
	php_module_shutdown_wrapper,   /* shutdown */

	NULL,                          /* activate */
	php_embed_deactivate,           /* deactivate */

	php_embed_ub_write,             /* unbuffered write */
	php_embed_flush,                /* flush */
	NULL,                          /* get uid */
	NULL,                          /* getenv */

	php_error,                     /* error handler */

	NULL,                          /* header handler */
	NULL,                          /* send headers handler */
	php_embed_send_header,          /* send header handler */

	NULL,                          /* read POST data */
	php_embed_read_cookies,         /* read Cookies */

	php_embed_register_variables,   /* register server variables */
	php_embed_log_message,          /* Log message */
	NULL,							/* Get request time */
	NULL,							/* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};

BEGIN_EXTERN_C()
void zend_signal_startup(void);
END_EXTERN_C()


EMBED_SAPI_API int sapi_php_init(int argc, char **argv, const zend_function_entry additional_functions[] TSRMLS_DC)
{
	zend_llist global_vars;

#if defined(SIGPIPE) && defined(SIG_IGN)
	signal(SIGPIPE, SIG_IGN); /* ignore SIGPIPE in standalone mode so
								 that sockets created via fsockopen()
								 don't kill PHP if the remote site
								 closes it.  in apache|apxs mode apache
								 does that for us!  thies@thieso.net
								 20000419 */
#endif

#ifdef ZTS
	php_tsrm_startup();
# ifdef PHP_WIN32
	ZEND_TSRMLS_CACHE_UPDATE();
# endif
#endif

	zend_signal_startup();

	sapi_startup(&php_embed_module);

#ifdef PHP_WIN32
	_fmode = _O_BINARY;			/*sets default for file streams to binary */
	setmode(_fileno(stdin), O_BINARY);		/* make the stdio mode be binary */
	setmode(_fileno(stdout), O_BINARY);		/* make the stdio mode be binary */
	setmode(_fileno(stderr), O_BINARY);		/* make the stdio mode be binary */
#endif

	php_embed_module.ini_entries = (char*)malloc(sizeof(HARDCODED_INI));
	memcpy(php_embed_module.ini_entries, HARDCODED_INI, sizeof(HARDCODED_INI));

	php_embed_module.additional_functions = additional_functions;

	if (argv) {
		php_embed_module.executable_location = argv[0];
	}

	if (php_embed_module.startup(&php_embed_module) == FAILURE) {
		return FAILURE;
	}

	zend_llist_init(&global_vars, sizeof(char *), NULL, 0);

	/* Set some Embedded PHP defaults */
	SG(options) |= SAPI_OPTION_NO_CHDIR;
	SG(request_info).argc = argc;
	SG(request_info).argv = argv;

	if (php_request_startup() == FAILURE) {
		php_module_shutdown();
		return FAILURE;
	}

	SG(headers_sent) = 1;
	SG(request_info).no_headers = 1;
	php_register_variable("PHP_SELF", "-", NULL);

	return SUCCESS;
}


EMBED_SAPI_API void sapi_php_shutdown(void)
{
	php_module_shutdown();
	sapi_shutdown();
#ifdef ZTS
	tsrm_shutdown();
	TSRMLS_CACHE_RESET();
#endif
	if (php_embed_module.ini_entries) {
		free(php_embed_module.ini_entries);
		php_embed_module.ini_entries = NULL;
	}
}
