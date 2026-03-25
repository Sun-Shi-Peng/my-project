#ifndef __M_MESSAGE_H__
#define __M_MESSAGE_H__
// 定义日志消息类
//   1.日志输出时间
//   2.日志等级
//   3.源文件名称
//   4.源代码行号
//   5.线程ID
//   6.日志主体消息
//   7.日志器名称
#include "util.hpp"
#include "level.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <ctime>

namespace polylog
{
    struct LogMsg
    {
        time_t _ctime;          // 日志产生的时间戳
        LogLevel::value _level; // 日志等级
        size_t _line;           // 行号
        std::thread::id _tid;   // 线程id
        std::string _file;      // 源码文件名
        std::string _logger;    // 日志器名称
        std::string _payload;   // 日志主体消息

        LogMsg(LogLevel::value level,
               size_t line,
               const std::string &file,
               const std::string &logger,
               const std::string &msg) : _ctime(util::Date::now()), _level(level), _line(line), _tid(std::this_thread::get_id()),
                                         _file(file), _logger(logger), _payload(msg)
        {
        }
    };
}
#endif