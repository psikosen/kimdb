//
// Created by ray on 4/19/2023.
//

#ifndef KIMDB_KIMFILEHEAD_H
#define KIMDB_KIMFILEHEAD_H

//
// Created by ray on 4/19/2023.
//

#include "KimFileHead.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <regex>

struct KimFileHeaderV3 {
    uint8_t FileFormatVersion;
    uint32_t FileSize;
    uint16_t NumTables;
    uint32_t LinkKeysSectionOffset;
};

struct ColumnHeader {
    char ColumnName[64];
    uint8_t DataType;
    uint16_t DataSize;
    bool IsIndexed;
    bool IsLinkKey;
    bool IsUnique;
    bool IsPrimaryKey;
};

struct TableHeader {
    char TableName[64];
    uint16_t NumColumns;
    uint16_t NumRows;
    int16_t LinkColumnIndex;
    bool HasUniqueRows;
};



class KimTable {
public:
    KimTable() {
        std::memset(&header, 0, sizeof(header));
    }

    TableHeader header;
    std::vector<ColumnHeader> columnHeaders;
    std::vector<std::vector<std::string>> rows; // Replace std::string with the appropriate data type
    void addRow(const std::vector<std::string>& rowData);
    void setTableName(const std::string& tableName);
    std::vector<std::string> selectRowWithSQL(const  KimTable& table,const std::string& sqlQuery) const;
    void loadFromFile(const std::string& fileName);
    std::string select(const KimTable& table, size_t rowIndex, size_t columnIndex);
    std::vector<std::string> selectRow( const KimTable& table, size_t rowIndex);
    void deleteRow(size_t rowIndex);
    void updateRow(KimTable& table, size_t rowIndex, size_t columnIndex, const std::string& newValue);
    std::vector<std::string> selectRowWithSQL(const KimTable& table, const std::string& sqlQuery);
    std::vector<std::vector<std::string>> selectRowsWithSQL(const KimTable& table, const std::string& sqlQuery);

    void createTable(const std::vector<std::string> &columnNames);

    void writeToFile(const std::string &fileName);
};


#endif //KIMDB_KIMFILEHEAD_H
