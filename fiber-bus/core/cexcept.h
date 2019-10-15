#pragma once
#include <stdexcept>
#include <string>
#include <cstring>

namespace core {

	class throwable_error : std::exception {
	protected:
		std::string srcWhere;
		ssize_t		srcLine;
	protected:
		throwable_error(const char* src_where, ssize_t src_line) noexcept : srcWhere(src_where), srcLine(src_line) { ; }
		virtual ~throwable_error() noexcept { ; }
	public:
		const std::string& where() const noexcept { return srcWhere; }
		const ssize_t& line() const noexcept { return srcLine; }
	};

	class system_error : throwable_error {
		int			errNo;
		std::string errWhat;
	public:
		explicit system_error(int err_no, const char* src_where = __PRETTY_FUNCTION__, ssize_t src_line = __LINE__) noexcept : throwable_error(src_where, src_line), errNo(err_no) {
			errWhat.append(std::strerror(err_no)).append(" (").append(std::to_string(err_no)).append(") #").append(src_where).append(" (").append(std::to_string(src_line)).append(")");
		}

		explicit system_error(int err_no,const std::string& src_msg, const char* src_where = __PRETTY_FUNCTION__, ssize_t src_line = __LINE__) noexcept : throwable_error(src_where, src_line), errNo(err_no) {
			errWhat.append(src_msg).append(": ").append(std::strerror(err_no)).append(" (").append(std::to_string(err_no)).append(") #").append(src_where).append(" (").append(std::to_string(src_line)).append(")");
		}
		
		virtual ~system_error() noexcept { ; }

		virtual const char* what() const noexcept {
			return errWhat.c_str();
		}
		system_error(const system_error& e) noexcept : throwable_error(e.srcWhere.c_str(),e.srcLine), errNo(e.errNo) { ; }
		system_error& operator=(const system_error& e) noexcept { srcWhere = e.srcWhere; srcLine = e.srcLine; errNo = e.errNo; errWhat = e.errWhat; return *this; }
	};
}
