#include <iostream>

// 饿汉模式：程序一运行就创建对象，空间换事件
//  class Singleton
//  {
//  private:
//      static Singleton _eton;
//      Singleton() : _data(99)
//      {
//          std::cout<<"单例构造完成"<<std::endl;
//      }
//      Singleton(const Singleton &) = delete;
//      Singleton operator=(const Singleton &) = delete;
//      ~Singleton() {}

// private:
//     int _data;

// public:
//     static Singleton &getInstance()
//     {
//         return _eton;
//     }
//     int getData() { return _data; }
// };
// Singleton Singleton::_eton;

// 懒汉模式：使用到再创建对象 --- 延迟加载
class Singleton
{
private:
    static Singleton _eton;
    Singleton() : _data(99)
    {
        std::cout << "单例构造完成" << std::endl;
    }
    Singleton(const Singleton &) = delete;
    Singleton operator=(const Singleton &) = delete;
    ~Singleton() {}
private:
    int _data;
public:
    static Singleton &getInstance()
    {
        static Singleton _eton;
        return _eton;
    }
    int getData() { return _data; }
};

int main()
{
    // std::cout << Singleton::getInstance().getData();
}