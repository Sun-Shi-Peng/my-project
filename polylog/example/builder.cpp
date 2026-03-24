// 通过苹果笔记本电脑的构造理解建造者模式
#include <iostream>
#include <string>
#include <memory>

class Computer
{
protected:
    std::string _board;
    std::string _display;
    std::string _os;

public:
    Computer() {}
    void setBoard(const std::string &board)
    {
        _board = board;
    }
    void setDisplay(const std::string &display)
    {
        _display = display;
    }
    void showParamaters()
    {
        std::string param = "Computer Paramaters:\n";
        param += "\tBoard:" + _board + "\n";
        param += "\tDisplay:" + _display + "\n";
        param += "\tOs:" + _os + "\n";
        std::cout << param << std::endl;
    }
    virtual void setOs() = 0;
};

class MacBook : public Computer
{
public:
    void setOs() override
    {
        _os = "MacOS";
    }
};

class Builder
{
public:
    virtual void buildBoard(const std::string &board) = 0;
    virtual void buildDisplay(const std::string &display) = 0;
    virtual void buildOs() = 0;
    virtual std::shared_ptr<Computer> builder() = 0;
};

class MacBookBuilder : public Builder
{
private:
    std::shared_ptr<Computer> _computer;

public:
    MacBookBuilder() : _computer(new MacBook()) {}
    void buildBoard(const std::string &board)
    {
        _computer->setBoard(board);
    }
    void buildDisplay(const std::string &display)
    {
        _computer->setDisplay(display);
    }
    void buildOs()
    {
        _computer->setOs();
    }
    std::shared_ptr<Computer> builder()
    {
        return _computer;
    }
};

class Director
{
private:
    std::shared_ptr<Builder> _builder;

public:
    Director(Builder *builder) : _builder(builder)
    {
    }
    void construct(const std::string &board, const std::string &display)
    {
        _builder->buildBoard(board);
        _builder->buildDisplay(display);
        _builder->buildOs();
    }
};

int main()
{
    Builder *builder = new MacBookBuilder();
    std::unique_ptr<Director> director(new Director(builder));
    director->construct("华硕主板", "三星显示器");
    std::shared_ptr<Computer> computer = builder->builder();
    computer->showParamaters();
    return 0;
}