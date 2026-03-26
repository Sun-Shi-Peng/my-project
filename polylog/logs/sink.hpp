#ifndef __M_SINK_H__
#define __M_SINK_H__

// 日志落地模块的实现
// 1.抽象落地基类
// 2.派生子类（根据不同落地方向进行派生）
// 3.是用工厂模式进行创建与表示的分离
#include "util.hpp"
#include <memory>
#include <fstream>
#include <unistd.h>
#include <cassert>
#include <sstream>

namespace polylog
{
    class LogSink
    {
    public:
        using ptr = std::shared_ptr<LogSink>;
        virtual ~LogSink() {}
        virtual void log(const char *data, size_t len) = 0;
    };

    // 落地方向：标准输出
    class StdoutSink : public LogSink
    {
    public:
        // 将消息写入到标准输出
        void log(const char *data, size_t len)
        {
            std::cout.write(data, len);
        }
    };

    // 落地方向：指定文件
    class FileSink : public LogSink
    {
    private:
        std::string _pathname;
        std::ofstream _ofs;

    public:
        // 构造时传入文件名，并打开文件，讲操作句柄管理起来
        FileSink(const std::string &pathname) : _pathname(pathname)
        {
            // 1.创建日志文件所在目录
            util::File::createDirectory(util::File::path(_pathname));
            // 2.创建并打开日志文件
            _ofs.open(_pathname, std::ios::binary | std::ios::app);
            assert(_ofs.is_open());
        }
        // 将日志消息写入到指定文件
        void log(const char *data, size_t len)
        {
            _ofs.write(data, len);
            assert(_ofs.good());
        }
    };
    // 落地文件：滚动文件（以大小进行滚动）
    class RollBySizeSink : public LogSink
    {
    private:
        // 基础文件名+扩展文件名（以时间命名） 组成一个实际的当前输出文件名
        std::string _basename; // ./logs/base- -》 ./logs/base-20260325171033.log
        std::ofstream _ofs;
        size_t _max_fsize; // 记录最大大小，当前文件超过了这个大小就要切换文件
        size_t _cur_fsize; // 记录当前文件大小
        size_t _name_count;

    private:
        // 进行大小判断，超过指定大小则创建新文件
        std::string createNewFile()
        {
            // 获取系统时间，以时间来构造文件扩展名
            time_t t = util::Date::now();
            struct tm lt;
            localtime_r(&t, &lt);
            std::stringstream filename;
            filename << _basename;
            filename << lt.tm_year + 1900 << lt.tm_mon + 1 << lt.tm_mday << lt.tm_hour << lt.tm_min << lt.tm_sec << "-" << _name_count++ << ".log";
            return filename.str();
        }

    public:
        // 构造时传入文件名，并打开文件，将操作句柄管理起来
        RollBySizeSink(const std::string &basename, size_t max_size)
            : _basename(basename), _max_fsize(max_size), _cur_fsize(0), _name_count(0)
        {
            std::string pathname = createNewFile();
            // 1.创建日志文件所在目录
            util::File::createDirectory(util::File::path(pathname));
            // 2.创建并打开日志文件
            _ofs.open(pathname, std::ios::binary | std::ios::app);
            assert(_ofs.is_open());
        }
        // 将日志消息写入到指定输出 --- 写入前判断文件大小，超过了最大大小就要切换
        void log(const char *data, size_t len)
        {
            if (_cur_fsize >= _max_fsize) // 打开新的文件
            {
                std::string pathname = createNewFile();
                _ofs.close(); // 不要关闭原来打开的文件
                _ofs.open(pathname, std::ios::binary | std::ios::app);
                assert(_ofs.is_open());
                _cur_fsize = 0;
            }
            _ofs.write(data, len);
            assert(_ofs.good());
            _cur_fsize += len;
        }
    };

    class SinkFactory
    {
    public:
        template <typename SinkType, typename... Args>
        static LogSink::ptr create(Args &&...args)
        {
            return std::make_shared<SinkType>(std::forward<Args>(args)...);
        }
    };
}

#endif