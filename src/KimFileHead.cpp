// Created by ray on 4/19/2023.
//

#include "KimFileHead.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <regex>

    void KimTable::loadFromFile(const std::string& fileName) {
    std::ifstream ifs(fileName, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open file: " << fileName << std::endl;
        return;
    }

    // Read file header
    KimFileHeaderV3 fileHeader{};
    ifs.read(reinterpret_cast<char*>(&fileHeader), sizeof(KimFileHeaderV3));

    // Read table headers and column headers
    for (int i = 0; i < fileHeader.NumTables; ++i) {
        TableHeader tableHeader;
        ifs.read(reinterpret_cast<char*>(&tableHeader), sizeof(TableHeader));
        // Set the table name in the header object
        std::memcpy(header.TableName, tableHeader.TableName, sizeof(tableHeader.TableName));

        for (int j = 0; j < tableHeader.NumColumns; ++j) {
            ColumnHeader columnHeader;
            ifs.read(reinterpret_cast<char*>(&columnHeader), sizeof(ColumnHeader));
            columnHeaders.push_back(columnHeader);
        }
    }

    // Read link keys section (if present)
    if (fileHeader.LinkKeysSectionOffset > 0) {
        ifs.seekg(fileHeader.LinkKeysSectionOffset);
        uint32_t numLinkKeys;
        ifs.read(reinterpret_cast<char*>(&numLinkKeys), sizeof(uint32_t));

        for (uint32_t i = 0; i < numLinkKeys; ++i) {
            uint32_t tableIndex, columnIndex;
            ifs.read(reinterpret_cast<char*>(&tableIndex), sizeof(uint32_t));
            ifs.read(reinterpret_cast<char*>(&columnIndex), sizeof(uint32_t));
            // Process link keys as needed for your specific application
        }
    }

    // Read data (assuming it's stored as binary format)
    // You may need to adjust this section based on the actual data storage format
    for (int i = 0; i < fileHeader.NumTables; ++i) {
        TableHeader tableHeader;
        ifs.read(reinterpret_cast<char*>(&tableHeader), sizeof(TableHeader));

        for (int j = 0; j < tableHeader.NumRows; ++j) {
            std::vector<std::basic_string<char>> row(tableHeader.NumColumns * sizeof(uint64_t));
            ifs.read(reinterpret_cast<char*>(row.data()), row.size());
             rows.push_back(row);
        }
    }

    ifs.close();
}
void KimTable::createTable(const std::vector<std::string>& columnNames) {
    // Set the number of columns for the table header
    header.NumColumns = columnNames.size();

    // Add the column names to the columnHeader vector
    for (const auto& columnName : columnNames) {
        ColumnHeader columnHeader{};
        std::memset(columnHeader.ColumnName, 0, sizeof(columnHeader.ColumnName));
        std::strncpy(columnHeader.ColumnName, columnName.c_str(), sizeof(columnHeader.ColumnName) - 1);
        columnHeaders.push_back(columnHeader);
    }
}

void KimTable::addRow(const std::vector<std::string>& rowData) {
    // Check if rowData size matches the number of columns in the table
    if (rowData.size() != columnHeaders.size()) {
        std::cerr << "Error: rowData size (" << rowData.size() << ") doesn't match the number of columns ("
                  << columnHeaders.size() << ")." << std::endl;
        return;
    }

    // Add the row data to the rows vector
    rows.push_back(rowData);
}
void KimTable::writeToFile(const std::string& fileName) {
    std::ofstream ofs(fileName, std::ios::binary);
    if (!ofs) {
        std::cerr << "Failed to open file: " << fileName << std::endl;
        return;
    }
    std::cout << "Writing to file: " << fileName << std::endl;

    // Write the KimFileHeaderV3
    KimFileHeaderV3 fileHeader;
    fileHeader.NumTables = 1; // Assuming only one table for now
    ofs.write(reinterpret_cast<const char*>(&fileHeader), sizeof(KimFileHeaderV3));

    // Write the TableHeader
    TableHeader tableHeader;
    std::memset(tableHeader.TableName, 0, sizeof(tableHeader.TableName));
    std::strncpy(tableHeader.TableName, header.TableName, sizeof(tableHeader.TableName) - 1);
    tableHeader.NumColumns = columnHeaders.size();
    ofs.write(reinterpret_cast<const char*>(&tableHeader), sizeof(TableHeader));

    // Write the number of columns
    uint32_t numColumns = columnHeaders.size();
    ofs.write(reinterpret_cast<char*>(&numColumns), sizeof(uint32_t));

    // Write the column headers
    for (const auto& columnHeader : columnHeaders) {
        ofs.write(reinterpret_cast<const char*>(&columnHeader), sizeof(ColumnHeader));
    }

    // Write the number of rows
    uint32_t numRows = rows.size();
    ofs.write(reinterpret_cast<char*>(&numRows), sizeof(uint32_t));

    // Write the rows
    for (const auto& row : rows) {
        for (const auto& value : row) {
            ofs.write(value.c_str(), value.size() + 1); // +1 for null terminator
        }
    }

    ofs.close();
    std::cout << "Writing to file: " << fileName << std::endl;

    // Check if the file has been written successfully
    if (!ofs) {
        std::cerr << "An error occurred while writing to the file: " << fileName << std::endl;
    }
    std::cout << "Writing to file: " << fileName << std::endl;

}


void KimTable::setTableName(const std::string &tableName) {
    std::memset(header.TableName, 0, sizeof(header.TableName));
    std::strncpy(header.TableName, tableName.c_str(), sizeof(header.TableName) - 1);
}


std::string select(const KimTable& table, size_t rowIndex, size_t columnIndex) {
    if (rowIndex < table.rows.size() && columnIndex < table.columnHeaders.size()) {
        return table.rows[rowIndex][columnIndex];
    } else {
        std::cerr << "Invalid row or column index" << std::endl;
        return "";
    }
}

std::vector<std::string> KimTable::selectRow(const KimTable& table, size_t rowIndex) {
    if (rowIndex < table.rows.size()) {
        return table.rows[rowIndex];
    } else {
        std::cerr << "Invalid row index" << std::endl;
        return std::vector<std::string>();
    }
}

void KimTable::deleteRow(size_t rowIndex) {
    if (rowIndex < rows.size()) {
        rows.erase(rows.begin() + rowIndex);
    }
}
void KimTable::updateRow(KimTable& table, size_t rowIndex, size_t columnIndex, const std::string& newValue) {
    if (rowIndex < table.rows.size() && columnIndex < table.columnHeaders.size()) {
        table.rows[rowIndex][columnIndex] = newValue;
    } else {
        std::cerr << "Invalid row or column index" << std::endl;
    }
}


//================================================SQL==========================================================================/
std::vector<std::string> KimTable::selectRowWithSQL(const  KimTable& table, const std::string& sqlQuery) {
    std::regex selectRowRegex(R"(SELECT\s+\*\s+FROM\s+(\w+)\s+WHERE\s+(\w+)\s*=\s*(['"]?)(.+)\3)", std::regex::icase);

    std::smatch match;
    if (std::regex_search(sqlQuery, match, selectRowRegex)) {
        std::string tableName = match[1];
        std::string columnName = match[2];
        std::string value = match[4];

        if (tableName != table.header.TableName) {
            std::cerr << "Table name does not match" << std::endl;
            return {};
        }

        int columnIndex = -1;
        for (int i = 0; i < table.header.NumColumns; ++i) {
            if (table.columnHeaders[i].ColumnName == columnName) {
                columnIndex = i;
                break;
            }
        }

        if (columnIndex == -1) {
            std::cerr << "Column not found" << std::endl;
            return std::vector<std::string>();
        }

        for (size_t i = 0; i < table.rows.size(); ++i) {
            if (table.rows[i][columnIndex] == value) {
                return selectRow(table, i);
            }
        }
    } else {
        std::cerr << "Invalid SQL query" << std::endl;
    }

    return std::vector<std::string>();
}

std::vector<std::vector<std::string>> selectRowsWithSQL(const KimTable& table, const std::string& sqlQuery) {
    std::istringstream iss(sqlQuery);
    std::string token, tableName, columnName, value;
    std::vector<std::string> tokens;
    std::vector<std::vector<std::string>> result;

    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() != 8 || tokens[0] != "SELECT" || tokens[1] != "*" || tokens[2] != "FROM" || tokens[4] != "WHERE" || tokens[6] != "=") {
        std::cerr << "Invalid SQL query" << std::endl;
        return result;
    }

    tableName = tokens[3];
    columnName = tokens[5];
    value = tokens[7];

    if (tableName != table.header.TableName) {
        std::cerr << "Table name does not match" << std::endl;
        return result;
    }

    int columnIndex = -1;
    for (int i = 0; i < table.header.NumColumns; ++i) {
        if (table.columnHeaders[i].ColumnName == columnName) {
            columnIndex = i;
            break;
        }
    }

    if (columnIndex == -1) {
        std::cerr << "Column not found" << std::endl;
        return result;
    }

    for (size_t i = 0; i < table.rows.size(); ++i) {
        if (table.rows[i][columnIndex] == value) {
            result.push_back(table.rows[i]);
        }
    }

    return result;
}

//================================================SQL==========================================================================/


/*
void WriteHeader(std::ofstream& ofs, const KimFileHeaderV3& header) {
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(KimFileHeaderV3));
}

void WriteColumnHeader(std::ofstream& ofs, const ColumnHeader& header) {
    ofs.write(header.ColumnName, sizeof(header.ColumnName));
    ofs.write(reinterpret_cast<const char*>(&header.DataType), sizeof(header.DataType));
    ofs.write(reinterpret_cast<const char*>(&header.DataSize), sizeof(header.DataSize));
    ofs.write(reinterpret_cast<const char*>(&header.IsIndexed), sizeof(header.IsIndexed));
    ofs.write(reinterpret_cast<const char*>(&header.IsLinkKey), sizeof(header.IsLinkKey));
}

void WriteTableHeader(std::ofstream& ofs, const TableHeader& header) {
    ofs.write(header.TableName, sizeof(header.TableName));
    ofs.write(reinterpret_cast<const char*>(&header.NumColumns), sizeof(header.NumColumns));
    ofs.write(reinterpret_cast<const char*>(&header.LinkColumnIndex), sizeof(header.LinkColumnIndex));
    ofs.write(reinterpret_cast<const char*>(&header.HasUniqueRows), sizeof(header.HasUniqueRows));
}
*/