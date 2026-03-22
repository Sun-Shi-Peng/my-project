#include "../server.hpp"
#include <fstream>
#include <regex>
#include <sys/stat.h>
class Util
{
public:
    // 字符串分割函数，将src字符串按照sep进行分割，得到的各个子串放到arry中，最终返回子串的数量
    static size_t Split(const std::string &src, const std::string &sep, std::vector<std::string> *arry)
    {
        size_t offset = 0;
        // 有10个字符，offset是查找的起始位置，范围应该是0-9，offset==10就代表已经越界了
        while (offset < src.size())
        {
            size_t pos = src.find(sep, offset); // 在src字符串偏移量offset处，开始向后查找sep字符/子串，返回查找到的位置
            if (pos == std::string::npos)       // 没有找到特定的字符
            {
                if (pos == src.size())
                {
                    break;
                }
                // 将剩余的部分当作一个子串中，放入arry中
                arry->push_back(src.substr(offset));
                return arry->size();
            }
            if (pos == offset)
            {
                offset = pos + sep.size();
                continue; // 当前子串是一个空空的，没有内容
            }
            arry->push_back(src.substr(offset, pos - offset));
            offset = pos + sep.size();
        }
        return arry->size();
    }
    // 读取文件内容，将读取的内容放到一个Buffer中
    static bool ReadFile(const std::string &filename, std::string *buf)
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open() == false)
        {
            ERR_LOG("OPEN %s FTLE FAILED!!", filename.c_str());
            return false;
        }
        size_t fsize = 0;
        ifs.seekg(0, ifs.end); // 跳转读写位置到末尾
        fsize = ifs.tellg();   // 获取当前读写位置相当于起始位置的偏移量，从末尾偏移刚好是文件大小
        ifs.seekg(0, ifs.beg); // 跳转到起始位置
        buf->resize(fsize);    // 开辟文件大小的空间
        ifs.read(&(*buf)[0], fsize);
        if (ifs.good() == false)
        {
            ERR_LOG("READ %s FTLE FAILED!!", filename.c_str());
            ifs.close();
            return false;
        }
        ifs.close();
        return true;
    }
    // 向文件写入数据
    static bool WriteFile(const std::string &filename, const std::string &buf)
    {
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc); // 丢弃原有数据
        if (ofs.is_open() == false)
        {
            ERR_LOG("OPEN %s FTLE FAILED!!", filename.c_str());
            return false;
        }
        ofs.write(buf.c_str(), buf.size());
        if (ofs.good() == false)
        {
            ERR_LOG("WRITE %s FTLE FAILED!", filename.c_str());
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }
    // URL编码，避免URL资源路径与查询字符串中的特殊字符与HTTP请求中特殊字符产生歧义
    // 编码格式：将特殊字符的ascii值，转换为两个16进制字符
    // 不编码的特殊字符：RFC3986文档归定 . - _ ~ 以及字母和数字 属于绝对不编码字符
    // RFC3986文档归定，编码格式 &HH
    // W3C文档中规定，查询字符串中的空格，需要被转换为 + ，解码则是+转空格
    static std::string UrlEncode(const std::string url, bool convert_space_to_plus)
    {
        std::string res;
        for (auto &c : url)
        {
            if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c)) // isalnum检查字符是不是 0-9 或 A-Za-z。
            {
                res += c;
                continue;
            }
            if (c == ' ' && convert_space_to_plus)
            {
                res += '+';
                continue;
            }
            // 剩下的字符都需要编码称为 %HH 格式
            char tmp[4] = {0};
            snprintf(tmp, 4, "%%%02X", c);
            res += tmp;
        }
        return res;
    }
    static char HEXTOI(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        else if (c >= 'a' && c <= 'z')
        {
            // 读取到字符b，计算机认为ascii编码是98，为了得到数值11，需要做加法，'b'-'a'+10=11
            return c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'Z')
        {
            return c - 'A' + 10;
        }
        return -1;
    }
    // URL解码
    static std::string UrlDecode(const std::string url, bool convert_space_to_plus)
    {
        // 遇到了%,则将紧随其后的2个字符，转换为数字，第一个数字左移4位（*16），然后加上第二个数字
        std::string res;
        for (int i = 0; i < url.size(); i++)
        {
            if (url[i] == '+' && convert_space_to_plus)
            {
                res += ' ';
                continue;
            }
            if (url[i] == '%')
            {
                char v1 = HEXTOI(url[i + 1]);
                char v2 = HEXTOI(url[i + 2]);
                char v = (v1 << 4) + v2;
                res += v;
                i += 2;
                continue;
            }
            res += url[i];
        }
        return res;
    }
    // 响应状态码的描述信息获取
    static std::string StatuDesc(int statu)
    {
        std::unordered_map<int, std::string> _statu_msg = {
            {100, "Continue"},
            {101, "Switching Protocol"},
            {102, "Processing"},
            {103, "Early Hints"},
            {200, "OK"},
            {201, "Created"},
            {202, "Accepted"},
            {203, "Non-Authoritative Information"},
            {204, "No Content"},
            {205, "Reset Content"},
            {206, "Partial Content"},
            {207, "Multi-Status"},
            {208, "Already Reported"},
            {226, "IM Used"},
            {300, "Multiple Choice"},
            {301, "Moved Permanently"},
            {302, "Found"},
            {303, "See Other"},
            {304, "Not Modified"},
            {305, "Use Proxy"},
            {306, "unused"},
            {307, "Temporary Redirect"},
            {308, "Permanent Redirect"},
            {400, "Bad Request"},
            {401, "Unauthorized"},
            {402, "Payment Required"},
            {403, "Forbidden"},
            {404, "Not Found"},
            {405, "Method Not Allowed"},
            {406, "Not Acceptable"},
            {407, "Proxy Authentication Required"},
            {408, "Request Timeout"},
            {409, "Conflict"},
            {410, "Gone"},
            {411, "Length Required"},
            {412, "Precondition Failed"},
            {413, "Payload Too Large"},
            {414, "URI Too Long"},
            {415, "Unsupported Media Type"},
            {416, "Range Not Satisfiable"},
            {417, "Expectation Failed"},
            {418, "I'm a teapot"},
            {421, "Misdirected Request"},
            {422, "Unprocessable Entity"},
            {423, "Locked"},
            {424, "Failed Dependency"},
            {425, "Too Early"},
            {426, "Upgrade Required"},
            {428, "Precondition Required"},
            {429, "Too Many Requests"},
            {431, "Request Header Fields Too Large"},
            {451, "Unavailable For Legal Reasons"},
            {501, "Not Implemented"},
            {502, "Bad Gateway"},
            {503, "Service Unavailable"},
            {504, "Gateway Timeout"},
            {505, "HTTP Version Not Supported"},
            {506, "Variant Also Negotiates"},
            {507, "Insufficient Storage"},
            {508, "Loop Detected"},
            {510, "Not Extended"},
            {511, "Network Authentication Required"}};
        auto it = _statu_msg.find(statu);
        if (it != _statu_msg.end())
        {
            return it->second;
        }
        return "Unknow";
    }
    // 根据文件后缀名获取文件mime
    static std::string ExtMime(const std::string &filename)
    {
        std::unordered_map<std::string, std::string> _mime_msg = {
            {".aac", "audio/aac"},
            {".abw", "application/x-abiword"},
            {".arc", "application/x-freearc"},
            {".avi", "video/x-msvideo"},
            {".azw", "application/vnd.amazon.ebook"},
            {".bin", "application/octet-stream"},
            {".bmp", "image/bmp"},
            {".bz", "application/x-bzip"},
            {".bz2", "application/x-bzip2"},
            {".csh", "application/x-csh"},
            {".css", "text/css"},
            {".csv", "text/csv"},
            {".doc", "application/msword"},
            {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
            {".eot", "application/vnd.ms-fontobject"},
            {".epub", "application/epub+zip"},
            {".gif", "image/gif"},
            {".htm", "text/html"},
            {".html", "text/html"},
            {".ico", "image/vnd.microsoft.icon"},
            {".ics", "text/calendar"},
            {".jar", "application/java-archive"},
            {".jpeg", "image/jpeg"},
            {".jpg", "image/jpeg"},
            {".js", "text/javascript"},
            {".json", "application/json"},
            {".jsonld", "application/ld+json"},
            {".mid", "audio/midi"},
            {".midi", "audio/x-midi"},
            {".mjs", "text/javascript"},
            {".mp3", "audio/mpeg"},
            {".mpeg", "video/mpeg"},
            {".mpkg", "application/vnd.apple.installer+xml"},
            {".odp", "application/vnd.oasis.opendocument.presentation"},
            {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
            {".odt", "application/vnd.oasis.opendocument.text"},
            {".oga", "audio/ogg"},
            {".ogv", "video/ogg"},
            {".ogx", "application/ogg"},
            {".otf", "font/otf"},
            {".png", "image/png"},
            {".pdf", "application/pdf"},
            {".ppt", "application/vnd.ms-powerpoint"},
            {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
            {".rar", "application/x-rar-compressed"},
            {".rtf", "application/rtf"},
            {".sh", "application/x-sh"},
            {".svg", "image/svg+xml"},
            {".swf", "application/x-shockwave-flash"},
            {".tar", "application/x-tar"},
            {".tif", "image/tiff"},
            {".tiff", "image/tiff"},
            {".ttf", "font/ttf"},
            {".txt", "text/plain"},
            {".vsd", "application/vnd.visio"},
            {".wav", "audio/wav"},
            {".weba", "audio/webm"},
            {".webm", "video/webm"},
            {".webp", "image/webp"},
            {".woff", "font/woff"},
            {".woff2", "font/woff2"},
            {".xhtml", "application/xhtml+xml"},
            {".xls", "application/vnd.ms-excel"},
            {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
            {".xml", "application/xml"},
            {".xul", "application/vnd.mozilla.xul+xml"},
            {".zip", "application/zip"},
            {".3gp", "video/3gpp"},
            {".3g2", "video/3gpp2"},
            {".7z", "application/x-7z-compressed"}};
        // a.b.txt 先获取文件扩展名
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos)
        {
            return "application/octet-stream";
        }
        // 根据扩展名，获取mime
        std::string ext = filename.substr(pos);
        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end())
        {
            return "application/octet-stream";
        }
        return it->second;
    }
    // 判断一个文件是否是一个目录
    static bool IsDirectroy(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            return false;
        }
        return S_ISDIR(st.st_mode);
    }
    // 判断一个文件是否是一个普通文件
    static bool ValidPath(const std::string &path)
    {
        // 思想：按照/进行路径分割，根据有多少子目录，计算目录深度有多少层
        std::vector<std::string> subdir;
        Split(path, "/", &subdir);
        int level = 0;
        for (auto &dir : subdir)
        {
            if (dir == "..")
            {
                level--;
                if (level < 0)
                {
                    return false;
                }
                continue;
            }
            level++;
        }
        return true;
    }
};

class Request
{
public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 资源路径
    std::string _version;                                  // 协议版本
    std::string _body;                                     // 请求正文
    std::smatch _matches;                                  // 资源路径的正则提取数据
    std::unordered_map<std::string, std::string> _headers; // 头部字段
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
public:
    void ReSet()
    {
        _method.clear();
        _path.clear();
        _version.clear();
        _body.clear();
        std::smatch match;
        _matches.swap(match);
        _headers.clear();
        _params.clear();
    }
    // 插入头部字段
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert(std::make_pair(key, val));
    }
    // 判断是否存在指定头部字段
    bool HasHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return false;
        }
        return true;
    }
    // 获取指定头部字段的值
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    // 插入查询字符串
    void SetParam(const std::string &key, std::string &val)
    {
        _params.insert(std::make_pair(key, val));
    }
    // 判断是否有某个指定的查询字符串
    bool HasParam(const std::string &key)
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return false;
        }
        return true;
    }
    // 获取指定的查询字符串
    std::string GetParam(std::string &key)
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return "";
        }
        return it->second;
    }
    // 获取正文长度
    size_t ContentLength()
    {
        // Content-Length:1234\r\n
        bool ret = HasHeader("Content-Length");
        if (ret == false)
        {
            return 0;
        }
        std::string clen = GetHeader("Content-Length");
        return std::stoi(clen);
    }
    // 判断是否是短链接
    bool Close()
    {
        // 没有Connection字段，或者有Connection但是值是close，则都是短链接，否则就是长连接
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return true;
        }
        return false;
    }
};

class HttpResponse
{
private:
    int _statu;
    bool _redirect_flag; // 重定向标志
    std::string _body;
    std::string _redirect_url; // 重定向地址
    std::unordered_map<std::string, std::string> _headers;

public:
    HttpResponse() : _redirect_flag(false), _statu(200)
    {
    }
    HttpResponse(int statu) : _redirect_flag(false), _statu(statu)
    {
    }
    void ReSet()
    {
        _statu = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert(std::make_pair(key, val));
    }
    bool HasHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return false;
        }
        return true;
    }
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    void SetContent(const std::string &body, const std::string &type = "text/html")
    {
        _body = body;
        SetHeader("Content-Type", type);
    }
    void SetRedirect(const std::string &url, int statu = 302) // 302临时重定向
    {
        _statu = statu;
        _redirect_flag = true;
        _redirect_url = url;
    }
    // 判断是否是短连接
    bool Close()
    {
        // 没有Connection字段，或者有Connection但是值是close，则都是短链接，否则就是长连接
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return true;
        }
        return false;
    }
};