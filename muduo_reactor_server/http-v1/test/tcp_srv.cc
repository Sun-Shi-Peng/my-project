#include "../source/server.hpp"

// 管理所有的连接
std::unordered_map<uint64_t, PtrConnection> _conns;
uint64_t conn_id = 0;
EventLoop base_loop;    //绑定到主线程
LoopThreadPool *loop_pool;
std::vector<LoopThread> threads(2);
int next_loop=0;    //下标
void ConnectionDestroy(const PtrConnection &conn)
{
    _conns.erase(conn->Id());
}
void OnConnected(const PtrConnection &conn)
{
    DBG_LOG("NEW CONNECTION:%p", conn.get());
}
void OnMessage(const PtrConnection &conn, Buffer *buf)
{
    DBG_LOG("%s", buf->ReadPosition());
    buf->MoveReadOffset(buf->ReadAbleSize());
    std::string str = "Hello ssp!";
    conn->Send(str.c_str(), str.size());
}

void NewConnection(int fd)
{
    next_loop=(next_loop+1)%2;
    PtrConnection conn(new Connection(loop_pool->NextLoop(), ++conn_id, fd));
    // 为通信套接字设置回调函数
    conn->SetMessageCallback(std::bind(OnMessage, std::placeholders::_1,std::placeholders::_2));
    conn->SetSrvClosedCallback(std::bind(ConnectionDestroy, std::placeholders::_1));
    conn->SetConnectedCallback(std::bind(OnConnected, std::placeholders::_1));
    conn->EnableInactiveRelease(10); // 启动非活跃超时销毁功能
    conn->Established();             // 就绪初始化
    _conns.insert(std::make_pair(conn_id, conn));
    DBG_LOG("NEW ---------");
}

int main()
{
    loop_pool=new LoopThreadPool(&base_loop);
    loop_pool->SetThreadCount(2);
    loop_pool->Create();
    Acceptor acceptor(&base_loop,8080);
    acceptor.SetAcceptCallback(std::bind(NewConnection,std::placeholders::_1));
    acceptor.Listen();
    while(1)
    {
        base_loop.Start();
    }
    return 0;
}