#include "dataFile/DataFile.h"
#include <fstream>
#include <sstream>
#include "Logger.h"
#include <iostream>

bool DataFile::loadFromFile(const std::string& filePath, const Config& config) {
    data_.clear();
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    auto settings = config.getDataFileSettings();
    int start = settings.startPosition;
    int end = settings.endPosition;
    std::string line;
    int fileLineNum = 0;
    while (std::getline(file, line)) {
        ++fileLineNum;
        if (!line.empty()) {
            int lineLen = static_cast<int>(line.size());
            if (lineLen < start || (end > 0 && lineLen < end)) {
                getLogger()->warn("Data File: [line {}] Line too short for start/end. Skipping. Line: '{}' (len={}), start={}, end={}", fileLineNum, line, lineLen, start, end);
                continue;
            }
            int s = std::max(0, std::min(start, lineLen));
            int e = (end <= 0 || end > lineLen) ? lineLen : end;
            if (s < e) {
                data_.push_back(line.substr(s, e - s));
            }
            // else: do not insert anything
        }
    }
    testPrint();
    return true;
}

void DataFile::testPrint() const {
    std::cout << "DataFile contents:" << std::endl;
    int idx = 0;
    for (const auto& line : data_) {
        std::cout << "[" << idx << "] " << line << std::endl;
        ++idx;
    }
}




void DataFile::clear() {
    data_.clear();
}

const std::vector<std::string>& DataFile::getData() const {
    return data_;
}

int DataFile::getIndex(const std::string& data) const {
    for (size_t i = 0; i < data_.size(); ++i) {
        if (data_[i] == data) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
        
