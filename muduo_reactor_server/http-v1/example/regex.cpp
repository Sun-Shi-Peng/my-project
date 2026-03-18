#include <iostream>
#include <string>
#include <regex>

int main()
{
    std::string str="/numbers/1234";
    std::regex e("/numbers/(\\d+)");
    //\d表示数组字符，+表示出现一次或多次；匹配以/numbers/开始，跟了一个或多个数字字符的字符串
    //并在提取过程中提取这个匹配到的数字字符串
    std::smatch matches; //存放提取到的内容，第一个存放的是原始字符串
    bool ret=std::regex_match(str,matches,e);
    if(ret==false)
    {
        return -1;
    }
    for(auto &s:matches)
    {
        std::cout<<s<<std::endl;
    }
    return 0;
}