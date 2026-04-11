#pragma once
#include <string>
#include <memory>
#include <atomic>

class JackHandler {
public:
    explicit JackHandler(const std::string&);
    ~JackHandler();

    void watch(const std::atomic<bool>&);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};