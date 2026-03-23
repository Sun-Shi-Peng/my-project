#include <iostream>
#include <string>
#include <regex>
#include <algorithm>
#include <cctype>

int main()
{
    // HTTP请求行格式：  GET /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1\r\n
    std::string str = "get /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1\r\n";
    std::smatch matches;
    std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
    bool ret = std::regex_match(str, matches, e);
    if (ret == false)
    {
        return false;
    }
    std::string method=matches[1];
    std::transform(method.begin(),method.end(),method.begin(),::toupper);
    std::cout <<method<<std::endl;
    for (int i = 0; i < matches.size(); i++)
    {
        std::cout << i << " : " << matches[i] << std::endl;
    }
    // 0 : get /bitejiuyeke/login?user=xiaoming&pass=123123 HTTP/1.1

    // 1 : get
    // 2 : /bitejiuyeke/login
    // 3 : user=xiaoming&pass=123123
    // 4 : HTTP/1.1
    return 0;
}