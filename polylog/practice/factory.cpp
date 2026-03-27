#include <iostream>
#include <memory>

class Fruit
{
public:
    virtual void name() = 0;
};

class Apple : public Fruit
{
public:
    void name() override
    {
        std::cout << "苹果" << std::endl;
    }
};

class Banana : public Fruit
{
public:
    void name() override
    {
        std::cout << "香蕉" << std::endl;
    }
};

class Animal
{
public:
    virtual void name() = 0;
};

class Lamp : public Animal
{
public:
    void name() override
    {
        std::cout << "山羊" << std::endl;
    }
};

class Dog : public Animal
{
public:
    void name() override
    {
        std::cout << "狗" << std::endl;
    }
};

class Factory
{
public:
    virtual std::shared_ptr<Fruit> getFruit(const std::string &name) = 0;
    virtual std::shared_ptr<Animal> getAnimal(const std::string &name) = 0;
};

class FruitFactory : public Factory
{
public:
    std::shared_ptr<Animal> getAnimal(const std::string &name)
    {
        return std::shared_ptr<Animal>();
    }

    std::shared_ptr<Fruit> getFruit(const std::string &name)
    {
        if (name == "苹果")
        {
            return std::make_shared<Apple>();
        }
        else
        {
            return std::make_shared<Banana>();
        }
    }
};

class AnimalFactory : public Factory
{
public:
    std::shared_ptr<Fruit> getFruit(const std::string &name)
    {
        return std::shared_ptr<Fruit>();
    }

    std::shared_ptr<Animal> getAnimal(const std::string &name)
    {
        if (name == "狗")
        {
            return std::make_shared<Dog>();
        }
        else
        {
            return std::make_shared<Lamp>();
        }
    }
};

class FactoryProducer
{
public:
    static std::shared_ptr<Factory> create(const std::string &name)
    {
        if (name == "水果")
        {
            return std::make_shared<FruitFactory>();
        }
        else
        {
            return std::make_shared<AnimalFactory>();
        }
    }
};

// class FruitFactory
// {
// public:
//     static std::shared_ptr<Fruit> create(const std::string &name)
//     {
//         if (name == "苹果")
//         {
//             return std::make_shared<Apple>();
//         }
//         else if (name == "香蕉")
//         {
//             return std::make_shared<Banana>();
//         }
//         return nullptr;
//     }
// };

// class FruitFactory
// {
// public:
//     virtual std::shared_ptr<Fruit> create() = 0;
// };

// class AppleFactory : public FruitFactory
// {
// private:
//     std::shared_ptr<Fruit> create() override
//     {
//         return std::make_shared<Apple>();
//     }
// };

// class BananaFactory : public FruitFactory
// {
// private:
//     std::shared_ptr<Fruit> create() override
//     {
//         return std::make_shared<Banana>();
//     }
// };

int main()
{
    std::shared_ptr<Factory> f = FactoryProducer::create("水果");
    std::shared_ptr<Fruit> ff = f->getFruit("香蕉");
    ff->name();

    std::shared_ptr<Factory> aa = FactoryProducer::create("动物");
    std::shared_ptr<Animal> a = aa->getAnimal("狗");
    a->name();
    // std::shared_ptr<FruitFactory> ff(new AppleFactory);
    // std::shared_ptr<Fruit> fruit = ff->create();
    // fruit->name();
    // ff.reset(new BananaFactory());
    // fruit = ff->create();
    // fruit->name();
    // std::shared_ptr<Fruit> fruit = FruitFactory::create("香蕉");
    // fruit->name();
    // return 0;
}