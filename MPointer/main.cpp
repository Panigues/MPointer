#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <utility>
#define MPOINTER_GC(NAME) MPointer<NAME> NAME(new NAME())

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

template <typename T>
class LinkedList {
private:
    struct Node {
        T data;
        Node* next;
        explicit Node(T data) : data(std::move(data)), next(nullptr) {}
    };

    Node* head;
    std::mutex mutex;

public:
    LinkedList() : head(nullptr) {}

    ~LinkedList() {
        Node* current = head;
        while (current) {
            Node* next = current->next;
            delete current;
            current = next;
        }
    }

    void pushFront(T data) {
        std::lock_guard<std::mutex> lock(mutex);
        Node* newNode = new Node(data);
        newNode->next = head;
        head = newNode;
    }

    void remove(Node* node) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!node) return;
        Node* current = head;
        Node* previous = nullptr;
        while (current && current != node) {
            previous = current;
            current = current->next;
        }
        if (current) {
            if (previous) {
                previous->next = current->next;
            } else {
                head = current->next;
            }
            delete current;
        }
    }

    Node* find(T data) {
        std::lock_guard<std::mutex> lock(mutex);
        Node* current = head;
        while (current) {
            if (current->data == data) return current;
            current = current->next;
        }
        return nullptr;
    }

    void traverse(std::function<void(T&)> callback) {
        std::lock_guard<std::mutex> lock(mutex);
        Node* current = head;
        while (current) {
            callback(current->data);
            current = current->next;
        }
    }
};

class MyNode {
public:
    MPointer<int> data;

    explicit MyNode(int value) : data(MPointer<int>::New()) {
        *data = value;
    }
};

int main() {
    LinkedList<MyNode> list;
    list.pushFront(MyNode(10));
    list.pushFront(MyNode(20));
    list.pushFront(MyNode(30));

    list.traverse([](MyNode& node) {
        std::cout << *node.data << std::endl;
    });

    MPointerGC::getInstance()->runGC();

    return 0;
}