#pragma once
#include "IDMMDriver.h"
#include <string>

class LiveJsonPublisher {
public:
    explicit LiveJsonPublisher(std::string path) : path_(std::move(path)) {}
    bool Publish(const IDMMDriver& driver, const std::string& identity,
                 const Measurement& measurement, const std::string& status,
                 const std::string& error);
    bool PublishDisconnected(const std::string& identity, const std::string& error);
    const std::string& Path() const { return path_; }
private:
    std::string path_;
};
