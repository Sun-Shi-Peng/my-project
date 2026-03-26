#include "util.hpp"
#include "level.hpp"
#include "message.hpp"
#include "format.hpp"
#include "sink.hpp"
#include "logger.hpp"
#include "buffer.hpp"
#include <unistd.h>
// // 扩展一个以时间作为入职文件滚动切换类型的日志落地模块
// // 1.以时间进行文件滚动，实际上是以时间段进行滚动
// //   实现思想:以当前系统时间，取模，时间段大小，可以得到当前时间段是第几个时间段
// //           每次以当前系统时间取模，判断与当前文件的时间段是否一致，不一致代表不是一个时间段

// enum class TimeGap
// {
//     GAP_SECOND,
//     GAP_MINUTE,
//     GAP_HOUR,
//     GAP_DAY
// };

// class RollByTimeSink : public polylog::LogSink
// {
// private:
//     std::string _basename;
//     std::ofstream _ofs;
//     size_t _cur_gap;  // 当前是第几个时间段
//     size_t _gap_size; // 时间段大小
// private:
//     std::string createNewFile()
//     {
//         // 获取系统时间，以时间来构造文件扩展名
//         time_t t = polylog::util::Date::now();
//         struct tm lt;
//         localtime_r(&t, &lt);
//         std::stringstream filename;
//         filename << _basename;
//         filename << lt.tm_year + 1900 << lt.tm_mon + 1 << lt.tm_mday << lt.tm_hour << lt.tm_min << lt.tm_sec << ".log";
//         return filename.str();
//     }

// public:
//     // 构造时传入文件名，并打开文件，讲操作句柄管理起来
//     RollByTimeSink(const std::string &basename, TimeGap gap_type) : _basename(basename)
//     {
//         switch (gap_type)
//         {
//         case TimeGap::GAP_SECOND:
//             _gap_size = 1;
//             break;
//         case TimeGap::GAP_MINUTE:
//             _gap_size = 60;
//             break;
//         case TimeGap::GAP_HOUR:
//             _gap_size = 3600;
//             break;
//         case TimeGap::GAP_DAY:
//             _gap_size = 3600 * 24;
//             break;
//         }
//         _cur_gap = _gap_size == 1 ? polylog::util::Date::now() : polylog::util::Date::now() % _gap_size; // 获取当前是第几个时间段
//         std::string filename = createNewFile();
//         polylog::util::File::createDirectory(polylog::util::File::path(filename));
//         _ofs.open(filename, std::ios::binary | std::ios::app);
//         assert(_ofs.is_open());
//     }
//     // 将日志消息写入到标准输出，判断当前时间是否是当前文件的时间段，不是则切换文件
//     void log(const char *data, size_t len)
//     {
//         time_t cur = polylog::util::Date::now();
//         if ((cur % _gap_size) != _cur_gap)
//         {
//             _ofs.close();
//             std::string filename = createNewFile();
//             _ofs.open(filename, std::ios::binary | std::ios::app);
//             assert(_ofs.is_open());
//         }
//         _ofs.write(data, len);
//         assert(_ofs.good());
//     }
// };

int main()
{
    // 读取文件数据，一点一点写入缓冲区，最终将缓冲区数据写入文件，判断生成的新文件与源文件是否一致
    std::ifstream ifs("test.cc", std::ios::binary);
    if (ifs.is_open() == false)
    {
        return -1;
    }
    ifs.seekg(0, std::ios::end); // 读写位置跳转到文件末尾
    size_t fsize = ifs.tellg();  // 获取当前读写位置相对于起始位置的偏移量
    ifs.seekg(0, std::ios::beg); // 重新跳转到读取位置
    std::string body;
    body.resize(fsize);
    ifs.read(&body[0], fsize);
    if (ifs.good() == false)
    {
        std::cout << "read error\n";
        return -1;
    }
    ifs.close();

    polylog::Buffer buffer;
    for (int i = 0; i < body.size(); i++)
    {
        buffer.push(&body[i], 1);
    }

    std::ofstream ofs("tmp.cc", std::ios::binary);
    // ofs.write(buffer.begin(),buffer.readAbleSize());
    size_t rsize=buffer.readAbleSize();
    for (int i = 0; i < rsize; i++)
    {
        ofs.write(buffer.begin(), 1);
        buffer.moveReader(1);
    }
    ofs.close();

    // std::unique_ptr<polylog::LoggerBuilder> builder(new polylog::LocalLoggerBuilder());
    // builder->buildLoggerName("sync_logger");
    // builder->buildLoggerLevel(polylog::LogLevel::value::WARN);
    // builder->buildFormatter("%m%n");
    // builder->buildLoggerType(polylog::LoggerType::LOGGER_SYNC);
    // builder->buildSink<polylog::FileSink>("./logfile/test.log");
    // builder->buildSink<polylog::StdoutSink>();
    // polylog::Logger::ptr logger=builder->build();

    // logger->debug(__FILE__, __LINE__, "%s", "测试日志");
    // logger->info(__FILE__, __LINE__, "%s", "测试日志");
    // logger->warn(__FILE__, __LINE__, "%s", "测试日志");
    // logger->error(__FILE__, __LINE__, "%s", "测试日志");
    // logger->fatal(__FILE__, __LINE__, "%s", "测试日志");
    // size_t cursize = 0, count = 0;
    // std::string str = "测试日志 - ";
    // while (cursize < 1024 * 1024 * 5)
    // {
    //     logger->fatal(__FILE__, __LINE__, "测试日志-%d",count++);
    //     cursize += 50;
    // }

    // std::string logger_name = "sync_logger";
    // polylog::LogLevel::value limit = polylog::LogLevel::value::WARN;
    // polylog::Formatter::ptr fmt(new polylog::Formatter("[%d{%H:%M:%S}][%c][%f:%l][%p]%T%m%n"));
    // polylog::LogSink::ptr stdout_lsp = polylog::SinkFactory::create<polylog::StdoutSink>();
    // polylog::LogSink::ptr file_lsp = polylog::SinkFactory::create<polylog::FileSink>("./logfile/test.log");
    // polylog::LogSink::ptr roll_lsp = polylog::SinkFactory::create<polylog::RollBySizeSink>("./logfile/roll-", 1024 * 1024);
    // std::vector<polylog::LogSink::ptr> sinks = {stdout_lsp, file_lsp, roll_lsp};
    // polylog::Logger::ptr logger(new polylog::SyncLogger(logger_name, limit, fmt, sinks));

    // polylog::LogMsg msg(polylog::LogLevel::value::INFO, 8, "main.c", "root", "功能测试");
    // polylog::Formatter fmt("abc%%abc[%d{%H:%M:%S}] %m%n");
    // std::string str = fmt.format(msg);
    // // polylog::LogSink::ptr stdout_lsp = polylog::SinkFactory::create<polylog::StdoutSink>();
    // // polylog::LogSink::ptr file_lsp = polylog::SinkFactory::create<polylog::FileSink>("./logfile/test.log");
    // // polylog::LogSink::ptr roll_lsp = polylog::SinkFactory::create<polylog::RollBySizeSink>("./logfile/roll-", 1024 * 1024);
    // polylog::LogSink::ptr time_lsp = polylog::SinkFactory::create<RollByTimeSink>("./logfile/roll-", TimeGap::GAP_MINUTE);
    // time_t old = polylog::util::Date::now();
    // while (polylog::util::Date::now() < old+5)
    // {
    //     time_lsp->log(str.c_str(),str.size());
    //     usleep(1000);
    // }
    // stdout_lsp->log(str.c_str(), str.size());
    // file_lsp->log(str.c_str(), str.size());
    // size_t cursize = 0;
    // size_t count = 0;
    // while (cursize < 1024 * 1024 * 10)
    // {
    //     std::string tmp = str + std::to_string(count++);
    //     roll_lsp->log(tmp.c_str(), tmp.size());
    //     cursize += tmp.size();
    // }

    // std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::DEBUG) << std::endl;
    // std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::INFO) << std::endl;
    // std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::WARN) << std::endl;
    // std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::ERROR) << std::endl;
    // std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::FATAL) << std::endl;
    // std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::OFF) << std::endl;
    // std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::UNKNOW) << std::endl;
    // std::cout << polylog::util::Date::now() << std::endl;
    // std::string pathname="./abc/bcd/a.txt";
    // polylog::util::File::createDirectory(polylog::util::File::path(pathname));
    return 0;
}