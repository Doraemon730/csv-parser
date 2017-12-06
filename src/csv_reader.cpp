#include "csv_parser.h"
#include <algorithm>
#include <random>

namespace csv_parser {
    /** @file */

    std::string guess_delim(std::string filename) {
        /** Guess the delimiter of a delimiter separated values file
         *  by scanning the first 100 lines
         *
         *  - "Winner" is based on which delimiter has the most number
         *    of correctly parsed rows + largest number of columns
         *  -  **Note:** Assumes that whatever the dialect, all records
         *     are newline separated
         */

        std::vector<std::string> delims = { ",", "|", "\t", ";", "^" };
        std::string current_delim;
        int max_rows = 0;
        int max_cols = 0;

        for (size_t i = 0; i < delims.size(); i++) {
            CSVReader guesser(delims[i]);
            guesser.read_csv(filename, 100);
            if ((guesser.row_num >= max_rows) &&
                (guesser.get_col_names().size() > max_cols)) {
                max_rows = guesser.row_num > max_rows;
                max_cols = guesser.get_col_names().size();
                current_delim = delims[i];
            }
        }

        return current_delim;
    }
    
    std::vector<std::string> get_col_names(std::string filename, 
        std::string delim, std::string quote, int header) {
        /** Return a CSV's column names */

        CSVReader reader(delim, quote, header);
        reader.read_csv(filename, 1);
        return reader.get_col_names();
    }

    int col_pos(std::string filename, std::string col_name,
        std::string delim, std::string quote, int header) {
        // Resolve column position from column name
        std::vector<std::string> col_names = get_col_names(filename, delim, quote, header);

        auto it = std::find(col_names.begin(), col_names.end(), col_name);
        if (it != col_names.end())
            return it - col_names.begin();
        else
            return -1;
    }

    CSVFileInfo get_file_info(std::string filename) {
        // Get basic information about a CSV file
        //char delim = *(guess_delim(filename).c_str());
        std::string delim = guess_delim(filename);
        CSVReader reader(delim);        
        reader.read_csv(filename, ITERATION_CHUNK_SIZE, false);

        while (!reader.eof) {
            reader.read_csv(filename, ITERATION_CHUNK_SIZE, false);
            reader.clear();
        }

        CSVFileInfo* info = new CSVFileInfo;

        info->filename = filename;
        info->n_rows = reader.correct_rows;
        info->n_cols = (int)reader.get_col_names().size();
        info->col_names = reader.get_col_names();
        info->delim = delim;

        return *info;
    }

    CSVReader::CSVReader(
        std::string delim,
        std::string quote,
        int header,
        std::vector<int> subset_) {
        // Type cast std::string to char
        delimiter = delim[0];
        quote_char = quote[0];
        
        quote_escape = false;
        header_row = header;
        subset = subset_;
    }

    void CSVReader::set_col_names(std::vector<std::string> col_names) {
        /** - Set or override the CSV's column names
         *  - When parsing, rows that are shorter or longer than the list 
         *    of column names get dropped
         */
        
        this->col_names = col_names;
        
        if (this->subset.size() > 0) {
            this->subset_flag = true;
            for (size_t i = 0; i < this->subset.size(); i++) {
                subset_col_names.push_back(col_names[this->subset[i]]);
            }
        } else {
            // "Subset" is every column
            for (size_t i = 0; i < this->col_names.size(); i++) {
                this->subset.push_back(i);
            }
            subset_col_names = col_names;
        }
    }

    std::vector<std::string> CSVReader::get_col_names() {
        return this->subset_col_names;
    }

    void CSVReader::feed(std::string &in) {
        /** Parse a CSV-formatted string. Incomplete CSV fragments can be 
         *  joined together by calling feed() on them sequentially.
         *  **Note**: end_feed() should be called after the last string
         */

        std::string * str_buffer = &(this->records.back().back());

        for (size_t i = 0, ilen = in.length(); i < ilen; i++) {
            if (in[i] == this->delimiter) {
                this->process_possible_delim(in, i, str_buffer);
            } else if (in[i] == this->quote_char) {
                this->process_quote(in, i, str_buffer);
            } else {
                switch(in[i]) {
                    case '\r':
                    case '\n':
                        this->process_newline(in, i, str_buffer);
                        break;
                    default:
                        *str_buffer += in[i];
                }
            }
        }
    }

    void CSVReader::end_feed() {
        /** Indicate that there is no more data to receive,
         *  and handle the last row
         */
        if ((this->records.back()).size() > 1)
            this->write_record();
        else
            this->records.pop_back();
    }

    void CSVReader::process_possible_delim(std::string &in, size_t &index, std::string* &out) {
        /** Process a delimiter character and determine if it is a field
         *  separator
         */

        if (!this->quote_escape) {
            // Case: Not being escaped --> Write field
            this->records.back().push_back(std::string());
            out = &(this->records.back().back());
        } else {
            *out += in[index]; // Treat as regular data
        }
    }

    void CSVReader::process_newline(std::string &in, size_t &index, std::string* &out) {
        /** Process a newline character and determine if it is a record
         *  separator        
         */
        if (!this->quote_escape) {
            // Case: Carriage Return Line Feed, Carriage Return, or Line Feed
            // => End of record -> Write record
            if ((in[index] == '\r') && (in[index + 1] == '\n')) {
                index++;
            }
            
            // Write record
            this->write_record();
            out = &(this->records.back().back());
        } else {
            *out += in[index]; // Quote-escaped
        }
    }

    void CSVReader::process_quote(std::string &in, size_t &index, std::string* &out) {
        /** Determine if the usage of a quote is valid or fix it
         */
        if (this->quote_escape) {
            if ((in[index + 1] == this->delimiter) || 
                (in[index + 1] == '\r') ||
                (in[index + 1] == '\n')) {
                // Case: End of field
                this->quote_escape = false;
            } else {
                // Note: This may fix single quotes (not strictly valid)
                *out += in[index];
                if (in[index + 1] == this->quote_char)
                    index++;  // Case: Two consecutive quotes
            }
        } else {
			/* Add index > 0 to prevent string index errors
             * Case: Previous character was delimiter
             */
            if (index > 0 && in[index - 1] == this->delimiter)
                this->quote_escape = true;
        }
    }

    void CSVReader::write_record() {
        /** Push the current row into a queue if it is the right length.
         *  Drop it otherwise.
         */
        
        auto& record = this->records.back();
        size_t col_names_size = this->col_names.size();
        this->quote_escape = false;  // Unset all flags
        
        if (this->row_num > this->header_row) {            
            /* Workaround: CSV parser doesn't catch the last field if
             * it is empty */             
            if (record.size() + 1 == col_names_size)
                record.push_back(std::string());
            
            // Make sure record is of the right length
            if (record.size() == col_names_size) {
                this->correct_rows++;
                if (this->subset_flag) {
                    std::vector<std::string> subset_record;
                    for (size_t i = 0; i < this->subset.size(); i++)
                        subset_record.push_back(record[ this->subset[i] ]);
                    
                    this->records.pop_back();
                    this->records.push_back(subset_record);
                }
            } else {
                /* 1) Zero-length record, probably caused by extraneous newlines
                 * 2) Too short or too long
                 */
                if (!record.empty() && this->bad_row_handler) {
                    auto copy = this->records.back();
                    this->records.pop_back();
                    this->bad_row_handler(copy);
                }
            }
        } else if (this->row_num == this->header_row) {
            this->set_col_names(this->records.back());
            this->records.pop_back();
        } else {
            // Ignore rows before header row
        }

        this->records.push_back(std::vector<std::string>{std::string()});
        this->records.back().reserve(col_names_size);
        this->row_num++;
    }

    std::vector<std::string> CSVReader::pop() {
        /** - Remove and return the first CSV row
         *  - Considering using empty() to avoid popping from an empty queue
         */
        std::vector< std::string > record = this->records.front();
        this->records.pop_front();
        return record;
    }
    
    std::vector<std::string> CSVReader::pop_back() {
        /** - Remove and return the last CSV row
         *  - Considering using empty() to avoid popping from an empty queue
         */
        std::vector< std::string > record = this->records.back();
        this->records.pop_back();
        return record;
    }
    
    std::map<std::string, std::string> CSVReader::pop_map() {
        /** - Remove and return the first CSV row as a std::map
         *  - Considering using empty() to avoid popping from an empty queue
         */
        std::vector< std::string > record = this->pop();
        std::map< std::string, std::string > record_map;
        
        for (size_t i = 0; i < subset.size(); i ++) {
            record_map[ this->subset_col_names[i] ] = record[i];
        }
        
        return record_map;
    }

    bool CSVReader::empty() {
        /** Indicates whether or not the queue still contains CSV rows */
        return this->records.empty();
    }

    void CSVReader::clear() {
        this->records.clear();
        this->records.push_back(std::vector<std::string>{std::string()});
    }

    void CSVReader::_read_csv() {
        /** Multi-threaded reading/processing worker thread */

        while (true) {
            std::unique_lock<std::mutex> lock{ this->feed_lock }; // Get lock
            this->feed_cond.wait(lock,                            // Wait
                [this] { return !(this->feed_buffer.empty()); });

            // Wake-up
            std::string* in = this->feed_buffer.front();
            this->feed_buffer.pop_front();

            if (!in) { // Nullptr --> Die
                delete in;
                break;    
            }

            lock.unlock();      // Release lock
            this->feed(*in);
            delete in;          // Free buffer
        }
    }

    void CSVReader::read_csv(std::string filename, int nrows, bool close) {
        /** Parse an entire CSV file */

        if (!this->infile) {
            this->infile = std::fopen(filename.c_str(), "r");
            this->infile_name = filename;

            if (!this->infile)
                throw std::runtime_error("Cannot open file");
        }
        
        char * line_buffer = new char[10000];
        std::string* buffer = new std::string();
        std::thread worker(&CSVReader::_read_csv, this);

        while (nrows <= -1 || nrows > 0) {
            char * result = std::fgets(line_buffer, sizeof(char[10000]), this->infile);            
            *buffer += line_buffer;
            nrows--;

            if ((*buffer).size() >= 1000000) {
                std::unique_lock<std::mutex> lock{ this->feed_lock };
                this->feed_buffer.push_back(buffer);
                this->feed_cond.notify_one();
                buffer = new std::string();  // Buffers get freed by worker
                // Doesn't help oddly enough: buffer->reserve(1000000);
            }

            if (result == NULL || std::feof(this->infile))
                break;
        }

        // Feed remaining bits
        std::unique_lock<std::mutex> lock{ this->feed_lock };
        this->feed_buffer.push_back(buffer);
        this->feed_buffer.push_back(nullptr); // Termination signal
        this->feed_cond.notify_one();
        lock.unlock();
        worker.join();

        if (std::feof(this->infile)) {
            this->end_feed();
            this->eof = true;
            this->close();
        }

        if (close)
            this->close();
    }

    void CSVReader::close() {
        std::fclose(this->infile);
    }
    
    std::string CSVReader::csv_to_json(std::vector<std::string>& record) {
        /** Helper method for both to_json() methods */
        std::string json_record = "{";
        std::string * col_name;
        
        for (size_t i = 0; i < this->subset_col_names.size(); i++) {
            col_name = &this->subset_col_names[i];
            json_record += "\"" + *col_name + "\":";
            
            /* Quote strings but not numeric fields
             * Recall data_type() returns 2 for ints and 3 for floats
             */
            
            if (data_type(record[i]) > 1)
                json_record += record[i];
            else
                json_record += "\"" + json_escape(record[i]) + "\"";

            if (i + 1 != record.size())
                json_record += ",";
        }

        json_record += "}";
        return json_record;
    }

    void CSVReader::to_json(std::string filename, bool append) {
        /** Convert CSV to a newline-delimited JSON file, where each
         *  row is mapped to an object with the column names as keys.
         *
         *  # Example
         *  ## Input
         *  <TABLE>
         *      <TR><TH>Name</TH><TH>TD</TH><TH>Int</TH><TH>Yards</TH></TR>
         *      <TR><TD>Tom Brady</TD><TD>2</TD><TD>1</TD><TD>466</TD></TR>
         *      <TR><TD>Matt Ryan</TD><TD>2</TD><TD>0</TD><TD>284</TD></TR>
         *  </TABLE>
         *
         *  ## Output
         *  > to_json("twentyeight-three.ndjson")
         *
         *  > {"Name":"Tom Brady","TD":2,"Int":1,"Yards":466}
         *  >
         *  > {"Name":"Matt Ryan","TD":2,"Int":0,"Yards":284}
         */
        std::vector<std::string> record;
        std::ofstream outfile;

        if (append)
            outfile.open(filename, std::ios_base::app);
        else
            outfile.open(filename);

        for (auto it = this->records.begin(); it != this->records.end(); ++it)
            outfile << this->csv_to_json(*it) + "\n";

        outfile.close();
    }

    std::vector<std::string> CSVReader::to_json() {
        /** Similar to to_json(std::string filename), but outputs a vector of
         *  JSON strings instead
         */
        std::vector<std::string> output;

        for (auto it = this->records.begin(); it != this->records.end(); ++it)
            output.push_back(this->csv_to_json(*it));

        return output;
    }

    void CSVReader::sample(int n) {
        /** Take a random uniform sample (with replacement) of n rows */
        std::deque<std::vector<std::string>> new_rows;
        std::default_random_engine generator;
        std::uniform_int_distribution<int> distribution(0, this->records.size() - 1);

        for (; n > 1; n--)
            new_rows.push_back(this->records[distribution(generator)]);

        this->records.clear();
        this->records.swap(new_rows);
    }
    
    std::string json_escape(std::string in) {
        /** Given a CSV string, convert it to a JSON string with proper
         *  escaping as described by RFC 7159
         */

        std::string out;
        
        for (size_t i = 0, ilen = in.length(); i < ilen; i++) {
            switch (in[i]) {
                case '"':
                    // Assuming quotes come in pairs due to CSV escaping
                    out += "\\\"";
                    i++; // Skip over next quote
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '/':
                    out += "\\/";
                    break;
                case '\r':
                    out += "\\\r";
                    break;
                case '\n':
                    out += "\\\n";
                    break;
                case '\t':
                    out += "\\\t";
                    break;
                default:
                    out += in[i];
            }
        }
        
        return out;
    }


}