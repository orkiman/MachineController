#pragma once
#include <vector>
#include <string>
#include "Config.h"

class DataFile {
public:
    DataFile() = default;
    bool loadFromFile(const std::string& filePath, const Config& config);

    void testPrint() const;
    void clear();
    const std::vector<std::string>& getData() const;
    int getIndex(const std::string& data) const;
private:
    std::vector<std::string> data_;
};
