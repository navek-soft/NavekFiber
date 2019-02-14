#pragma once
#include <exception>
#include <string>
#include <cstring>
#include <cerrno>
#include <cstdarg>

class RuntimeException : std::exception {
	std::string exMessage;
public:
	RuntimeException(std::string&& SrcFile, size_t SrcLine,std::string&& Format,...) noexcept {
		va_list args;
		va_start(args, Format);
		size_t size = std::vsnprintf(nullptr, 0, Format.c_str(), args);
		va_end(args);
		std::string buffer;
		buffer.resize(size + 1);
		va_start(args, Format);
		std::vsnprintf((char*)buffer.data(), size, Format.c_str(), args);
		va_end(args);
		exMessage.append(buffer.c_str()).append(" | ").append(SrcFile.c_str() + SrcFile.rfind('/') + 1).append(":").append(std::to_string(SrcLine));
	}
	inline virtual const char*	what() const noexcept {
		return exMessage.c_str();
	}
};

class SystemException : std::exception {
	std::string exMessage;
public:
	SystemException(error_t ErrCode, std::string&& Scope, std::string&& SrcFile, size_t SrcLine) noexcept { 
		size_t size = std::snprintf(nullptr, 0, "[EXCEPTION] %s: %s (%ld) | %s:%ld", Scope.c_str(),std::strerror(ErrCode),ErrCode, SrcFile.c_str() + SrcFile.rfind('/') + 1,SrcLine);
		exMessage.resize(size + 1);
		exMessage[size] = '\0';
		std::snprintf((char*)exMessage.data(), size, "[EXCEPTION] %s: %s (%ld) | %s:%ld", Scope.c_str(), std::strerror(ErrCode), ErrCode, SrcFile.c_str() + SrcFile.rfind('/') + 1, SrcLine);
	}
	inline virtual const char*	what() const noexcept {
		return exMessage.c_str();
	}
};