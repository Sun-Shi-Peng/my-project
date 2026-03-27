#include "../logs/polylog.hpp"
#include <unistd.h>

void test_log(const std::string &name)
{
    INFO("%s", "测试开始");

    polylog::Logger::ptr logger = polylog::LoggerManager::getInstance().getLogger(name);
    logger->debug("%s", "测试日志");
    logger->info("%s", "测试日志");
    logger->warn("%s", "测试日志");
    logger->error("%s", "测试日志");
    logger->fatal("%s", "测试日志");
    INFO("%s", "测试完毕");
}

int main()
{
    std::unique_ptr<polylog::LoggerBuilder> builder(new polylog::GlobalLoggerBuilder());
    builder->buildLoggerName("async_logger");
    builder->buildLoggerLevel(polylog::LogLevel::value::DEBUG);
    builder->buildFormatter("[%c][%f:%l][%p]%m%n");
    builder->buildLoggerType(polylog::LoggerType::LOGGER_SYNC);
    builder->buildSink<polylog::FileSink>("./logfile/sync.log");
    builder->buildSink<polylog::StdoutSink>();
    builder->buildSink<polylog::RollBySizeSink>("./logfile/roll-sync-by-size", 1024 * 1024);
    builder->build();
    test_log("async_logger");
    return 0;
}