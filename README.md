# Vince's CSV Parser
[![Build Status](https://travis-ci.org/vincentlaucsb/csv-parser.svg?branch=master)](https://travis-ci.org/vincentlaucsb/csv-parser)
[![codecov](https://codecov.io/gh/vincentlaucsb/csv-parser/branch/master/graph/badge.svg)](https://codecov.io/gh/vincentlaucsb/csv-parser)

## Motivation
There's plenty of other CSV parsers in the wild, but I had a hard time finding what I wanted. Specifically, I wanted something which had an interface similar to Python's `csv` module, but--obviously since we're using C++--a lot faster. Furthermore, I wanted support for special use cases such as calculating statistics on very large files. Thus, this library was created with these following goals in mind:

### Performance
This CSV parser uses multiple threads to simulatenously pull data from disk and parse it. Furthermore, it is capable of incremental streaming (parsing larger than RAM files), and quickly parsing data types.

### RFC 4180 Compliance
This CSV parser is much more than a fancy string splitter, and follows every guideline from [RFC 4180](https://www.rfc-editor.org/rfc/rfc4180.txt).

### Easy to Use and Well-Documented
https://vincentlaucsb.github.io/csv-parser

In additon to being easy on your computer's hardware, this library is also easy on you--the developer. Some helpful features include:
 * Decent ability to guess the dialect of a file (CSV, tab-delimited, etc.)
 * Ability to handle common deviations from the CSV standard, such as inconsistent row lengths, and leading comments
 * Ability to manually set the delimiter and quoting character of the parser

### Well Tested

## Building
All of this library's essentials are located under `src/`. This is a C++11 library developed using Microsoft Visual Studio and compatible with g++ and clang. The CMakeList and Makefile contain instructions for building the main library, some sample programs, and the test suite.

**GCC/Clang Compiler Flags**: `-O3 -pthread -std=c++11 -Wall`

## Features & Examples
### Reading a Large File
With this library, you can easily stream over a large file without reading its entirety into memory.

```
# include "csv_parser.h"

using namespace csv;

...

CSVReader reader("very_big_file.csv");
std::vector<std::string> row;

while (reader.read_row(row)) {
    // Do stuff with row here
}

```

You can also reorder a CSV or only keep a subset of the data simply by passing
in a vector of column indices.

```
# include "csv_parser.h"

using namespace csv;

...

std::vector<size_t> new_order = { 0, 2, 3, 5 };
CSVReader reader("very_big_file.csv", new_order);
std::vector<std::string> row;

while (reader.read_row(row)) {
    // Do stuff with row here
}

```

### Automatic Type Conversions
If your CSV has lots of numeric values, you can also have this parser automatically convert them to the proper data type.

```
# include "csv_parser.h"

using namespace csv;

...

CSVReader reader("very_big_file.csv");
std::vector<CSVField> row;

while (reader.read_row(row)) {
    if (row[0].is_number()) { // is_int() and is_float() are also available
        // Do something with data by calling
        // row[0].get_int(), row[0].get_float(), row[0].get_number<long int>(), ..., etc.
    }
    
    // get_string() can be called on any data types
    std::cout << row[0].get_string() << std::endl;
}

```

### Specifying a Specific Delimiter, Quoting Character, etc.
As you may have noticed in the examples above, it seems like I simply assumed the parser would automatically know how each CSV file was formatted. That's partially true! By default, all of the classes and functions (unless otherwise specified) will try to guess the features of a file. For example, this means that CSVReader will try to guess whether the delimiter is a comma, tab, pipe, etc. The guesser is also capable of differentiating leading comments in a file from the actual data.

This largely allows code using this parser to focus on what operations should on the data itself, rather than worrying about what format it is in. However, in rare circumstances the guess isn't perfect, and you might need to give the parser some guidance.

```
# include "csv_parser.h"
# include ...

using namespace csv;

CSVFormat format = {
    '\t',    // Delimiter
    '~',     // Quote-character
    '2',     // Line number of header
    {}       // Column names -- if empty, then filled by reading header row
};

CSVReader reader("wierd_csv_dialect.csv", {}, format);
vector<CSVField> row;

while (reader.read_row(row)) {
    // Do stuff with rows here
}

```

### Parsing an In-Memory String
This library has lots of short-hand functions so you can do common tasks without writing a lot of code, and parsing a CSV string is one of them.

```
# include "csv_parser.h"
# include ...

using namespace csv;

std::string csv_string = "Actor,Character"
    "Will Ferrell,Ricky Bobby\r\n"
    "John C. Reilly,Cal Naughton Jr.\r\n"
    "Sacha Baron Cohen,Jean Giard\r\n"

CSVReader csv_rows = parse(csv_string);
vector<string> row;
while (csv_rows.read(row)) {
    // Do stuff with row here
}
```

### CSV Metadata
Sometimes we don't really care for the CSV data--we just want details about the file itself. 

```
# include "csv_parser.h"
# include ...

using namespace csv;

std::vector<std::string> column_names = get_col_names("worldcitiespop.txt");

// Returns -1 if the column can't be found (case-sensitive search)
// Zero-indexed
int column_index = get_col_pos("worldcitiespop.txt", "Country");

// Note: Quote character guessing isn't implemented yet--it's always
// assumed to be "
CSVFormat format = guess_format("worldcitiespop.txt");

std::cout << "This is a CSV file delimited by " << format.delim
    << " with its header on row " << format.header
    << " and its second column named " << format.col_names.at(2)
    << std::endl;
    
// You can also get the same meta-data while parsing a CSV
CSVReader reader("very_big_file.csv");
std::vector<CSVField> row;

std::vector<std::string> col_names = reader.get_col_names();
CSVFormat big_file = reader.get_format();

while (reader.read_row(row)) {
    ...
}

```

## Contributing
Bug reports, feature requests, and so on are always welcome. Feel free to leave a note in the Issues section.
