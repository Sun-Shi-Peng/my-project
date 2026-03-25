//完成日志器模块：
// 1.抽象日志器基类
// 2.派生出不同的子类（同步日志器类 & 异步日志器类）
#include "util.hpp"
#include "level.hpp"
#include "message.hpp"
#include "format.hpp"
#include "sink.hpp"
#include <atomic>

namespace polylog
{
    class Loger
    {
    private:    
        std::string _logger_name;
        std::atomic<LogLevel::value> _imit_level;
    public:
    

    };
}