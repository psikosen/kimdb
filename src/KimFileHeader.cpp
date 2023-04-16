#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <regex>

struct KimFileHeaderV2 {
    uint8_t FileFormatVersion;
    uint32_t FileSize;
    uint16_t NumTables;
    uint32_t LinkKeysSectionOffset;
    char Magic[8];          // File identifier
    uint32_t Version;       // File version number
    char TableName[256];    // Name of the table in the file
    uint32_t NumColumns;    // Number of columns in the table
    uint32_t NumRows;       // Number of rows in the table
    uint32_t PrimaryKeyCol; // Index of the primary key column
    uint32_t LinkKeyCol;    // Index of the link key column
    uint32_t JoinToken;     // Token used for inner joins
};


struct ColumnHeader {
    char ColumnName[64];
    uint8_t DataType;
    uint16_t DataSize;
    bool IsIndexed;
    bool IsLinkKey;
    bool IsIdentifier;
    bool IsPrimaryKey; // new field for whether column is part of primary key
};

struct TableHeader {
    char TableName[64];
    uint16_t NumColumns;
    uint16_t LinkColumnIndex;
    uint16_t IdentifierColumnIndex;
    uint16_t PrimaryKeyColumnIndex; // new field for primary key column
    bool HasUniqueRows;
};

class KimTable {
public:
    KimFileHeaderV2 header;
    std::vector<ColumnHeader> columnHeaders;
    std::vector<std::vector<std::string>> rows; // Replace std::string with the appropriate data type

    void loadFromFile(const std::string& fileName) {
        std::ifstream ifs(fileName, std::ios::binary);
        if (!ifs) {
            std::cerr << "Failed to open file: " << fileName << std::endl;
            return;
        }

        // Read file header
        ifs.read(reinterpret_cast<char*>(&header), sizeof(KimFileHeaderV2));

        // Read table headers and column headers
        for (int i = 0; i < header.NumTables; ++i) {
            TableHeader tableHeader;
            ifs.read(reinterpret_cast<char*>(&tableHeader), sizeof(TableHeader));

            for (int j = 0; j < tableHeader.NumColumns; ++j) {
                ColumnHeader columnHeader;
                ifs.read(reinterpret_cast<char*>(&columnHeader), sizeof(ColumnHeader));
                columnHeaders.push_back(columnHeader);
            }
        }

        // Read link keys section (if present)
        if (header.LinkKeysSectionOffset > 0) {
            ifs.seekg(header.LinkKeysSectionOffset);
            uint32_t numLinkKeys;
            ifs.read(reinterpret_cast<char*>(&numLinkKeys), sizeof(uint32_t));

            for (uint32_t i = 0; i < numLinkKeys; ++i) {
                uint32_t tableIndex, columnIndex;
                ifs.read(reinterpret_cast<char*>(&tableIndex), sizeof(uint32_t));
                ifs.read(reinterpret_cast<char*>(&columnIndex), sizeof(uint32_t));
                // Process link keys as needed for your specific application
            }
        }

        // Read data (assuming it's stored as plain text)
        // You may need to adjust this section based on the actual data storage format
        std::string line;
        while (std::getline(ifs, line)) {
            std::istringstream iss(line);
            std::vector<std::string> row;
            std::string cell;

            for (int i = 0; i < header.NumColumns; ++i) {
                if (std::getline(iss, cell, ',')) {
                    row.push_back(cell);
                }
            }

            if (!row.empty()) {
                rows.push_back(row);
            }
        }

        ifs.close();
    }
    

    std::string select(const KimTable& table, size_t rowIndex, size_t columnIndex) {
      if (rowIndex < table.rows.size() && columnIndex < table.columnHeaders.size()) {
        return table.rows[rowIndex][columnIndex];
      } else {
        std::cerr << "Invalid row or column index" << std::endl;
        return "";
      }
    }

    std::vector<std::string> selectRow(const KimTable& table, size_t rowIndex) {
        if (rowIndex < table.rows.size()) {
            return table.rows[rowIndex];
        } else {
            std::cerr << "Invalid row index" << std::endl;
            return std::vector<std::string>();
        }
    }

    void deleteRow(size_t rowIndex) {
        if (rowIndex < rows.size()) {
            rows.erase(rows.begin() + rowIndex);
        }
    }
    void updateRow(KimTable& table, size_t rowIndex, size_t columnIndex, const std::string& newValue) {
     if (rowIndex < table.rows.size() && columnIndex < table.columnHeaders.size()) {
        table.rows[rowIndex][columnIndex] = newValue;
     } else {
        std::cerr << "Invalid row or column index" << std::endl;
     }
    }
    

    //================================================SQL==========================================================================/
    std::vector<std::string> selectRowWithSQL(const KimTable& table, const std::string& sqlQuery) {
        std::regex selectRowRegex(R"(SELECT\s+\*\s+FROM\s+(\w+)\s+WHERE\s+(\w+)\s*=\s*(['"]?)(.+)\3)", std::regex::icase);

        std::smatch match;
        if (std::regex_search(sqlQuery, match, selectRowRegex)) {
            std::string tableName = match[1];
            std::string columnName = match[2];
            std::string value = match[4];

            if (tableName != table.header.TableName) {
                std::cerr << "Table name does not match" << std::endl;
                return std::vector<std::string>();
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

};
 
void WriteHeader(std::ofstream& ofs, const KimFileHeaderV2& header) {
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(KimFileHeaderV2));
}

void WriteColumnHeader(std::ofstream& ofs, const ColumnHeader& header) {
    ofs.write(header.ColumnName, sizeof(header.ColumnName));
    ofs.write(reinterpret_cast<const char*>(&header.DataType), sizeof(header.DataType));
    ofs.write(reinterpret_cast<const char*>(&header.DataSize), sizeof(header.DataSize));
    ofs.write(reinterpret_cast<const char*>(&header.IsIndexed), sizeof(header.IsIndexed));
    ofs.write(reinterpret_cast<const char*>(&header.IsLinkKey), sizeof(header.IsLinkKey));
    ofs.write(reinterpret_cast<const char*>(&header.IsIdentifier), sizeof(header.IsIdentifier));
}

void WriteTableHeader(std::ofstream& ofs, const TableHeader& header) {
    ofs.write(header.TableName, sizeof(header.TableName));
    ofs.write(reinterpret_cast<const char*>(&header.NumColumns), sizeof(header.NumColumns));
    ofs.write(reinterpret_cast<const char*>(&header.LinkColumnIndex), sizeof(header.LinkColumnIndex));
    ofs.write(reinterpret_cast<const char*>(&header.IdentifierColumnIndex), sizeof(header.IdentifierColumnIndex));
    ofs.write(reinterpret_cast<const char*>(&header.HasUniqueRows), sizeof(header.HasUniqueRows));
}
