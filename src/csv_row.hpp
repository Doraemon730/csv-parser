#pragma once
// Auxiliary data structures for CSV parser

#include "data_type.h"
#include <math.h>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map> // For ColNames
#include <memory> // For CSVField
#include <limits> // For CSVField

namespace csv {
    namespace internals {
        /** @struct ColNames
         *  @brief A data structure for handling column name information.
         *
         *  These are created by CSVReader and passed (via smart pointer)
         *  to CSVRow objects it creates, thus
         *  allowing for indexing by column name.
         */
        struct ColNames {
            ColNames(const std::vector<std::string>&);
            std::vector<std::string> col_names;
            std::unordered_map<std::string, size_t> col_pos;

            std::vector<std::string> get_col_names() const;
            size_t size() const;
        };
    }

    /**
    * @class CSVField
    * @brief Data type representing individual CSV values. 
    *        CSVFields can be obtained by using CSVRow::operator[]
    */
    class CSVField {
    public:
        CSVField(std::string_view _sv) : sv(_sv) { };
        long double value = 0;
        std::string_view sv;
        bool operator==(const std::string&) const;

        /** Returns the value casted to the requested type, performing type checking before.
        *  An std::runtime_error will be thrown if a type mismatch occurs, with the exception
        *  of T = std::string, in which the original string representation is always returned.
        *  Converting long ints to ints will be checked for overflow.
        *
        *  **Valid options for T**:
        *   - std::string or std::string_view
        *   - int
        *   - long
        *   - long long
        *   - double
        *   - long double
        */
        template<typename T=std::string_view> T get() {
            /** Get numeric values */
            if (internals::type_num<T>() >= CSV_INT) {
                if (is_num()) {
                    if (internals::type_num<T>() < this->type())
                        throw std::runtime_error("Overflow error.");

                    return static_cast<T>(this->value);
                }
                else
                    throw std::runtime_error("Not a number.");
            }
        }

        DataType type();
        bool is_null() { return type() == CSV_NULL; }
        bool is_str() { return type() == CSV_STRING; }
        bool is_num() { return type() >= CSV_INT; }
        bool is_int() {
            return (type() >= CSV_INT) && (type() <= CSV_LONG_LONG_INT);
        }
        bool is_float() { return type() == CSV_DOUBLE; };

    private:
        int _type = -1;
        void get_value();
    };

    /**
     * @class CSVRow 
     * @brief Data structure for representing CSV rows
     *
     * Internally, a CSVRow consists of:
     *  - A pointer to the original column names
     *  - A string containing the entire CSV row (row_str)
     *  - An array of positions in that string where individual fields begin (splits)
     *
     * CSVRow::operator[] uses splits to compute a string_view over row_str.
     *
     */
    class CSVRow {
    public:
        CSVRow() = default;
        CSVRow(std::string&& _str, std::vector<size_t>&& _splits,
            std::shared_ptr<internals::ColNames> _cnames = nullptr) :
            row_str(std::move(_str)),
            splits(std::move(_splits)),
            col_names(_cnames)
        {};

        bool empty() const { return this->row_str.empty(); }
        size_t size() const;

        /** @name Value Retrieval */
        ///@{
        CSVField operator[](size_t n) const;
        CSVField operator[](const std::string&) const;
        std::string_view get_string_view(size_t n) const;
        operator std::vector<std::string>() const;
        ///@}

    private:
        std::shared_ptr<internals::ColNames> col_names = nullptr;
        std::string row_str;
        std::vector<size_t> splits;
    };

    // get() specializations
    template<>
    inline std::string CSVField::get<std::string>() {
        return std::string(this->sv);
    }

    template<>
    inline std::string_view CSVField::get<std::string_view>() {
        return this->sv;
    }

    template<>
    inline long double CSVField::get<long double>() {
        if (!is_num())
            throw std::runtime_error("Not a number.");

        return this->value;
    }
}