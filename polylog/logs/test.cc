#include "util.hpp"
#include "level.hpp"
#include "message.hpp"

int main()
{
    std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::DEBUG) << std::endl;
    std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::INFO) << std::endl;
    std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::WARN) << std::endl;
    std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::ERROR) << std::endl;
    std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::FATAL) << std::endl;
    std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::OFF) << std::endl;
    std::cout << polylog::LogLevel::toString(polylog::LogLevel::value::UNKNOW) << std::endl;
    // std::cout << polylog::util::Date::now() << std::endl;
    // std::string pathname="./abc/bcd/a.txt";
    // polylog::util::File::createDirectory(polylog::util::File::path(pathname));
    return 0;
}