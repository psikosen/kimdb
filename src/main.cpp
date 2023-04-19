//
// Created by ray on 4/19/2023.
//
#include "KimFileHead.h"
#include "main.h"

int main() {
    KimTable table;
    table.setTableName("example");

    // Create a new table with column names
    std::vector<std::string> columnNames = {"column1", "column2", "column3"};
    table.createTable(columnNames);

    // Add rows to the table
    table.addRow({"value1", "value2", "value3"});
    table.addRow({"value4", "value5", "value6"});
    std::cout << "writing to file" << " ";
    // Write the table to a .kim file
    table.writeToFile("example.kim");
    std::cout << "loading file" << " ";
    // Load the table from the .kim file
    KimTable loadedTable;
    loadedTable.loadFromFile("example.kim");
    std::cout << "reading file" << " ";
    // Select a row with an SQL query
 //   std::vector<std::string> row = loadedTable.selectRowWithSQL(loadedTable, "SELECT * FROM example WHERE column1 = 'value1'");

    // Print the row data
   // for (const auto& value : row) {
   //     std::cout << value << " ";
  //  }
  //  std::cout << std::endl;

    return 0;
}

