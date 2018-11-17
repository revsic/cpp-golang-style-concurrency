#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <condition_variable>
#include <list>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>

template <typename T,
          typename = std::enable_if_t<std::is_default_constructible_v<T>>>
class RingBuffer {
public:
    RingBuffer() : size_buffer(0), buffer(nullptr) {
        // Do Nothing
    }

    RingBuffer(size_t size_buffer) :
        size_buffer(size_buffer), buffer(std::make_unique<T[]>(size_buffer))
    {
        // Do Nothing
    }

    template <typename... U>
    void emplace_back(U&&... args) {
        buffer[ptr_tail] = T(std::forward<U>(args)...);

        num_data += 1;
        ptr_tail = (ptr_tail + 1) % size_buffer;
    }

    void pop_front() {
        num_data -= 1;
        ptr_head = (ptr_head + 1) % size_buffer;
    }

    T& front() {
        return buffer[ptr_head];
    }

    const T& front() const {
        return buffer[ptr_head];
    }

    size_t size() const {
        return num_data;
    }

    size_t max_size() const {
        return size_buffer;
    }

private:
    size_t size_buffer;
    std::unique_ptr<T[]> buffer;

    size_t num_data = 0;
    size_t ptr_head = 0;
    size_t ptr_tail = 0;
};

template <typename T,
          typename Container = RingBuffer<T>> // or Container = std::list<T>
class Channel {
public:
    template <typename... U>
    Channel(U&&... args) : buffer(std::forward<U>(args)...) {
        // Do Nothing
    }

    template <typename... U>
    void Add(U&&... task) {
        std::unique_lock lock(mtx);
        cv.wait(lock, [&]{ return !runnable || buffer.size() < buffer.max_size(); });

        if (runnable) {
            buffer.emplace_back(std::forward<U>(task)...);
        }
        cv.notify_all();
    }

    std::optional<T> Get() {
        std::unique_lock lock(mtx);
        cv.wait(lock, [&]{ return !runnable || buffer.size() > 0; });

        if (!runnable) return std::nullopt;

        T given = std::move(buffer.front());
        buffer.pop_front();

        cv.notify_all();
        return std::optional<T>(std::move(given));
    }

    void Close() {
        runnable = false;
        cv.notify_all();
    } 

    struct Iterator {
        Channel& channel;
        std::optional<T> item;

        Iterator(Channel& channel, std::optional<T>&& item) : 
            channel(channel), item(std::move(item)) 
        {
            // Do Nothing
        }

        T& operator*() {
            return item.value();
        }

        const T& operator*() const {
            return item.value();
        }

        Iterator& operator++() {
            item = channel.Get();
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return item != other.item;
        }
    };

    Iterator begin() {
        return Iterator(*this, Get());
    }

    Iterator end() {
        return Iterator(*this, std::nullopt);
    }

private:
    Container buffer;
    bool runnable = true;

    std::mutex mtx;
    std::condition_variable cv;
};

template <typename T>
using UChannel = Channel<T, std::list<T>>;

#endif