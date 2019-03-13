#pragma once

#ifndef NDEBUG
#define DEBUG
#include <string>
#define trace(fmt, args...) fprintf(stdout, "[ %s ] " fmt " # %s:%-3d\n",__FUNCTION__, ##args,((const char*)__FILE__) + std::string(__FILE__).rfind('/')+1, __LINE__)
#define dbg_trace(fmt, args...) fprintf(stdout, "# DBG %s " fmt " # %s:%-3d\n",__FUNCTION__, ##args,((const char*)__FILE__) + std::string(__FILE__).rfind('/')+1, __LINE__)
#define log_option(option,fmt,args...) fprintf(stdout, "%28s : " fmt "\n",option, ##args)
#define log_print(fmt,args...) fprintf(stdout, fmt " # %s:%-3d\n", ##args,((const char*)__FILE__) + std::string(__FILE__).rfind('/')+1, __LINE__)
#else
#define dbg_trace(fmt, args...)
#define trace(fmt, args...) fprintf(stdout, fmt "\n", ##args)
#define log_option(option,fmt,args...) fprintf(stdout, "%28s : " fmt "\n",option, ##args)
#define log_print(fmt,args...) fprintf(stdout, fmt "\n", ##args)
#endif // DEBUG
