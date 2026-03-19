#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <ctime>

#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL INF
#define LOG(level, format, ...)                                                             \
    do                                                                                      \
    {                                                                                       \
        if (level < LOG_LEVEL)                                                              \
            break;                                                                          \
        time_t t = time(NULL);                                                              \
        struct tm *ltm = localtime(&t);                                                     \
        char tmp[32] = {0};                                                                 \
        strftime(tmp, 31, "%H:%M:%S", ltm);                                                 \
        fprintf(stdout, "[%s %s:%d] " format "\n", tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)
//__FILE__:当前文件名  __LINE__：所在行  __VA_ARGS__：不定参
//...：可变参数，对应后面的变量  "[%s:%d] " format "\n"：拼接成最终输出格式
// ## 的作用：当你不传参数时，防止编译报错

#define BUFFER_DEFAULT_SIZE 1024

class Buffer
{
private:
    std::vector<char> _buffer; // 使用vector进行内存空间管理
    uint64_t _reader_idx;      // 读偏移
    uint64_t _writer_idx;      // 写偏移
public:
    Buffer() : _reader_idx(0), _writer_idx(0), _buffer(BUFFER_DEFAULT_SIZE) {}
    char *Begin() { return &*_buffer.begin(); }
    // 获取当前写入起始地址,_buffer的空间起始地址，加上写偏移量
    char *WritePosition() { return Begin() + _writer_idx; }
    // 获取当前读取起始地址
    char *ReadPosition() { return Begin() + _reader_idx; }
    // 获取缓冲区末尾空闲空间大小 --- 写偏移之后的空闲空间
    uint64_t TailIdleSize() { return _buffer.size() - _writer_idx; }
    // 获取缓冲区起始空闲空间大小 --- 读偏移之前的空闲空间
    uint64_t HeadIdleSize() { return _reader_idx; }
    // 获取可读数据大小
    uint64_t ReadAbleSize() { return _writer_idx - _reader_idx; }
    // 将读偏移向后移动
    void MoveReadOffset(uint64_t len)
    {
        // 向后移动的大小，必须小于可读数据大小
        assert(len <= ReadAbleSize());
        _reader_idx += len;
    }
    // 将写偏移向后移动
    void MoveWriteOffset(uint64_t len)
    {
        // 向后移动的大小，必须小于当前后边的空闲空间大小
        assert(len <= TailIdleSize());
        _writer_idx += len;
    }
    // 确保可写空间足够（整体空闲空间够了就移动数据，否则就扩容）
    void EnsureWriteSpace(uint64_t len)
    {
        // 如果末尾空闲空间大小足够，直接返回
        if (TailIdleSize() >= len)
        {
            return;
        }
        // 末尾空闲空间不够，则判断加上起始位置的空闲空间大小是否足够,够了就将数据移动到起始位置
        if (len <= TailIdleSize() + HeadIdleSize())
        {
            // 将数据移动到起始位置
            uint64_t rsz = ReadAbleSize();                            // 把当前数据大小保存起来
            std::copy(ReadPosition(), ReadPosition() + rsz, Begin()); // 把可读的数据拷贝到起始位置
            _reader_idx = 0;                                          // 将读偏移归0
            _writer_idx = rsz;                                        // 将写位置置为可读数据大小，因为当前的可读数据大小就是写偏移量
        }
        else
        {
            // 总体空间不够，则需要扩容，不移动数据，直接在写偏移之后扩容足够空间即可
            _buffer.resize(_buffer.size() + len);
        }
    }
    // 写入数据
    void Write(const void *data, uint64_t len)
    {
        // 1.确保有足够空间 2.拷贝数据进去
        EnsureWriteSpace(len);
        const char *d = (const char *)data; // void*没有步长，不知道+len偏移到哪里
        std::copy(d, d + len, WritePosition());
    }
    void WriteAndPush(const void *data, uint64_t len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }
    void WriteString(const std::string &data)
    {
        Write(data.c_str(), data.size());
    }
    void WriteStringAndPush(const std::string &data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
    }
    void WriteBuffer(Buffer &data)
    {
        Write(data.ReadPosition(), data.ReadAbleSize());
    }
    void WriteBufferAndPush(Buffer &data)
    {
        WriteBuffer(data);
        MoveWriteOffset(data.ReadAbleSize());
    }
    // 读取数据
    void Read(void *buf, uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::copy(ReadPosition(), ReadPosition() + len, (char *)buf);
    }
    void ReadAndPop(void *buf, uint64_t len)
    {
        Read(buf, len);
        MoveReadOffset(len);
    }
    std::string ReadAsString(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::string str;
        str.resize(len);
        Read(&str[0], len);
        return str;
    }
    std::string ReadAsStringAndPop(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::string str = ReadAsString(len);
        MoveReadOffset(len);
        return str;
    }
    char *FindCRLF() // 寻找换行字符
    {
        void *res = memchr(ReadPosition(), '\n', ReadAbleSize());
        return (char *)res;
    }
    // 获取一行数据
    std::string GetLine()
    {
        char *pos = FindCRLF();
        if (pos == nullptr)
            return "";
        // +1是为了把换行字符也取出来
        return ReadAsString(pos - ReadPosition() + 1);
    }
    std::string GetLineAndPop()
    {
        std::string str = GetLine();
        MoveReadOffset(str.size());
        return str;
    }

    // 清空缓冲区
    void Clear()
    {
        // 只需要将偏移量归0即可
        _writer_idx = 0;
        _reader_idx = 0;
    }
};

#define MAX_LISTEN 1024
class Socket
{
private:
    int _sockfd;

public:
    Socket() : _sockfd(-1) {}
    Socket(int fd) : _sockfd(fd) {}
    ~Socket() { Close(); }
    int Fd() { return _sockfd; }
    // 创建套接字
    bool Create()
    {
        _sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (_sockfd < 0)
        {
            ERR_LOG("CREATE SOCKET FAILED!!");
            return false;
        }
        return true;
    }
    // 绑定地址信息
    bool Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        // int bind(int sockfd,struct sockaddr* addr,socklent_t len);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("BIND ADDRESS FAILED!");
            return false;
        }
        return true;
    }
    // 开始监听
    bool Listen(int backlog = MAX_LISTEN)
    {
        // int listen(int backlog)
        int ret = listen(_sockfd, backlog);
        if (ret < 0)
        {
            ERR_LOG("SOCKET LISTEN FAILED!");
            return false;
        }
        return true;
    }
    // 向服务器发起连接
    bool Connect(const std::string &ip, uint16_t port)
    {
        // int connect(int sockfd,struct sockaddr* addr,socklen_t len);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("CONNECT SERVER FAILED!");
            return false;
        }
        return true;
    }
    // 获取新连接
    int Accept()
    {
        // int accept(int sockfd,struct sockaddr *addr,socklen_t *len);
        int newfd = accept(_sockfd, NULL, NULL);
        if (newfd < 0)
        {
            ERR_LOG("SOCKET ACCEPT FAILED!");
            return -1;
        }
        return newfd;
    }
    // 接收数据
    ssize_t Recv(void *buf, size_t len, int flag = 0) // 0时候阻塞
    {
        // ssize_t recv(int sockfd,void *buf,size_t int flag);
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if (ret <= 0)
        {
            // EAGAIN 当前socket的接收缓冲区中没有数据了，在非阻塞的情况下才会有这个错误
            // EINTR 当前socket的阻塞等待，被信号打断了
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0; // 表示这次没有接收到数据
            }
            ERR_LOG("SOCKET RECV FAILED!!");
            return -1;
        }
        return ret; // 实际接收的数据长度
    }
    ssize_t NonBlockRecv(void *buf, size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前接收为非阻塞接收
    }
    // 发送数据
    ssize_t Send(const void *buf, size_t len, int flag = 0)
    {
        // ssize_t send(int sockfd,void *data,size_t len,int flag);
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret < 0)
        {
            ERR_LOG("SOCKET SEND FAILED!!");
            return -1;
        }
        return ret; // 实际发送的数据长度
    }
    ssize_t NonBlockSend(void *buf, size_t len)
    {
        return Send(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前发送为非阻塞接收
    }
    // 关闭套接字
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }
    // 创建一个服务器端连接
    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
    {
        // 1.创建套接字 2.绑定地址 3.开始监听 4.设置非阻塞 5.启动地址重用
        if (Create() == false)
            return false;
        if (block_flag) // 默认阻塞
            NonBlock();
        if (Bind(ip, port) == false)
            return false;
        if (Listen() == false)
            return false;
        ReuseAddress();
        return true;
    }
    // 创建一个客户端连接
    bool CreateClient(uint16_t port, const std::string &ip)
    {
        // 1.创建套接字 2.连接服务器
        if (Create() == false)
            return false;
        if (Connect(ip, port) == false)
            return false;
        return true;
    }
    // 设置套接字选项 --- 开启地址端口重用
    void ReuseAddress()
    {
        // int setsockopt(int fd,int level,int optname,void *val,int vallen)
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
    }
    // 设置套接字阻塞属性 --- 设置为非阻塞
    void NonBlock()
    {
        // int fcntl(int fd,int cmd,.../*arg*/);
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
};

class Poller;
class Channel
{
private:
    int _fd;
    Poller *_poller;
    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; // 当前连接触发的事件
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;  // 可读事件被触发的回调函数
    EventCallback _write_callback; // 可写事件被触发的回调函数
    EventCallback _error_callback; // 错误事件被触发的回调函数
    EventCallback _close_callback; // 连接断开事件被触发的回调函数
    EventCallback _event_callback; // 任意事件被触发的回调函数
public:
    Channel(Poller *poller, int fd) : _fd(fd), _events(0), _revents(0), _poller(poller) {}
    int Fd() { return _fd; }
    uint32_t Events() { return _events; }                     // 获取想要监控的事件
    void SetREvents(uint32_t revents) { _revents = revents; } // 设置实际就绪的事件
    void SetReadCallback(const EventCallback &cb) { _read_callback = cb; }
    void SetWriteCallback(const EventCallback &cb) { _write_callback = cb; }
    void SetErrorCallback(const EventCallback &cb) { _error_callback = cb; }
    void SetCloseCallback(const EventCallback &cb) { _close_callback = cb; }
    void SetEventCallback(const EventCallback &cb) { _event_callback = cb; }
    // 当前监控了可读
    bool ReadAble() { return _events & EPOLLIN; }
    // 当前监控了可写
    bool WriteAble() { return _events & EPOLLOUT; }
    // 启动读事件监控
    void EnableRead()
    {
        _events |= EPOLLIN;
        Update();
    }
    // 启动写事件监控
    void EnableWrite()
    {
        _events |= EPOLLOUT;
        Update();
    }
    // 关闭读事件监控
    void DisableRead()
    {
        _events &= ~EPOLLIN;
        Update();
    }
    // 关闭写事件监控
    void DisableWrite()
    {
        _events &= ~EPOLLOUT;
        Update();
    }
    // 关闭所有事件监控
    void DisableALL()
    {
        _events &= 0;
        Update();
    }
    // 移除监控(先声明，在下面实现)
    void Remove();
    // 添加修改监控
    void Update();
    // 事件处理，一旦连接出发了事件，就调用这个函数,自己触发了什么事件如何处理自己决定
    void HandleEvent()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            // 不管任何事件，都调用的回调函数
            if (_event_callback)
                _event_callback();
            // 可读，对方挂断，优先事件发生都触发可读事件回调
            if (_read_callback) // 可读事件存在
                _read_callback();
        }
        // 有可能会释放连接的操作时间，一次只处理一个
        if (_revents & EPOLLOUT)
        {
            // 不管任何事件，都调用的回调函数
            if (_event_callback)
                _event_callback(); // 放到事件处理完毕后调用，刷新活跃度
            if (_write_callback)
                _write_callback();
        }
        else if (_revents & EPOLLERR)
        {
            // 错误/挂断是，处理一个就够了，处理完连接就没了
            // 错误事件，一旦出错，就会释放连接，因此要放到前面调用任意回调
            if (_error_callback)
            {
                _error_callback();
            }
        }
        else if (_revents & EPOLLHUP)
        {
            // 连接断开
            if (_close_callback)
            {
                _close_callback();
            }
        }
    }
};

#define MAX_EPOLLEVENTS 1024
class Poller
{
private:
    int _epfd;
    struct epoll_event _evs[MAX_EPOLLEVENTS]; // 存放触发事件的fd
    std::unordered_map<int, Channel *> _channels;

private:
    // 对epoll的直接操作
    void Update(Channel *channel, int op)
    {
        // int epoll_ctl(int epfd,int op,int fd,struct epoll_event *ev);
        int fd = channel->Fd();
        struct epoll_event ev;
        ev.events = channel->Events();
        ev.data.fd = fd; // 要监控的描述符
        int ret = epoll_ctl(_epfd, op, fd, &ev);
        if (ret < 0)
        {
            ERR_LOG("EPOLLCTL FAILED!");
            abort(); // 退出程序
        }
        return;
    }
    // 判断一个Channel是否已经添加了事件监控
    bool HasChannel(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it == _channels.end())
        {
            return false;
        }
        return true;
    }

public:
    Poller()
    {
        _epfd = epoll_create(1);
        if (_epfd < 0)
        {
            ERR_LOG("EPOLL CREATE FAILED!!");
            abort(); // 退出程序
        }
    }
    // 添加或修改监控
    void UpdateEvents(Channel *channel)
    {
        bool ret = HasChannel(channel);
        if (ret == false)
        {
            // 不存在则添加
            _channels.insert(std::make_pair(channel->Fd(),channel));
            Update(channel, EPOLL_CTL_ADD);
        }
        else
        {
            // 存在则更新
            Update(channel, EPOLL_CTL_MOD);
        }
    }
    // 移除监控
    void RemoveEvent(Channel *channel)
    {
        Update(channel, EPOLL_CTL_DEL); // 从epoll中删除
        auto it = _channels.find(channel->Fd());
        if (it != _channels.end())
        {
            _channels.erase(it); // 从监控列表中移除
        }
    }
    // 开始监控，返回活跃连接
    void Poll(std::vector<Channel*> *active) // active输出型参数，放有事件的Channel
    {
        // int epoll_wait(int epfd,struct epoll_event *evs,int maxevents,int timeout)
        int nfds = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1); // 阻塞监控
        // 会把就绪的channel放到_evs中，nfds表示就绪的事件数量
        if (nfds < 0)
        {
            if (errno == EINTR)
            {
                return; // 被信号打断了
            }
            ERR_LOG("EPOLL WAIT ERROR:%s\n", strerror(errno));
            abort(); // 退出程序
        }
        for (int i = 0; i < nfds; i++)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());
            it->second->SetREvents(_evs[i].events); // 设置实际就绪的事件
            active->push_back(it->second);
        }
    }
};

void Channel::Remove() { _poller->RemoveEvent(this); }
void Channel::Update() { _poller->UpdateEvents(this); }