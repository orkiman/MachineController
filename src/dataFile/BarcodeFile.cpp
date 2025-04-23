#include "dataFile/BarcodeFile.h"
#include <fstream>
#include <sstream>

bool BarcodeFile::loadFromFile(const QString& filePath) {
    barcodes_.clear();
    std::ifstream file(filePath.toStdString());
    if (!file.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            barcodes_.push_back(line);
        }
    }
    return true;
}



void BarcodeFile::clear() {
    barcodes_.clear();
}

const std::vector<std::string>& BarcodeFile::getBarcodes() const {
    return barcodes_;
}

int BarcodeFile::getIndex(const std::string& barcode) const {
    for (size_t i = 0; i < barcodes_.size(); ++i) {
        if (barcodes_[i] == barcode) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
