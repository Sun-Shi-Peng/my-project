#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
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
        if (len == 0)
            return;
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
        if (len == 0)
            return;
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
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("SOCKET RECV FAILED!!");
            return -1;
        }
        return ret; // 实际发送的数据长度
    }
    ssize_t NonBlockSend(void *buf, size_t len)
    {
        if (len == 0)
            return 0;
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
        ReuseAddress();
        if (Bind(ip, port) == false)
            return false;
        if (Listen() == false)
            return false;
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
class EventLoop;
class Channel
{
private:
    int _fd;
    EventLoop *_loop;
    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; // 当前连接触发的事件
    using EventCallback = std::function<void()>;
    EventCallback _read_callback;  // 可读事件被触发的回调函数
    EventCallback _write_callback; // 可写事件被触发的回调函数
    EventCallback _error_callback; // 错误事件被触发的回调函数
    EventCallback _close_callback; // 连接断开事件被触发的回调函数
    EventCallback _event_callback; // 任意事件被触发的回调函数
public:
    Channel(EventLoop *loop, int fd) : _fd(fd), _events(0), _revents(0), _loop(loop) {}
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
    void UpdateEvent(Channel *channel)
    {
        bool ret = HasChannel(channel);
        if (ret == false)
        {
            // 不存在则添加
            _channels.insert(std::make_pair(channel->Fd(), channel));
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
    void Poll(std::vector<Channel *> *active) // active输出型参数，放有事件的Channel
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

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
// 定时器任务类
class TimerTask
{
private:
    uint64_t _id;         // 定时任务对象ID
    uint32_t _timeout;    // 定时任务的超时时间
    bool _canceled;       // false-表示没有被取消，true-表示被取消了
    TaskFunc _task_cb;    // 定时器对象要执行的定时任务
    ReleaseFunc _release; // 用于删除TimerWheel中保存的定时器对象信息

public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb)
        : _id(id),
          _timeout(delay),
          _task_cb(cb),
          _canceled(false)
    {
    }
    ~TimerTask()
    {
        if (_canceled == false)
            _task_cb();
        _release();
    }
    void Cancel() { _canceled = true; }
    void SetRelease(const ReleaseFunc &cb) { _release = cb; }
    uint32_t DelayTime() { return _timeout; }
};

class TimerWheel
{
private:
    using WeakTask = std::weak_ptr<TimerTask>;
    using PtrTask = std::shared_ptr<TimerTask>;
    int _tick;                                      // 当前的秒针，走到哪里是放哪里，适当哪里，就相当于执行那里的任务
    int _capacity;                                  // 表盘最大数量 --- 起始就是最大延迟时间
    std::vector<std::vector<PtrTask>> _wheel;       // 时钟
    std::unordered_map<uint64_t, WeakTask> _timers; // 存指向时钟任务对象的虚指针

    EventLoop *_loop;
    std::unique_ptr<Channel> _timer_channel;
    int _timerfd; // 定时器描述符 -- 可读事件回调，就是读取计数器，执行定时任务
private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
            _timers.erase(it); // 从哈希表中删除这个id
    }
    static int CreateTimerfd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            ERR_LOG("TIMERFD CREATE FAILED!");
            abort();
        }
        struct itimerspec itm;
        itm.it_value.tv_sec = 1; // 设置第⼀次超时的时间
        itm.it_value.tv_nsec = 0;

        itm.it_interval.tv_sec = 1; // 第⼀次超时后，每隔多长时间超时
        itm.it_interval.tv_nsec = 0;
        timerfd_settime(timerfd, 0, &itm, NULL); // 启动定时器
        return timerfd;
    }
    int ReadTimerfd()
    {
        uint64_t times;
        // 需要注意的是定时器超时后,则描述符触发可读事件，必须读取8字节的数据，保存的是自上次启动定时器或read后的超时次数
        int ret = read(_timerfd, &times, sizeof(times));
        if (ret < 0)
        {
            ERR_LOG("READ TIMEFD FAILED!");
            abort();
        }
        return times;
    }
    // 这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
    void RunTimeTask()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
    }
    void OnTime()
    {
        int times = ReadTimerfd(); // 读次数
        for (int i = 0; i < times; i++)
        { // 循环执行
            RunTimeTask();
        }
    }
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb) // 添加定时任务
    {
        PtrTask pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
        _timers[id] = WeakTask(pt);
    }
    void TimerRefreshInLoop(uint64_t id) // 刷新/延迟定时任务
    {
        // 通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
        auto it = _timers.find(id);
        if (it == _timers.end())
            return;                     // 没找到定时任务，无法刷新，没法延迟
        PtrTask pt = it->second.lock(); // lock获取weak_ptr管理的对象对应的shared_ptr，weak_ptr不能直接使用，必须lock()转成shared_ptr 才能用
        int delay = pt->DelayTime();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }
    void TimerCancelInLoop(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
            return;
        PtrTask pt = it->second.lock();
        if (pt)
            pt->Cancel();
    }

public:
    TimerWheel(EventLoop *loop) : _capacity(60), _tick(0), _wheel(_capacity), _loop(loop),
                                  _timerfd(CreateTimerfd()), _timer_channel(new Channel(_loop, _timerfd))
    {
        _timer_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));
        _timer_channel->EnableRead(); // 启动读事件监控
    }
    // 定时器中有个_timers，定时器信息的操作有可能在多线程中进行，因此需要考虑线程安全问题
    // 如果不想加锁，那么就把对定时器的所有操作，都放到一个线程中进行
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb); // 添加定时任务
    void TimerRefresh(uint64_t id);                                 // 刷新/延迟定时任务
    void TimerCancel(uint64_t id);
    // 存在线程安全问题，这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行
    bool HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
            return false;
        return true;
    }
};

class EventLoop
{
private:
    std::thread::id _thread_id; // 线程id
    int _event_fd;              // eventfd唤醒IO事件监控有可能导致的阻塞
    Poller _poller;             // 进行所有描述符的事件监控
    std::unique_ptr<Channel> _event_channel;
    using Functor = std::function<void()>;
    std::vector<Functor> _tasks; // 任务池
    std::mutex _mutex;           // 实现任务池操作的线程安全
    TimerWheel _timer_wheel;     // 定时器模块

private:
    // 执行任务池中的所有任务
    void RunAllTask()
    {
        std::vector<Functor> functor;
        {
            std::unique_lock<std::mutex> _losk(_mutex); // 智能锁
            _tasks.swap(functor);
        }
        for (auto &f : functor)
        {
            f();
        }
    }
    static int CreateEventFd()
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (efd < 0)
        {
            ERR_LOG("CREATE EVENTFD FAILED!!");
            abort(); // 让程序异常退出
        }
        return efd;
    }
    void ReadEventfd()
    {
        uint64_t res = 0;
        int ret = read(_event_fd, &res, sizeof(res));
        if (ret < 0)
        {
            // EINTR --- 被信号打断 EAGAIN --- 无数据可读
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }
    void WeakUpEventFd()
    {
        uint64_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        if (ret < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }

public:
    EventLoop()
        : _thread_id(std::this_thread::get_id()),
          _event_fd(CreateEventFd()),
          _event_channel(new Channel(this, _event_fd)),
          _timer_wheel(this)
    {
        // 给eventfd添加可读事件回调函数，读取eventfd事件通知次数
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd, this));
        // 启动eventfd的读事件监控
        _event_channel->EnableRead();
    }
    void Start()
    {
        while (1)
        {
            // 1.事件监控
            std::vector<Channel *> actives;
            _poller.Poll(&actives); // 会阻塞在这里
            // 2.事件处理
            for (auto &channel : actives)
            {
                channel->HandleEvent();
            }
            // 3.执行任务
            RunAllTask();
        }
    }
    bool IsInLoop() // 用于判断当前线程是否是EventLoop对应的线程
    {
        return _thread_id == std::this_thread::get_id();
        // eventloop创建时的线程id      当前线程的id
    }
    void RunInLoop(const Functor &cb) // 判断将要执行的任务是否处于当前线程中，如果是执行，不是则压入队列
    {
        if (IsInLoop())
        {
            return cb();
        }
        else
            QueueInLoop(cb);
    }
    void QueueInLoop(const Functor &cb) // 将操作压入内存池
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }
        // 唤醒有可能因为没有时间就绪，导致的epoll阻塞
        // 其实就是给eventfd写入一个数据，eventfd就会触发可读事件
        WeakUpEventFd();
    }
    // 添加/修改描述符的监控事件
    void UpdateEvent(Channel *channel) { return _poller.UpdateEvent(channel); }
    // 移除描述符的监控
    void RemoveEvent(Channel *channel) { return _poller.RemoveEvent(channel); }

    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) { return _timer_wheel.TimerAdd(id, delay, cb); }
    void TimerRefresh(uint64_t id) { return _timer_wheel.TimerRefresh(id); }
    void TimerCancel(uint64_t id) { return _timer_wheel.TimerCancel(id); }
    bool HasTimer(uint64_t id) { return _timer_wheel.HasTimer(id); }
};

typedef enum
{
    DISCONNECTED, // 连接关闭状态
    CONNECTING,   // 连接建立成功，待处理状态
    CONNECTED,    // 连接建立完成，各种设置已完成，可以通信的状态
    DISCONNECTING // 待关闭状态
} ConnStatu;
using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection>
{
private:
    uint64_t _conn_id; // 连接的唯一id，便于连接的管理和查找
    // uint64_t _timer_id; //定时器id，必须是唯一的，这块为了简单操作使用conn_id作为定时器id
    int _sockfd;                   // 连接关联的文件描述符
    bool _enable_inactive_release; // 连接是否启动非活跃连接销毁的判断标志
    EventLoop *_loop;              // 连接所关联的EventLoop
    ConnStatu _statu;              // 连接状态
    Socket _socket;                // 套接字操作管理
    Channel _channel;              // 连接的事件管理
    Buffer _in_buffer;             // 输入缓冲区 --- 存放从socket中读取到的数据
    Buffer _out_buffer;            // 输出缓冲区 --- 存放要发送给对端的数据
    Any _context;                  // 请求的接受处理上下文

    /*这四个回调函数，是让服务器模块来设置的（其实服务器模块的处理回调也是组件使用者设置的）*/
    /*换句话说，这几个回调都是组件使用者使用的*/
    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;
    ConnectedCallback _connected_callback;
    MessageCallback _message_callback;
    ClosedCallback _closed_callback;
    AnyEventCallback _event_callback;
    /*组件内的连接关闭回调--组件内设置的，因为服务器组件内会把所有的连接管理起来，一旦某个连接要关闭*/
    /*就应该从管理的地方移除掉自己的信息*/
    ClosedCallback _server_closed_callback;

private:
    /*五个channel的事件回调函数*/
    // 描述符可读事件触发后调用的函数，接收socket数据放到接收缓冲区中，然后调用_message_callback
    void HandleRead()
    {
        // 1.接收socket的数据，放到缓冲区
        char buf[65536];
        int ret = _socket.NonBlockRecv(buf, 655354);
        if (ret < 0)
        {
            // 出错了，不能直接关闭连接
            return ShutDownInLoop();
        }
        // 这里的等于0表示的是没有读取到数据，并不是连接断开了，连接断开返回的是-1
        // 将数据放入输出缓冲区
        _in_buffer.WriteAndPush(buf, ret);
        // 2.调用message_callback进行业务处理
        if (_in_buffer.ReadAbleSize() > 0)
        {
            // shared_from_this() --- 从当前对象获取自身的shared_ptr管理对象
            return _message_callback(shared_from_this(), &_in_buffer);
        }
    }
    // 描述符可写事件触发后调用的函数，将发送缓冲区中的数据进行发送
    void HandleWrite()
    {
        int ret = _socket.NonBlockSend(_out_buffer.ReadPosition(), _out_buffer.ReadAbleSize());
        if (ret < 0)
        {
            // 发送错误就该关闭连接了
            if (_in_buffer.ReadAbleSize() > 0)
            {
                _message_callback(shared_from_this(), &_in_buffer);
            }
            return ReleaseInLoop(); // 这时候就是实际的关闭释放操作了
        }
        _out_buffer.MoveReadOffset(ret); // 千万不要忘了把读偏移向后移动
        if (_out_buffer.ReadAbleSize() == 0)
        {
            _channel.DisableWrite(); // 发送缓冲区没有数据了，关闭写事件监控
        }
        // 如果当前是连接待关闭状态，则有数据，发送完数据释放连接，没有数据直接释放
        if (_statu == DISCONNECTING && _out_buffer.ReadAbleSize() == 0)
        {
            return ReleaseInLoop();
        }
        return;
    }
    // 描述符触发挂断事件
    void HandleClose()
    {
        /*一旦连接挂炖了，套接字就什么干不了了，因此有数据待处理就处理一下，完毕关闭连接*/
        if (_in_buffer.ReadAbleSize() > 0)
        {
            // shared_from_this() --- 从当前对象获取自身的shared_ptr管理对象
            return _message_callback(shared_from_this(), &_in_buffer);
        }
        return ReleaseInLoop();
    }
    // 描述符触发出错事件
    void HandleError()
    {
        return HandleClose();
    }
    // 描述符触发任意事件：1.刷新连接的活跃度 --- 延迟定时销毁任务 2.调用组件使用者的任意事件回调
    void HandleEvent()
    {
        if (_enable_inactive_release == true)
        {
            _loop->TimerRefresh(_conn_id);
        }
        if (_event_callback)
        {
            _event_callback(shared_from_this());
        }
    }
    // 连接获取之后所处的状态下，要进行各种设置（启动读监控，调用回调函数）
    void EstablishedInLoop()
    {
        // 1.修改连接状态 2.启动读事件监控 3.调用回调函数
        assert(_statu == CONNECTING); // 当前的状态必须是上层的半连接状态
        _statu = CONNECTED;           // 当前函数执行完毕，则连接进入已完成连接状态
        // 一旦启动读事件监控就有可能会立即触发读事件，如果这时候启动了非活跃连接销毁
        _channel.EnableRead();
        if (_connected_callback)
            _connected_callback(shared_from_this());
    }
    // 实际的释放接口
    void ReleaseInLoop()
    {
        // 1. 修改连接状态，将其置为DISCONNECTED
        _statu = DISCONNECTED;
        // 2.移除连接的事件监控
        _channel.Remove();
        // 3.关闭描述符
        _socket.Close();
        // 4.如果当前定时器队列中还有定时销毁任务，则取消任务
        if (_loop->HasTimer(_conn_id))
            CancleInactiveReleaseInLoop();
        // 5.调用关闭回调函数（用户），避免先移除服务器管理的连接信息导致Connection被释放，再去处理会出错，因此先调用用户的回调函数
        if (_closed_callback)
            _closed_callback(shared_from_this());
        // 移除服务器内部管理的连接信息
        if (_server_closed_callback)
            _server_closed_callback(shared_from_this());
    }

    // 这个接口并不是实际的发送接口，而只是把要发送的数据放到了发送缓冲区，启动了可写事件监控
    void SendInLoop(char *data, size_t len)
    {
        if (_statu == DISCONNECTED)
            return;
        _out_buffer.WriteAndPush(data, len);
        if (_channel.WriteAble() == false)
        {
            _channel.EnableWrite();
        }
    }
    // 这个关闭操作并非实际的连接释放操作，需要判断还有没有数据待处理，待发送
    void ShutDownInLoop()
    {
        _statu = DISCONNECTING; // 设置连接为半关闭状态
        if (_in_buffer.ReadAbleSize() > 0)
        {
            if (_message_callback)
                _message_callback(shared_from_this(), &_in_buffer);
        }
        // 要么写入数据的时候出错关闭，要么就是没有待发送数据，直接关闭
        if (_out_buffer.ReadAbleSize() > 0)
        {
            if (_channel.WriteAble() == false)
            {
                _channel.EnableWrite();
            }
        }
        if(_out_buffer.ReadAbleSize()==0)
        {
            ReleaseInLoop();
        }
    }
    void EnableInactiveReleaseInLoop(int sec);
    void CancleInactiveReleaseInLoop();
    void UpgradeInLoop(const Context &context, const ConnectedCallback &conn, const MessageCallback &msg,
                       const ClosedCallback &closed, AnyEventCallback &event);

public:
    Connection(EventLoop *loop, uint64_t conn_id, int sockfd);
    ~Connection();
    int Fd();                            // 获取管理的文件描述符
    int Id();                            // 获取连接ID
    bool Connected();                    // 是否处于CONNECTED状态
    void SetContext(const Any &context); // 设置上下文 --- 连接建立完成时
    Any *GetContext();                   // 获取上下文，返回的是指针
    void SetConnectedCallback(const ConnectedCallback &cb);
    void SetMessageCallback(const MessageCallback &cb);
    void SetClosedCallback(const ClosedCallback &cb);
    void SetAnyEventCallback(const AnyEventCallback &cb);
    void Established();                  // 连接建立就绪后，进行channel回调设置，启动读监控，调用_connedted_callback
    void Send(char *data, size_t len);   // 发送数据，将数据放到发送缓冲区，启动写事件监控
    void ShutDown();                     // 提供给组件使用者的关闭接口 --- 并不实际关闭，需要判断有没有数据待处理
    void EnableInactiveRelease(int sec); // 启动非活跃销毁，并定义多长时间无通信就是非活跃，添加定时任务
    void CancleInactiveRelease();        // 取消非活跃销毁
    void Upgrade(const Context &context, const ConnectedCallback &conn, const MessageCallback &msg,
                 const ClosedCallback &closed, AnyEventCallback &event); // 切换协议 --- 重置上下文以及阶段性处理函数
};

void Channel::Remove() { _loop->RemoveEvent(this); }
void Channel::Update() { _loop->UpdateEvent(this); }
void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) // 添加定时任务
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
void TimerWheel::TimerRefresh(uint64_t id) // 刷新/延迟定时任务
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}