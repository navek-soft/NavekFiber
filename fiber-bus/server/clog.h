#pragma once
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <ctime>

#define DEBUG

namespace fiber {
	class clog {
	private:
		static inline void timestamp(FILE* fp) {
			static thread_local std::time_t timestamp{0};
			static thread_local std::tm tm;
			std::time_t now = std::time(nullptr);
			if (now == timestamp) {
				std::fprintf(fp, "%4d-%02d-%02d %02d:%02d:%02d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
			}
			timestamp = now;
			localtime_r(&timestamp, &tm);
			std::fprintf(fp, "%4d-%02d-%02d %02d:%02d:%02d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		}
	public:
		static inline void err(size_t lvl, const char* fmt, ...) {
#ifdef DEBUG
			FILE* fp = stdout;
#else
			FILE* fp = stderr;
			if (lvl >= capp::verbose()) {
#endif // !DEBUG
				timestamp(fp);
				std::va_list args;
				va_start(args, fmt);
				std::vfprintf(fp, fmt, args);
				va_end(args);
#ifndef DEBUG
			}
#endif // !DEBUG
		}
		static inline void log(size_t lvl, const char* fmt, ...) {
#ifndef DEBUG
			if (lvl >= capp::verbose()) {
#endif // !DEBUG
				timestamp(stdout);
				std::va_list args;
				va_start(args, fmt);
				std::vfprintf(stdout, fmt, args);
				va_end(args);
#ifndef DEBUG
			}
#endif // !DEBUG
		}
		static inline void msg(size_t lvl, const char* fmt, ...) {
			//if (lvl >= capp::verbose()) {
				std::va_list args;
				va_start(args, fmt);
				std::vfprintf(stdout, fmt, args);
				va_end(args);
			//}
		}
	};
}