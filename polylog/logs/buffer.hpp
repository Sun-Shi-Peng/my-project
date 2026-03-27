// 实现异步日志缓冲区
#ifndef __M_BUF_H__
#define __M_BUF_H__
#include "util.hpp"
#include <vector>
#include <cassert>
namespace polylog
{
#define DEFAULT_BUFFER_SIZE (1 * 1024 * 1024)
#define THRESHOLD_BUFFERSIZE (8 * 1024 * 1024)
#define INCREMENT_BUFFERSIZE (1 * 1024 * 1024)
    class Buffer
    {
    private:
        std::vector<char> _buffer;
        size_t _reader_idx; // 当前可读数据指针
        size_t _writer_idx; // 当前可写数据指针
    private:
        // 写指针进行向后偏移操作
        void moveWriter(size_t len)
        {
            assert(len + _writer_idx <= _buffer.size());
            _writer_idx += len;
        }
        // 对空间进行扩容
        void ensureEnough(size_t len)
        {
            if (len <= writeAbleSize())
                return;
            size_t new_size = 0;
            while (len > writeAbleSize())
            {
                if (_buffer.size() < THRESHOLD_BUFFERSIZE)
                {
                    new_size = 2 * _buffer.size(); // 小于阈值翻倍增长
                }
                else
                {
                    new_size = _buffer.size() + INCREMENT_BUFFERSIZE; // 否则线性增长
                }
                _buffer.resize(new_size);
            }
        }

    public:
        Buffer() : _buffer(DEFAULT_BUFFER_SIZE), _writer_idx(0), _reader_idx(0) {}
        // 向缓冲区写入数据
        void push(const char *data, size_t len)
        {
            // 1.考虑空间不够则扩容
            ensureEnough(len);
            // 1.将数据拷贝进缓冲区
            std::copy(data, data + len, &_buffer[_writer_idx]);
            // 2.将当前写入位置向后偏移
            moveWriter(len);
        }
        // 返回可写数据长度
        size_t writeAbleSize()
        {
            // 对于扩容思路来说，不存在可写空间大小，因为总是可写，因此这个接口仅仅针对固定大小缓冲区
            return (_buffer.size() - _writer_idx);
        }
        // 返回可读数据起始位置
        const char *begin()
        {
            return &_buffer[_reader_idx];
        }
        // 返回可读数据的长度
        size_t readAbleSize()
        {
            // 因为当前实现的缓冲区设计思想是双缓冲区，处理完就交换，所以不存在空间循环使用的情况
            return (_writer_idx - _reader_idx);
        }
        // 读指针进行向后偏移操作
        void moveReader(size_t len)
        {
            assert(len <= readAbleSize());
            _reader_idx += len;
        }
        // 重置读写位置，初始化缓冲区
        void reset()
        {
            _writer_idx = _reader_idx = 0;
        }
        // 对Buffer实现交换操作
        void swap(Buffer &buffer)
        {
            _buffer.swap(buffer._buffer);
            std::swap(_reader_idx, buffer._reader_idx);
            std::swap(_writer_idx, buffer._writer_idx);
        }
        // 判断缓冲区是否为空
        bool empty()
        {
            return (_reader_idx == _writer_idx);
        }
    };
}
#endif