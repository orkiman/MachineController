#pragma once
#include <vector>
#include <string>
#include <QString>

class BarcodeFile {
public:
    BarcodeFile() = default;
    bool loadFromFile(const QString& filePath);

    void clear();
    const std::vector<std::string>& getBarcodes() const;
    int getIndex(const std::string& barcode) const;
private:
    std::vector<std::string> barcodes_;
};
