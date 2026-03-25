#ifndef __M_UTIL_H__
#define __M_UTIL_H__
// 实用工具类的实现
//   1.获取系统时间
//   2.判断文件是否存在
//   3.获取文件所在路径
//   4.创建目录
#include <iostream>
#include <string>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>

namespace polylog
{
    namespace util
    {
        class Date
        {
        public:
            static size_t now()
            {
                return (size_t)time(nullptr);
            }
        };
        class File
        {
        public:
            static bool exists(const std::string &pathname)
            {
                struct stat st;
                if (stat(pathname.c_str(), &st) < 0) // 通过获取文件属性判断
                {
                    return false;
                }
                return true;
            }
            static std::string path(const std::string &pathname)
            {
                // ./abc/a.txt
                size_t pos = pathname.find_last_of("/\\");
                if (pos == std::string::npos)
                {
                    return ".";
                }
                return pathname.substr(0, pos + 1);
            }
            static void createDirectory(const std::string &pathname) // 创建目录
            {
                // ./abc/a.txt
                size_t pos = 0, idx = 0; // idx标记查找的起始位置
                while (idx < pathname.size())
                {
                    pos = pathname.find_first_of("/\\", idx);
                    if (pos == std::string::npos)
                    {
                        mkdir(pathname.c_str(), 0755);
                        return;
                    }
                    std::string parent_dir = pathname.substr(0, pos + 1); // /和\也截取出来
                    if (exists(parent_dir.c_str()) == true)
                    {
                        idx = pos + 1;
                        continue;
                    }
                    mkdir(parent_dir.c_str(), 0755);
                    idx = pos + 1;
                }
            }
        };
    }
}
#endif