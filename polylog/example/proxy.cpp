//代理模式
#include <iostream>
#include <string>

class RentHouse
{
public:
    virtual void rentHouse()=0;
};

class Landlord:public RentHouse
{
public:
    void rentHouse()
    {
        std::cout<<"将房子租出去"<<std::endl;
    }
};

class Intermediary:public RentHouse
{
private:
    Landlord _landlord;
public:
    void rentHouse()
    {
        std::cout<<"发布招租广告"<<std::endl;
        std::cout<<"看房"<<std::endl;
       _landlord.rentHouse();
       std::cout<<"售后"<<std::endl;
    }
};

int main()
{
    Intermediary in;
    in.rentHouse();
    return 0;
}