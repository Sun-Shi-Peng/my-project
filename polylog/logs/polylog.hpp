#ifndef __M_POLYLOG_H__
#define __M_POLYLOG_H__
#include "logger.hpp"

namespace polylog {
    //1.提供获取指定日志器的全局接口（避免用户自己操作单例对象）
    Logger::ptr getLogger(const std::string &name)
    {
        return polylog::LoggerManager::getInstance().getLogger(name);
    }

    Logger::ptr rootLogger()
    {
        return polylog::LoggerManager::getInstance().rootLogger();
    }
    //2.使用宏函数对日志器的接口进行代理（代理模式）
    #define debug(fmt,...) debug(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define info(fmt,...) info(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define warn(fmt,...) warn(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define error(fmt,...) error(__FILE__,__LINE__,fmt,##__VA_ARGS__)
    #define fatal(fmt,...) fatal(__FILE__,__LINE__,fmt,##__VA_ARGS__)

    
    //3.提供宏函数，直接通过默认日志器进行日志的标准输出打印（不用获取日志器了）
    #define DEBUG(fmt,...) polylog::rootLogger()->debug(fmt,##__VA_ARGS__)
    #define INFO(fmt,...) polylog::rootLogger()->info(fmt,##__VA_ARGS__)
    #define WARN(fmt,...) polylog::rootLogger()->warn(fmt,##__VA_ARGS__)
    #define ERROR(fmt,...) polylog::rootLogger()->error(fmt,##__VA_ARGS__)
    #define FATAL(fmt,...) polylog::rootLogger()->fatal(fmt,##__VA_ARGS__)
}

#endif