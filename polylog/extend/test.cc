#include "../logs/polylog.hpp"
#include <unistd.h>
// 扩展一个以时间作为入职文件滚动切换类型的日志落地模块
// 1.以时间进行文件滚动，实际上是以时间段进行滚动
//   实现思想:以当前系统时间，取模，时间段大小，可以得到当前时间段是第几个时间段
//           每次以当前系统时间取模，判断与当前文件的时间段是否一致，不一致代表不是一个时间段

enum class TimeGap
{
    GAP_SECOND,
    GAP_MINUTE,
    GAP_HOUR,
    GAP_DAY
};

class RollByTimeSink : public polylog::LogSink
{
private:
    std::string _basename;
    std::ofstream _ofs;
    size_t _cur_gap;  // 当前是第几个时间段
    size_t _gap_size; // 时间段大小
private:
    std::string createNewFile()
    {
        // 获取系统时间，以时间来构造文件扩展名
        time_t t = polylog::util::Date::now();
        struct tm lt;
        localtime_r(&t, &lt);
        std::stringstream filename;
        filename << _basename;
        filename << lt.tm_year + 1900 << lt.tm_mon + 1 << lt.tm_mday << lt.tm_hour << lt.tm_min << lt.tm_sec << ".log";
        return filename.str();
    }

public:
    // 构造时传入文件名，并打开文件，讲操作句柄管理起来
    RollByTimeSink(const std::string &basename, TimeGap gap_type) : _basename(basename)
    {
        switch (gap_type)
        {
        case TimeGap::GAP_SECOND:
            _gap_size = 1;
            break;
        case TimeGap::GAP_MINUTE:
            _gap_size = 60;
            break;
        case TimeGap::GAP_HOUR:
            _gap_size = 3600;
            break;
        case TimeGap::GAP_DAY:
            _gap_size = 3600 * 24;
            break;
        }
        _cur_gap = _gap_size == 1 ? polylog::util::Date::now() : polylog::util::Date::now() % _gap_size; // 获取当前是第几个时间段
        std::string filename = createNewFile();
        polylog::util::File::createDirectory(polylog::util::File::path(filename));
        _ofs.open(filename, std::ios::binary | std::ios::app);
        assert(_ofs.is_open());
    }
    // 将日志消息写入到标准输出，判断当前时间是否是当前文件的时间段，不是则切换文件
    void log(const char *data, size_t len)
    {
        time_t cur = polylog::util::Date::now();
        if ((cur % _gap_size) != _cur_gap)
        {
            _ofs.close();
            std::string filename = createNewFile();
            _ofs.open(filename, std::ios::binary | std::ios::app);
            assert(_ofs.is_open());
        }
        _ofs.write(data, len);
        assert(_ofs.good());
    }
};

int main()
{
    std::unique_ptr<polylog::LoggerBuilder> builder(new polylog::GlobalLoggerBuilder());
    builder->buildLoggerName("async_logger");
    builder->buildLoggerLevel(polylog::LogLevel::value::WARN);
    builder->buildFormatter("[%c][%f:%l]%m%n");
    builder->buildLoggerType(polylog::LoggerType::LOGGER_ASYNC);
    builder->buildSink<polylog::FileSink>("./logfile/async.log");
    builder->buildSink<polylog::StdoutSink>();
    builder->buildSink<RollByTimeSink>("./logfile/roll-sync-by-size", TimeGap::GAP_SECOND);
    polylog::Logger::ptr logger = builder->build();
    size_t cur = polylog::util::Date::now();
    while (polylog::util::Date::now() < cur + 5)
    {
        logger->fatal("这是一条测试日志");
        usleep(1000);
    }
    return 0;
}