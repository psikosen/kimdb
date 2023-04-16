#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

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


void WriteHeader(std::ofstream& ofs, const KimFileHeader& header) {
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(KimFileHeader));
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

void Select(const std::string& fileName, const std::string& tableName, const std::vector<std::string>& columns, const std::string& whereClause) {
    // Read table header to get column information
    TableHeader header;
    std::ifstream ifs(fileName, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    while (strcmp(header.TableName, tableName.c_str()) != 0) {
        ifs.seekg(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
        if (ifs.eof()) {
            std::cerr << "Table not found." << std::endl;
            return;
        }
    }

    // Parse where clause to get column index and comparison value
    int whereColIndex = -1;
    std::string whereValue;
    if (whereClause != "") {
        std::vector<std::string> tokens = SplitString(whereClause, '=');
        if (tokens.size() != 2) {
            std::cerr << "Invalid SQL query." << std::endl;
            return;
        }
        whereColIndex = GetColumnIndex(header, tokens[0]);
        if (whereColIndex == -1) {
            std::cerr << "Invalid column name." << std::endl;
            return;
        }
        whereValue = tokens[1];
    }

    // Get selected column indices
    std::vector<int> selectedColumns = GetSelectedColumns(header, columns);

    // Print selected column headers
    for (int i = 0; i < selectedColumns.size(); i++) {
        std::cout << std::setw(20) << header.Columns[selectedColumns[i]].ColumnName;
    }
    std::cout << std::endl;

    // Loop through rows and print selected columns
    for (int i = 0; i < header.NumRows; i++) {
        std::vector<std::pair<const char*, const void*>> row = GetRow(ifs, header.NumColumns);
        if (whereColIndex == -1 || CompareRow(row[whereColIndex].second, whereValue)) {
            for (int j = 0; j < selectedColumns.size(); j++) {
                PrintCellValue(header.Columns[selectedColumns[j]], row[selectedColumns[j]].second);
            }
            std::cout << std::endl;
        }
    }
    ifs.close();
}

void CreateTable(std::ofstream& ofs, const char* tableName, std::vector<std::pair<const char*, uint8_t>> columns) {

    TableHeader tableHeader;
    strcpy(tableHeader.TableName, tableName);
    tableHeader.NumColumns = columns.size();
    tableHeader.LinkColumnIndex = 0;
    tableHeader.IdentifierColumnIndex = 0;
    tableHeader.HasUniqueRows = true;

    ofs.write(reinterpret_cast<char*>(&tableHeader), sizeof(TableHeader));

    uint32_t columnHeadersOffset = ofs.tellp();
    for (auto column : columns) {
        ColumnHeader columnHeader;
        strcpy(columnHeader.ColumnName, column.first);
        columnHeader.DataType = column.second;
        columnHeader.DataSize = 0;
        columnHeader.IsIndexed = false;
        columnHeader.IsLinkKey = false;
        columnHeader.IsIdentifier = false;

        ofs.write(reinterpret_cast<char*>(&columnHeader), sizeof(ColumnHeader));
    }

    ofs.seekp(columnHeadersOffset - sizeof(KimFileHeader));
    KimFileHeader header;
    ofs.read(reinterpret_cast<char*>(&header), sizeof(KimFileHeader));

    uint32_t tableSize = ofs.tellp() - columnHeadersOffset;
    ofs.seekp(columnHeadersOffset - sizeof(KimFileHeader));
    header.LinkKeysSectionOffset += tableSize;
    ofs.write(reinterpret_cast<char*>(&header), sizeof(KimFileHeader));
}

void DeleteKimTable(const std::string& tableName) {
    // Construct the file name from the table name
    std::string fileName = tableName + ".kim";

    // Delete the file
    int result = std::remove(fileName.c_str());
    if (result != 0) {
        std::cerr << "Error deleting file: " << fileName << std::endl;
    } else {
        std::cout << "File deleted: " << fileName << std::endl;
    }
}

void AddRow(const std::string& fileName, const std::string& tableName, const std::vector<std::pair<std::string, std::variant<int, double, std::string>>>& rowData) {
    // Read table header to get column information
    TableHeader header;
    std::ifstream ifs(fileName, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    while (strcmp(header.TableName, tableName.c_str()) != 0) {
        ifs.seekg(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    }

    // Read existing row data
    std::vector<std::vector<std::byte>> rows;
    rows.reserve(header.NumRows);
    for (uint32_t i = 0; i < header.NumRows; ++i) {
        std::vector<std::byte> row(header.NumColumns * sizeof(std::variant<int, double, std::string>));
        ifs.read(reinterpret_cast<char*>(row.data()), header.NumColumns * sizeof(std::variant<int, double, std::string>));
        rows.emplace_back(std::move(row));
    }

    // Add new row data to the vector
    std::vector<std::byte> newRowData(header.NumColumns * sizeof(std::variant<int, double, std::string>));
    size_t index = 0;
    for (const auto& [columnName, columnValue] : rowData) {
        const auto& column = header.Columns[index++];
        if (std::holds_alternative<int>(columnValue)) {
            std::memcpy(newRowData.data() + column.Offset, &std::get<int>(columnValue), sizeof(int));
        } else if (std::holds_alternative<double>(columnValue)) {
            std::memcpy(newRowData.data() + column.Offset, &std::get<double>(columnValue), sizeof(double));
        } else if (std::holds_alternative<std::string>(columnValue)) {
            const auto& str = std::get<std::string>(columnValue);
            std::memcpy(newRowData.data() + column.Offset, str.c_str(), str.size());
        }
    }
    rows.emplace_back(std::move(newRowData));

    // Update table header and write new row data to the file
    header.NumRows = static_cast<uint32_t>(rows.size());
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    std::ofstream ofs(fileName, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(TableHeader));
    for (const auto& row : rows) {
        ofs.write(reinterpret_cast<const char*>(row.data()), header.NumColumns * sizeof(std::variant<int, double, std::string>));
    }
    ofs.close();
}
 
void InnerJoin(const char* fileName, const char* tableName1, const char* tableName2, const char* colName1, const char* colName2, const char* selectColumn, const char* whereColumn, const char* whereValue) {
    // Get column indexes
    uint32_t colIndex1 = GetColumnIndex(fileName, tableName1, colName1);
    uint32_t colIndex2 = GetColumnIndex(fileName, tableName2, colName2);

    if (colIndex1 == -1 || colIndex2 == -1) {
        std::cerr << "Invalid column name." << std::endl;
        return;
    }

    // Open file for reading
    std::ifstream ifs(fileName, std::ios::binary);

    // Read table headers to get column information
    TableHeader header1, header2;
    ifs.read(reinterpret_cast<char*>(&header1), sizeof(TableHeader));
    ifs.read(reinterpret_cast<char*>(&header2), sizeof(TableHeader));

    while (strcmp(header1.TableName, tableName1) != 0 || strcmp(header2.TableName, tableName2) != 0) {
        ifs.seekg(header1.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header1), sizeof(TableHeader));

        ifs.seekg(header2.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header2), sizeof(TableHeader));
    }

    // Check if join columns are the same data type
    if (header1.NumColumns <= colIndex1 || header2.NumColumns <= colIndex2) {
        std::cerr << "Invalid column index." << std::endl;
        return;
    }
    if (header1.NumColumns <= colIndex1 || header2.NumColumns <= colIndex2) {
        std::cerr << "Invalid column index." << std::endl;
        return;
    }

    if (header1.PrimaryKeyCol == colIndex1 || header2.PrimaryKeyCol == colIndex2) {
        std::cerr << "Can't join on primary key." << std::endl;
        return;
    }

    if (header1.LinkKeyCol != colIndex1 || header2.LinkKeyCol != colIndex2) {
        std::cerr << "Join columns must be link keys." << std::endl;
        return;
    }

    if (header1.NumRows == 0 || header2.NumRows == 0) {
        std::cerr << "One of the tables is empty." << std::endl;
        return;
    }

    if (header1.NumRows > UINT32_MAX / header1.NumColumns ||
        header2.NumRows > UINT32_MAX / header2.NumColumns) {
        std::cerr << "Table size is too large." << std::endl;
        return;
    }

    // Find matching link keys
    std::vector<std::pair<uint32_t, uint32_t>> matchingLinkKeys;
    matchingLinkKeys.reserve(std::min(header1.NumRows, header2.NumRows));

    ifs.seekg(header1.LinkKeysSectionOffset, std::ios::beg);
    std::vector<uint32_t> linkKeys1(header1.NumRows);
    ifs.read(reinterpret_cast<char*>(linkKeys1.data()), header1.NumRows * sizeof(uint32_t));

    ifs.seekg(header2.LinkKeysSectionOffset, std::ios::beg);
    std::vector<uint32_t> linkKeys2(header2.NumRows);
    ifs.read(reinterpret_cast<char*>(linkKeys2.data()), header2.NumRows * sizeof(uint32_t));
    // Perform inner join
    std::vector<std::vector<std::pair<const char*, const void*>>> joinedRows;
    for (const auto& row1 : rows1) {
    for (const auto& row2 : rows2) {
        if (memcmp(row1[linkKeyCol1].second, row2[linkKeyCol2].second, columnHeaders1[linkKeyCol1].DataSize) == 0) {
            std::vector<std::pair<const char*, const void*>> joinedRow;
            joinedRow.reserve(row1.size() + row2.size());
            joinedRow.insert(joinedRow.end(), row1.begin(), row1.end());
            joinedRow.insert(joinedRow.end(), row2.begin(), row2.end());
            joinedRows.push_back(std::move(joinedRow));
        }
    }
  }


// Print the joined rows
for (const auto& joinedRow : joinedRows) {
    for (const auto& column : joinedRow) {
        std::cout << column.first << ": ";
        switch (columnHeaders1[getColumnIndex(column.first, columnHeaders1)].DataType) {
            case ColumnDataType::INT:
                std::cout << *reinterpret_cast<const int*>(column.second);
                break;
            case ColumnDataType::FLOAT:
                std::cout << *reinterpret_cast<const float*>(column.second);
                break;
            case ColumnDataType::STRING:
                std::cout << reinterpret_cast<const char*>(column.second);
                break;
        }
        std::cout << ", ";
    }
    std::cout << std::endl;
}
}

void DeleteRow(const std::string& fileName, const std::string& tableName, const std::string& whereColumnName, const std::string& whereColumnValue) {
    // Read table header to get column information
    TableHeader header;
    std::ifstream ifs(fileName, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    while (strcmp(header.TableName, tableName.c_str()) != 0) {
        ifs.seekg(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
        if (ifs.eof()) {
            std::cerr << "Table not found." << std::endl;
            return;
        }
    }

    // Find where column
    int whereColIndex = -1;
    for (int i = 0; i < header.NumColumns; i++) {
        ColumnHeader colHeader;
        ifs.read(reinterpret_cast<char*>(&colHeader), sizeof(ColumnHeader));
        if (strcmp(colHeader.ColumnName, whereColumnName.c_str()) == 0) {
            whereColIndex = i;
            break;
        }
    }
    if (whereColIndex == -1) {
        std::cerr << "Invalid where clause." << std::endl;
        return;
    }

    // Check if where column is indexed
    bool isWhereColIndexed = false;
    std::vector<uint32_t> index;
    if (header.NumRows >= MIN_INDEXED_ROWS) {
        ColumnHeader whereColHeader;
        ifs.seekg(sizeof(TableHeader) + whereColIndex * sizeof(ColumnHeader), std::ios::beg);
        ifs.read(reinterpret_cast<char*>(&whereColHeader), sizeof(ColumnHeader));
        isWhereColIndexed = whereColHeader.IsIndexed;
        if (isWhereColIndexed) {
            index.resize(header.NumRows);
            ifs.read(reinterpret_cast<char*>(index.data()), header.NumRows * sizeof(uint32_t));
        }
    }

    // Delete rows
    std::ofstream ofs(fileName, std::ios::in | std::ios::out | std::ios::binary);
    ofs.seekp(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader));
    int numDeletedRows = 0;
    for (int i = 0; i < header.NumRows; i++) {
        // Read row
        std::vector<std::pair<const char*, std::unique_ptr<char[]>>> row;
        for (int j = 0; j < header.NumColumns; j++) {
            ColumnHeader colHeader;
            ifs.read(reinterpret_cast<char*>(&colHeader), sizeof(ColumnHeader));
            std::unique_ptr<char[]> data(new char[colHeader.DataSize]);
            ifs.read(data.get(), colHeader.DataSize);
            row.push_back({colHeader.ColumnName, std::move(data)});
        }

        // Check where clause
        bool deleteRow = false;
        if (isWhereColIndexed) {
            uint32_t rowIndex = index[i];
            ifs.seekg(sizeof(TableHeader) + whereColIndex * sizeof(ColumnHeader) + rowIndex * sizeof(uint32_t), std::ios::beg);
            uint32_t valueIndex;
            ifs.read(reinterpret_cast<char*>(&valueIndex), sizeof(uint32_t));
            const char* value = row[whereColIndex].second.get();
            deleteRow = (strcmp(value + valueIndex, whereColumnValue.c_str()) == 0);
        } else {
            const char* value = row[whereColIndex].second.get();
            deleteRow = (strcmp(value, whereColumnValue.c_str()) == 0
          if (deleteRow) {
           numDeletedRows++;
         } else {
         // Write row back to file
          for (auto& col : row) {
            ColumnHeader colHeader;
            ifs.read(reinterpret_cast<char*>(&colHeader), sizeof(ColumnHeader));
            ofs.write(reinterpret_cast<const char*>(&colHeader), sizeof(ColumnHeader));
            ofs.write(col.second.get(), colHeader.DataSize);
          }
         }
         }// Update number of rows
        header.NumRows -= numDeletedRows;
        ofs.seekp(sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint16_t));
        ofs.write(reinterpret_cast<const char*>(&header.NumRows), sizeof(uint32_t));

        ofs.close();
    }
}
 
void CreateTableWithSQL(std::ofstream& ofs, const std::string& sqlQuery) {
    // Parse SQL query to get table name and columns
    std::regex rgx("\\((.*?)\\)");
    std::sregex_iterator iter(sqlQuery.begin(), sqlQuery.end(), rgx);
    std::sregex_iterator end;
    if (iter == end) {
        std::cerr << "Invalid SQL query." << std::endl;
        return;
    }
    std::string columnsStr = iter->str(1);
    std::vector<std::string> columns;
    std::stringstream ss(columnsStr);
    std::string column;
    while (std::getline(ss, column, ',')) {
        column = std::regex_replace(column, std::regex("(^ +| +$)"), "");
        columns.push_back(column);
    }
    std::string tableName = std::regex_replace(sqlQuery.substr(0, sqlQuery.find("(")), std::regex("(^ +| +$)"), "");

    // Write table header
    TableHeader header;
    strcpy(header.TableName, tableName.c_str());
    header.NumColumns = columns.size();
    header.LinkColumnIndex = 0;
    header.IdentifierColumnIndex = 0;
    header.HasUniqueRows = false;
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(TableHeader));

    // Write column headers
    for (const auto& column : columns) {
        std::vector<std::string> tokens;
        std::stringstream ss(column);
        std::string token;
        while (std::getline(ss, token, ' ')) {
            token = std::regex_replace(token, std::regex("(^ +| +$)"), "");
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        if (tokens.size() < 2) {
            std::cerr << "Invalid column definition: " << column << std::endl;
            return;
        }
        std::string columnName = tokens[0];
        std::string columnType = tokens[1];
        uint8_t dataType = 0;
        if (columnType == "integer") {
            dataType = 0;
        } else if (columnType == "text") {
            dataType = 1;
        } else if (columnType == "float") {
            dataType = 2;
        } else {
            std::cerr << "Invalid column type: " << columnType << std::endl;
            return;
        }
        uint16_t dataSize = 0;
        bool isIndexed = false;
        bool isLinkKey = false;
        bool isIdentifier = false;
        for (size_t i = 2; i < tokens.size(); i++) {
            std::string option = tokens[i];
            if (option == "primary_key") {
                isIdentifier = true;
            } else if (option == "unique") {
                header.HasUniqueRows = true;
            } else if (option == "indexed") {
                isIndexed = true;
            } else if (option == "link_key") {
                isLinkKey = true;
                header.LinkColumnIndex = static_cast<uint16_t>(ofs.tellp());
            }
        }
        ColumnHeader colHeader;
        strcpy(colHeader.ColumnName, columnName.c_str());
        colHeader.DataType = dataType;
        colHeader.DataSize = dataSize;
        colHeader.IsIndexed = isIndexed;
        colHeader.IsLinkKey = isLinkKey;
        colHeader.IsIdentifier = isIdentifier;
        ofs.write(reinterpret_cast<const char*>(&colHeader), sizeof(Column
    colHeader.IsLinkKey = isLinkKey;
    ofs.write(reinterpret_cast<const char*>(&colHeader), sizeof(ColumnHeader));

    // Write index data if column is indexed
    if (colHeader.IsIndexed) {
        std::vector<uint32_t> index(rowData.size());
        std::iota(index.begin(), index.end(), 0);
        std::sort(index.begin(), index.end(),
                  [&](uint32_t i1, uint32_t i2) { return strcmp(rowData[i1].second.get(), rowData[i2].second.get()) < 0; });
        ofs.write(reinterpret_cast<const char*>(index.data()), index.size() * sizeof(uint32_t));
    }

    // Write data
    for (auto& data : rowData) {
        ofs.write(data.second.get(), header2.NumRows * data.first.DataSize);
    }

    // Update row count in table header
    header2.NumRows++;
    ofs.seekp(sizeof(KimFileHeader));
    ofs.write(reinterpret_cast<const char*>(&header2), sizeof(TableHeader));
    ofs.close();

    std::cout << "Row added successfully." << std::endl;
    } else {
        std::cerr << "Table not found." << std::endl;
    }
}


void InsertWithSQL(const std::string& fileName, const std::string& sql) {
    // Parse SQL statement to get table name and values
    std::smatch match;
    std::regex_search(sql, match, std::regex(R"(INSERT INTO (\w+) \(([\w\s,]+)\) VALUES \(([\w\s,']+)\))"));
    if (match.empty()) {
        std::cerr << "Invalid SQL query." << std::endl;
        return;
    }
    std::string tableName = match[1];
    std::vector<std::string> colNames;
    std::string colNamesStr = match[2];
    std::istringstream iss(colNamesStr);
    std::string colName;
    while (std::getline(iss, colName, ',')) {
        colNames.push_back(trim(colName));
    }
    std::vector<std::string> values;
    std::string valuesStr = match[3];
    iss = std::istringstream(valuesStr);
    std::string value;
    while (std::getline(iss, value, ',')) {
        values.push_back(trim(value));
    }

    // Get column information from table header
    TableHeader header;
    std::ifstream ifs(fileName, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    while (strcmp(header.TableName, tableName.c_str()) != 0) {
        ifs.seekg(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
        if (ifs.eof()) {
            std::cerr << "Table not found." << std::endl;
            return;
        }
    }
    std::vector<ColumnHeader> colHeaders;
    colHeaders.reserve(header.NumColumns);
    for (int i = 0; i < header.NumColumns; i++) {
        ColumnHeader colHeader;
        ifs.read(reinterpret_cast<char*>(&colHeader), sizeof(ColumnHeader));
        colHeaders.push_back(colHeader);
    }

    // Create row buffer
    std::vector<std::pair<const char*, std::unique_ptr<char[]>>> row;
    row.reserve(header.NumColumns);
    for (int i = 0; i < header.NumColumns; i++) {
        const char* colName = colHeaders[i].ColumnName;
        auto it = std::find(colNames.begin(), colNames.end(), colName);
        if (it != colNames.end()) {
            int index = std::distance(colNames.begin(), it);
            const char* valueStr = values[index].c_str();
            int dataType = colHeaders[i].DataType;
            int dataSize = colHeaders[i].DataSize;
            std::unique_ptr<char[]> data(new char[dataSize]);
            if (dataType == DataType::Int) {
                int value = std::stoi(valueStr);
                memcpy(data.get(), &value, sizeof(int));
            } else if (dataType == DataType::Float) {
                float value = std::stof(valueStr);
                memcpy(data.get(), &value, sizeof(float));
            } else if (dataType == DataType::Double) {
                double value = std::stod(valueStr);
                memcpy(data.get(), &value, sizeof(double));
            } else if (dataType == DataType::String) {
                strncpy(data.get(), valueStr, dataSize);
            }
            row.push_back({colName, std::move(data)});
        } else {
            // Use default value for column
            std::unique_ptr<char[]> data(new char[col
char* colData = new char[colHeader.DataSize];
memcpy(colData, defaultValue, colHeader.DataSize);
row.push_back(std::make_pair(colHeader.ColumnName, colData));
} else {
// Prompt user for column value
std::cout << "Enter value for column " << colHeader.ColumnName << ": ";
std::string value;
std::getline(std::cin, value);
if (value.empty()) {
std::cerr << "Invalid value for column " << colHeader.ColumnName << std::endl;
return;
}
// Check data type and convert value to binary format
switch (colHeader.DataType) {
case DataType::INT32:
try {
int32_t intValue = std::stoi(value);
memcpy(colData, &intValue, sizeof(int32_t));
} catch (std::exception& e) {
std::cerr << "Invalid value for column " << colHeader.ColumnName << std::endl;
return;
}
break;
case DataType::FLOAT:
try {
float floatValue = std::stof(value);
memcpy(colData, &floatValue, sizeof(float));
} catch (std::exception& e) {
std::cerr << "Invalid value for column " << colHeader.ColumnName << std::endl;
return;
}
break;
case DataType::CHAR:
if (value.length() > colHeader.DataSize) {
std::cerr << "Value for column " << colHeader.ColumnName << " is too long." << std::endl;
return;
}
memset(colData, 0, colHeader.DataSize);
memcpy(colData, value.c_str(), value.length());
break;
default:
std::cerr << "Unsupported data type." << std::endl;
return;
}
row.push_back(std::make_pair(colHeader.ColumnName, colData));
}
}
// Write row to file
std::ofstream ofs(fileName, std::ios::binary | std::ios::app);
for (int i = 0; i < row.size(); i++) {
    ofs.write(row[i].second, colHeaders[i].DataSize);
}

ofs.close();
std::cout << "Row added." << std::endl;

}
 
void DeleteRowSQL(const std::string& fileName, const std::string& tableName, const std::string& whereClause) {
    // Parse where clause
    std::string whereColumnName, whereOperator, whereColumnValue;
    std::stringstream ss(whereClause);
    ss >> whereColumnName >> whereOperator >> whereColumnValue;

    // Read table header to get column information
    TableHeader header;
    std::ifstream ifs(fileName, std::ios::binary);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    while (strcmp(header.TableName, tableName.c_str()) != 0) {
        ifs.seekg(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
        if (ifs.eof()) {
            std::cerr << "Table not found." << std::endl;
            return;
        }
    }

    // Find where column
    int whereColIndex = -1;
    for (int i = 0; i < header.NumColumns; i++) {
        ColumnHeader colHeader;
        ifs.read(reinterpret_cast<char*>(&colHeader), sizeof(ColumnHeader));
        if (strcmp(colHeader.ColumnName, whereColumnName.c_str()) == 0) {
            whereColIndex = i;
            break;
        }
    }
    if (whereColIndex == -1) {
        std::cerr << "Invalid where clause." << std::endl;
        return;
    }

    // Check if where column is indexed
    bool isWhereColIndexed = false;
    std::vector<uint32_t> index;
    if (header.NumRows >= MIN_INDEXED_ROWS) {
        ColumnHeader whereColHeader;
        ifs.seekg(sizeof(TableHeader) + whereColIndex * sizeof(ColumnHeader), std::ios::beg);
        ifs.read(reinterpret_cast<char*>(&whereColHeader), sizeof(ColumnHeader));
        isWhereColIndexed = whereColHeader.IsIndexed;
        if (isWhereColIndexed) {
            index.resize(header.NumRows);
            ifs.read(reinterpret_cast<char*>(index.data()), header.NumRows * sizeof(uint32_t));
        }
    }

    // Delete rows
    std::ofstream ofs(fileName, std::ios::in | std::ios::out | std::ios::binary);
    ofs.seekp(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader));
    int numDeletedRows = 0;
    for (int i = 0; i < header.NumRows; i++) {
        // Read row
        std::vector<std::pair<const char*, std::unique_ptr<char[]>>> row;
        for (int j = 0; j < header.NumColumns; j++) {
            ColumnHeader colHeader;
            ifs.read(reinterpret_cast<char*>(&colHeader), sizeof(ColumnHeader));
            std::unique_ptr<char[]> data(new char[colHeader.DataSize]);
            ifs.read(data.get(), colHeader.DataSize);
            row.push_back({colHeader.ColumnName, std::move(data)});
        }

        // Check where clause
        bool deleteRow = false;
        if (isWhereColIndexed) {
            uint32_t rowIndex = index[i];
            ifs.seekg(sizeof(TableHeader) + whereColIndex * sizeof(ColumnHeader) + rowIndex * sizeof(uint32_t), std::ios::beg);
            uint32_t valueIndex;
            ifs.read(reinterpret_cast<char*>(&valueIndex), sizeof(uint32_t));
            const char* value = row[whereColIndex].second.get();
          // Check where clause
bool deleteRow = false;
if (isWhereColIndexed) {
    uint32_t rowIndex = index[i];
    ifs.seekg(sizeof(TableHeader) + whereColIndex * sizeof(ColumnHeader) + rowIndex * sizeof(uint32_t), std::ios::beg);
    uint32_t valueIndex;
    ifs.read(reinterpret_cast<char*>(&valueIndex), sizeof(uint32_t));
    const char* value = row[whereColIndex].second.get();
    deleteRow = (strcmp(value + valueIndex, whereColumnValue.c_str()) == 0);
} else {
    const char* value = row[whereColIndex].second.get();
    deleteRow = (strcmp(value, whereColumnValue.c_str()) == 0);
}

   // Delete row if necessary
   if (deleteRow) {
    // Seek to beginning of row
    ofs.seekp(-(static_cast<int>(sizeof(ColumnHeader) * header.NumColumns) + rowSize), std::ios::cur);

    // Overwrite with last row in file
    std::vector<std::pair<const char*, std::unique_ptr<char[]>>> lastRow;
    ifs.seekg(-(static_cast<int>(sizeof(ColumnHeader) * header.NumColumns) + rowSize), std::ios::cur);
    for (int j = 0; j < header.NumColumns; j++) {
        ColumnHeader colHeader;
        ifs.read(reinterpret_cast<char*>(&colHeader), sizeof(ColumnHeader));
        std::unique_ptr<char[]> data(new char[colHeader.DataSize]);
        ifs.read(data.get(), colHeader.DataSize);
        lastRow.push_back({colHeader.ColumnName, std::move(data)});
    }
    ifs.seekg(-(static_cast<int>(sizeof(ColumnHeader) * header.NumColumns) + rowSize), std::ios::cur);
    for (int j = 0; j < header.NumColumns; j++) {
        ColumnHeader colHeader;
        ifs.read(reinterpret_cast<char*>(&colHeader), sizeof(ColumnHeader));
        const char* data = lastRow[j].second.get();
        ofs.write(reinterpret_cast<const char*>(&colHeader), sizeof(ColumnHeader));
        ofs.write(data, colHeader.DataSize);
    }

    // Truncate file
    ofs.seekp(-(static_cast<int>(sizeof(ColumnHeader) * header.NumColumns) + rowSize), std::ios::cur);
    ofs.truncate(ofs.tellp());
    numDeletedRows++;
    }
        rowSize = static_cast<uint32_t>(ifs.tellg()) - rowStartPos;
        }
        ifs.close();
        ofs.close();

        if (numDeletedRows > 0) {
            std::cout << "Deleted " << numDeletedRows << " row(s) from table " << tableName << "." << std::endl;
        } else {
            std::cout << "No rows deleted from table " << tableName << "." << std::endl;
        }
                // Update number of rows in table header
                ofs.seekp(sizeof(KimFileHeader) + sizeof(TableHeader));
                uint32_t numRows = header.NumRows - numDeletedRows;
                ofs.write(reinterpret_cast<char*>(&numRows), sizeof(uint32_t));

                ofs.close();
            } else {
                std::cerr << "Table not found." << std::endl;
            }
}
 
void UpdateRow(std::ofstream& ofs, const char* tableName, const std::vector<std::pair<const char*, const void*>>& row, const std::vector<std::pair<const char*, const void*>>& updateValues) {
    ofs.write("UPDATE ", 7);
    ofs.write(tableName, strlen(tableName));
    ofs.write(" SET ", 5);
    for (size_t i = 0; i < updateValues.size(); ++i) {
        ofs.write(updateValues[i].first, strlen(updateValues[i].first));
        ofs.write(" = ", 3);
        const void* value = updateValues[i].second;
        switch (value[0]) {
            case '\x00': // Integer value
                ofs.write(reinterpret_cast<const char*>(value + 1), value[1]);
                break;
            case '\x01': // Text value
                ofs.write("'", 1);
                ofs.write(reinterpret_cast<const char*>(value + 1), value[1]);
                ofs.write("'", 1);
                break;
            case '\x02': // Decimal value
                ofs.write(reinterpret_cast<const char*>(value + 1), value[1]);
                break;
        }
        if (i < updateValues.size() - 1) {
            ofs.write(",", 1);
        }
    }
    ofs.write(" WHERE ", 7);
    for (size_t i = 0; i < row.size(); ++i) {
        ofs.write(row[i].first, strlen(row[i].first));
        ofs.write(" = ", 3);
        const void* value = row[i].second;
        switch (value[0]) {
            case '\x00': // Integer value
                ofs.write(reinterpret_cast<const char*>(value + 1), value[1]);
                break;
            case '\x01': // Text value
                ofs.write("'", 1);
                ofs.write(reinterpret_cast<const char*>(value + 1), value[1]);
                ofs.write("'", 1);
                break;
            case '\x02': // Decimal value
                ofs.write(reinterpret_cast<const char*>(value + 1), value[1]);
                break;
        }
        if (i < row.size() - 1) {
            ofs.write(" AND ", 5);
        }
    }
    ofs.write(";\n", 2);
}

// Have to fix
void Select(std::ifstream& ifs, const char* tableName, const std::string& query, std::vector<std::vector<std::pair<const char*, const void*>>>& results) {
    // Read table header to get column information
    TableHeader header;
    ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    while (strcmp(header.TableName, tableName) != 0) {
        ifs.seekg(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    }

    // Parse SQL query
    std::vector<std::string> tokens;
    std::istringstream iss(query);
    std::string token;
    while (std::getline(iss, token, ' ')) {
        tokens.push_back(token);
    }

    // Validate SQL query
    if (tokens.size() < 4 || tokens[0] != "SELECT" || tokens[2] != "FROM" || tokens[3] != tableName) {
        std::cerr << "Invalid SQL query." << std::endl;
        return;
    }

    // Determine which columns to select
    std::vector<int> selectedColumns;
    if (tokens[1] == "*") {
        for (int i = 0; i < header.NumColumns; ++i) {
            selectedColumns.push_back(i);
        }
    } else {
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i] == "FROM") {
                break;
            }
            for (int j = 0; j < header.NumColumns; ++j) {
                if (tokens[i] == header.Columns[j].ColumnName) {
                    selectedColumns.push_back(j);
                    break;
                }
            }
        }
    }

    // Read rows and filter by WHERE clause
    results.clear();
    results.reserve(header.NumRows);
    for (int i = 0; i < header.NumRows; ++i) {
        std::vector<std::pair<const char*, const void*>> row;
        row.reserve(selectedColumns.size());
        bool includeRow = true;
        for (size_t j = 0; j < selectedColumns.size(); ++j) {
            int colIndex = selectedColumns[j];
            ColumnHeader& colHeader = header.Columns[colIndex];
            char* colData = new char[colHeader.ColumnSize];
            ifs.read(colData, colHeader.ColumnSize);
            switch (colHeader.ColumnType) {
                case Integer:
                    row.push_back({colHeader.ColumnName, reinterpret_cast<const void*>(colData)});
                    break;
                case Text:
                    row.push_back({colHeader.ColumnName, reinterpret_cast<const void*>(colData + 1)});
                    break;
                case Decimal:
                    row.push_back({colHeader.ColumnName, reinterpret_cast<const void*>(colData)});
                    break;
            }
            delete[] colData;
        }
        if (tokens.size() > 4 && tokens[4] == "WHERE") {
            int whereColIndex = -1;
            const char* whereValue = nullptr;
            for (size_t j = 5; j < tokens.size(); j += 3) {
                for (int k = 0; k < header.NumColumns; ++k) {
                    if (tokens[j] == header.Columns[k].ColumnName) {
                        whereColIndex = k;
                        break;
                    }
                }
                if (whereColIndex == -1) {
                    std::cerr << "Invalid SQL query." << std::endl;
                    return;
                    }
            if (tokens[j + 1] != "=") {
                std::cerr << "Invalid SQL query." << std::endl;
                return;
            }
            whereValue = tokens[j + 2].c_str();
            ColumnHeader& whereColHeader = header.Columns[whereColIndex];
            char* whereColData = new char[whereColHeader.ColumnSize];
            ifs.seekg(i * header.RowSize + whereColHeader.ColumnOffset, std::ios::beg);
            ifs.read(whereColData, whereColHeader.ColumnSize);
            bool match = false;
            switch (whereColHeader.ColumnType) {
                case Integer:
                    match = *reinterpret_cast<const int*>(whereColData) == std::stoi(whereValue);
                    break;
                case Text:
                    match = strcmp(whereColData + 1, whereValue) == 0;
                    break;
                case Decimal:
                    match = *reinterpret_cast<const double*>(whereColData) == std::stod(whereValue);
                    break;
            }
            delete[] whereColData;
            if (!match) {
                includeRow = false;
                break;
            }
        }
    }
    if (includeRow) {
        results.push_back(row);
    }
}
       


// Close output file stream
ofs.close();

}