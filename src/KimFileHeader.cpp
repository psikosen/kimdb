#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

struct KimFileHeader {
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
    bool IsIdentifier;
};

struct TableHeader {
    char TableName[64];
    uint16_t NumColumns;
    uint16_t LinkColumnIndex;
    uint16_t IdentifierColumnIndex;
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

void CreateDemoKimFile(const std::string& filename) {
    KimFileHeader header;
    header.FileFormatVersion = 2;
    header.NumTables = 1;
    header.LinkKeysSectionOffset = 0;

    std::vector<TableHeader> tableHeaders(1);
    std::vector<std::vector<ColumnHeader>> columnHeaders(1);

    // Define table and column headers
    strcpy(tableHeaders[0].TableName, "my_table");
    tableHeaders[0].NumColumns = 3;
    tableHeaders[0].LinkColumnIndex = -1;
    tableHeaders[0].IdentifierColumnIndex = 0;
    tableHeaders[0].HasUniqueRows = true;

    columnHeaders[0].resize(3);
    strcpy(columnHeaders[0][0].ColumnName, "id");
    columnHeaders[0][0].DataType = 1;
    columnHeaders[0][0].DataSize = 4;
    columnHeaders[0][0].IsIndexed = true;
    columnHeaders[0][0].IsLinkKey = false;
    columnHeaders[0][0].IsIdentifier = true;

    strcpy(columnHeaders[0][1].ColumnName, "name");
    columnHeaders[0][1].DataType = 2;
    columnHeaders[0][1].DataSize = 64;
    columnHeaders[0][1].IsIndexed = false;
    columnHeaders[0][1].IsLinkKey = false;
    columnHeaders[0][1].IsIdentifier = false;

    strcpy(columnHeaders[0][2].ColumnName, "age");
    columnHeaders[0][2].DataType = 1;
    columnHeaders[0][2].DataSize = 4;
    columnHeaders[0][2].IsIndexed = true;
    columnHeaders[0][2].IsLinkKey = false;
    columnHeaders[0][2].IsIdentifier = false;

// Open output file stream and write header
std::ofstream ofs(filename, std::ios::binary);
if (!ofs) {
    std::cerr << "Error opening file " << filename << std::endl;
    return;
}
WriteHeader(ofs, header);

// Write table and column headers
for (size_t i = 0; i < tableHeaders.size(); ++i) {
    WriteTableHeader(ofs, tableHeaders[i]);
    for (size_t j = 0; j < columnHeaders[i].size(); ++j) {
        WriteColumnHeader(ofs, columnHeaders[i][j]);
    }
}


void CreateTable(std::ofstream& ofs, const char* tableName, const std::vector<std::pair<const char*, uint8_t>>& columns) {
    TableHeader header;
    strcpy(header.TableName, tableName);
    header.NumColumns = static_cast<uint16_t>(columns.size());
    header.LinkColumnIndex = -1;
    header.IdentifierColumnIndex = -1;
    header.HasUniqueRows = true;
    header.RowCount = 0;
    for (const auto& column : columns) {
        ColumnHeader columnHeader;
        strcpy(columnHeader.ColumnName, column.first);
        columnHeader.DataType = column.second;
        columnHeader.DataSize = 0;
        columnHeader.IsIndexed = false;
        columnHeader.IsLinkKey = false;
        columnHeader.IsIdentifier = false;
        header.Columns.push_back(columnHeader);
    }
    WriteTableHeader(ofs, header);
}

void AddRow(std::ofstream& ofs, const char* tableName, const std::vector<std::pair<const char*, const void*>>& row) {
    // Read table header to get column information
    TableHeader header;
    std::ifstream ifs("example.kim", std::ios::binary);
    ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    while (strcmp(header.TableName, tableName) != 0) {
        ifs.seekg(sizeof(TableHeader) + header.NumColumns * sizeof(ColumnHeader), std::ios::cur);
        ifs.read(reinterpret_cast<char*>(&header), sizeof(TableHeader));
    }

    // Calculate row size
    uint32_t rowSize = 0;
    for (const auto& column : header.Columns) {
        rowSize += column.DataSize;
    }

    // Write row to file
    ofs.write(reinterpret_cast<const char*>(&rowSize), sizeof(rowSize));

    // Add the primary key column if it exists
    if (header.PrimaryKeyColumnIndex != -1) {
        auto primaryKeyValue = GetColumnValue(row, header.Columns[header.PrimaryKeyColumnIndex]);
        ofs.write(reinterpret_cast<const char*>(primaryKeyValue), header.Columns[header.PrimaryKeyColumnIndex].DataSize);
    }

    // Add the link key column if it exists
    if (header.LinkKeyColumnIndex != -1) {
        auto linkKeyValue = GetColumnValue(row, header.Columns[header.LinkKeyColumnIndex]);
        ofs.write(reinterpret_cast<const char*>(linkKeyValue), header.Columns[header.LinkKeyColumnIndex].DataSize);
    }

    // Add the rest of the columns
    for (const auto& column : row) {
        for (const auto& tableColumn : header.Columns) {
            if (strcmp(tableColumn.ColumnName, column.first) == 0 &&
                strcmp(tableColumn.ColumnName, header.Columns[header.PrimaryKeyColumnIndex].ColumnName) != 0 &&
                strcmp(tableColumn.ColumnName, header.Columns[header.LinkKeyColumnIndex].ColumnName) != 0) {
                ofs.write(reinterpret_cast<const char*>(column.second), tableColumn.DataSize);
                break;
            }
        }
    }
}

//SQL Syntax
void CreateTableSQL(std::ofstream& ofs, const char* tableName, const std::vector<Column>& columns) {
    ofs.write("CREATE TABLE ", 13);
    ofs.write(tableName, strlen(tableName));
    ofs.write(" (", 2);
    for (size_t i = 0; i < columns.size(); ++i) {
        ofs.write(columns[i].name, strlen(columns[i].name));
        switch (columns[i].type) {
            case 0:
                ofs.write(" INTEGER", 8);
                break;
            case 1:
                ofs.write(" TEXT", 5);
                break;
            case 2:
                ofs.write(" DECIMAL", 8);
                break;
        }
        if (i < columns.size() - 1) {
            ofs.write(",", 1);
        }
    }
    ofs.write(");\n", 2);
}

void InsertRow(std::ofstream& ofs, const char* tableName, const std::vector<std::pair<const char*, const void*>>& row) {
    ofs.write("INSERT INTO ", 12);
    ofs.write(tableName, strlen(tableName));
    ofs.write(" (", 2);
    for (size_t i = 0; i < row.size(); ++i) {
        ofs.write(row[i].first, strlen(row[i].first));
        if (i < row.size() - 1) {
            ofs.write(",", 1);
        }
    }
    ofs.write(") VALUES (", 11);
    for (size_t i = 0; i < row.size(); ++i) {
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
            ofs.write(",", 1);
        }
    }
    ofs.write(");\n", 2);
}

void DeleteRow(std::ofstream& ofs, const char* tableName, const std::vector<std::pair<const char*, const void*>>& row) {
    ofs.write("DELETE FROM ", 13);
    ofs.write(tableName, strlen(tableName));
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


// Close output file stream
ofs.close();

