#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

class MPointerGC;

template <typename T>
class MPointer {
private:
    T* ptr;
    int id;
    static MPointerGC* gc;

public:
    MPointer() : ptr(nullptr), id(0) {}

    static MPointer<T> New() {
        MPointer<T> mp;
        mp.ptr = new T();
        mp.id = gc->assignId(mp.ptr);
        gc->addPointer(mp.ptr, mp.id);
        return mp;
    }

    T& operator*() {
        if (!ptr) {
            throw std::runtime_error("Puntero nulo");
        }
        return *ptr;
    }

    T* operator->() {
        if (!ptr) {
            throw std::runtime_error("Puntero nulo");
        }
        return ptr;
    }

    MPointer<T>& operator=(const MPointer<T>& other) {
        if (this != &other) {
            gc->removePointer(ptr, id);
            ptr = other.ptr;
            id = other.id;
            gc->addPointer(ptr, id);
        }
        return *this;
    }

    MPointer<T>& operator=(T value) {
        if (!ptr) {
            throw std::runtime_error("Puntero nulo");
        }
        *ptr = value;
        return *this;
    }

    int getId() const {
        return id;
    }

    ~MPointer() {
        gc->removePointer(ptr, id);
        delete ptr;
    }
};

template <typename T>
MPointerGC* MPointer<T>::gc = MPointerGC::getInstance();

class MPointerGC {
private:
    static MPointerGC* instance;
    std::unordered_map<void*, int> pointers;
    std::atomic<int> nextId;
    std::mutex mutex;

    MPointerGC() : nextId(0) {}

public:
    static MPointerGC* getInstance() {
        static std::once_flag once;
        std::call_once(once, []() {
            instance = new MPointerGC();
        });
        return instance;
    }

    int assignId(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex);
        int id = nextId++;
        pointers[ptr] = id;
        return id;
    }

    void removePointer(void* ptr, int id) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = pointers.find(ptr);
        if (it != pointers.end() && it->second == id) {
            pointers.erase(it);
        }
    }

    void runGC() {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto it = pointers.begin(); it != pointers.end();) {
            if (it->second == 0) {
                delete it->first;
                it = pointers.erase(it);
            } else {
                ++it;
            }
        }
    }
};

MPointerGC* MPointerGC::instance = nullptr;