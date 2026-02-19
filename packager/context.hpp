#pragma once

#include <iostream>

namespace crosside {

class Context {
public:
    explicit Context(bool verbose = true) : verbose_(verbose) {}

    template <typename... Args>
    void log(const Args &...args) const {
        (std::cout << ... << args) << '\n';
    }

    template <typename... Args>
    void warn(const Args &...args) const {
        std::cerr << "[warn] ";
        (std::cerr << ... << args) << '\n';
    }

    template <typename... Args>
    void error(const Args &...args) const {
        std::cerr << "[error] ";
        (std::cerr << ... << args) << '\n';
    }

    bool verbose() const { return verbose_; }

private:
    bool verbose_;
};

} // namespace crosside
