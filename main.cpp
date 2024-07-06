#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <map>
#include "empty_list.hpp"

using namespace std;

// have used pagenum * page_size, can cause overflow. Might want to try long long instead?

bool operator==(const string s1, const string s2)
{
    if (s1.length() != s2.length())
        return 0;

    for (int i = 0; i < s1.length(); i++)
        if (s1[i] != s2[i]) return 0;

    return 1;
}

/*
tables are in linked list style.
At first only one type of table will be used. Later, upgraded to add more tables
*/

/*
Files:
mydb.db: Main data file, first page contains info abt number of pages in all files including itself
and also other info
second page contains the first node of meta table
temp.db: for temp use
*/

string to_bits(int a)
{
    string s = "00000000";
    for (int i = 0; i < 8; i++)
        if (a&(1<<i)) s[i] = '1';
    
    for (int i = 0; i < 4; i++)
    swap(s[i], s[7-i]);
    return s;
}

uint32_t page_size = 400;

struct MasterRow
{
    char table_name[32];
    uint32_t num_col;
    uint32_t first_page;
    uint32_t last_page;
    uint32_t offset;
    uint32_t types_size;
    uint32_t names_size;
    // first 56 bytes can be written directly, remaining (types_size+names_size) have to be
    // written manually
    char* types;
    char* names;

    void create_table(string s)
    {
        first_page = last_page = offset = 0;
        stringstream ss(s);
        string temp;
        ss >> temp;
        strcpy(table_name, temp.c_str());

        vector<string> col_names;
        vector<pair<uint32_t, uint32_t>> col_types;

        num_col = 0;
        while (ss >> temp)
        {
            num_col++;

            col_names.emplace_back(temp);

            ss >> temp;
            pair<uint32_t, uint32_t> col_type;

            if (temp == "varchar")
            {
                col_type.first = 0;
                ss >> col_type.second;
            }
            else if (temp == "int")
            {
                col_type.first = 1;
                col_type.second = 4;
            }
            else
            {
                col_type.first = 2;
                col_type.second = 4;
            }
            col_types.emplace_back(col_type);
        }

        names_size = 4 * num_col;
        for (auto &s : col_names) names_size += 1+s.length();

        uint32_t cur_offset = 0;
        names = new char[names_size];

        for (auto &s : col_names)
        {
            uint32_t len = s.length()+1;
            memcpy(names+cur_offset, &len, 4);
            cur_offset+=4;

            strcpy(names+cur_offset, s.c_str());
            cur_offset += 1+s.length();
        }
        cout << names_size << '\n';
        types_size = 5 * num_col;
        types = new char[types_size];

        for (uint32_t i = 0; i < num_col; i++)
        {
            uint32_t temp = col_types[i].first;
            memcpy(types+5*i, &temp, 1);
            
            temp = col_types[i].second;
            memcpy(types+5*i+1, &temp, 4);
        }
    }

    void read_row(char* page, uint32_t offset)
    {
        names = new char[names_size];
        types = new char[types_size];
        memcpy(names, page+offset+56, names_size);
        memcpy(types, page+offset+56+names_size, types_size);
    }

    uint32_t row_size()
    {
        return 56 + types_size + names_size;
    }
};

struct First_page
{
    uint32_t num_pages;
    uint32_t num_tables;
};

struct Pager
{
    First_page first_page;
    char *buffer;

    vector<char*> pages_list;
    map<uint32_t, uint32_t> pages_index;

    void init()
    {
        fstream file("mydb.db", ios::in);

        buffer = new char[sizeof(First_page)];
        
        file.read(buffer, sizeof(First_page));
        memcpy(&first_page, buffer, sizeof(First_page));

        cout << first_page.num_pages << ' ' << first_page.num_tables << '\n';

        file.close();
        delete buffer;
    }

    void close()
    {
        fstream file("mydb.db", ios::out|ios::in);

        buffer = new char[sizeof(First_page)];
        memcpy(buffer, &first_page, sizeof(First_page));

        file.write(buffer, sizeof(First_page));

        for (auto &[page_num, page_idx] : pages_index)
        {
            if (pages_list[page_idx] == NULL) continue;
            file.seekp(page_num * page_size);
            file.write(pages_list[page_idx], page_size);
            delete pages_list[page_idx];
        }
        file.close();
    }
};

struct GeneralRow
{
    uint32_t row_size;
    char* data;

    void read_row(string s, MasterRow &mrow)
    {
        stringstream ss(s);
        vector<string> data_list;
        
        string input;

        while (ss >> input) data_list.emplace_back(input);
        if (data_list.size()!=mrow.num_col) {cout << "Number of columns don't match\n";return;}

        row_size = 0;
        uint32_t temp;
        for (uint32_t i = 0; i < mrow.num_col; i++)
        {
            temp = 0;
            memcpy(&temp, mrow.types+5*i, 1);
            if (temp == 0)
            {
                memcpy(&temp, mrow.types+5*i+1, 4);
                if (temp < 1+data_list[i].length())
                {
                    cout << "\"" << data_list[i] << "\" is too big\n";
                    return;
                }
                row_size += 4; // to store length of string (which will include the last null byte)
                row_size += 1+data_list[i].length();
            }
            else if (temp == 1 || temp == 2) row_size += 4;
            else { cout << "Invalid datatype.\n"; return; }
        }

        data = new char[row_size];
        // is there anything other than memset for this?
        for (uint32_t i = 0; i < row_size; i++) data[i] = 0;

        uint32_t offset = 0;

        for (uint32_t i = 0; i < mrow.num_col; i++)
        {
            temp = 0;
            memcpy(&temp, mrow.types+5*i, 1);
            if (temp==0)
            {
                temp = 1+data_list[i].length();
                memcpy(data+offset, &temp, 4);
                offset += 4;
                strcpy(data+offset, data_list[i].c_str());
                offset += 1+data_list[i].length();
            }
            else if (temp==1)
            {
                int a = stoi(data_list[i]);
                memcpy(data+offset, &a, 4);
                offset+=4;
            }
            else
            {
                float a = stof(data_list[i]);
                memcpy(data+offset, &a, 4);
                offset+=4;
            }
        }
    }
};

struct mCursor
{
    MasterRow mrow;
    uint32_t prev_page_num;
    uint32_t prev_offset;
    uint32_t page_num;
    uint32_t offset;
    bool end;

    void init(uint32_t pg_nm, uint32_t ofst, Pager &pager)
    {
        prev_page_num = 0;
        prev_offset = 0;
        page_num = pg_nm;
        offset = ofst;
        end = 0;
        if (pager.first_page.num_tables==0)
        {
            end = 1;
            return;
        }

        uint32_t idx;
        if (!pager.pages_index.count(page_num))
        {
            char* buffer = new char[page_size];
            pager.pages_index[page_num] = pager.pages_list.size();
            fstream file("mydb.db", ios::in);
            file.seekg(page_num*page_size);
            file.read(buffer, page_size);
            file.close();
            pager.pages_list.emplace_back(buffer);
        }

        idx = pager.pages_index[page_num];
        memcpy(&mrow, pager.pages_list[idx]+offset, 56);
        mrow.read_row(pager.pages_list[idx], offset);
    }

    void advance(Pager &pager)
    {
        prev_page_num = page_num;
        prev_offset = offset;

        offset += mrow.row_size();
        while (1)
        {
            uint32_t idx;
            if (!pager.pages_index.count(page_num))
            {
                char* buffer = new char[page_size];
                pager.pages_index[page_num] = pager.pages_list.size();
                fstream file("mydb.db", ios::in);
                file.seekg(page_num*page_size);
                file.read(buffer, page_size);
                file.close();
                pager.pages_list.emplace_back(buffer);
            }
            idx = pager.pages_index[page_num];

            memcpy(&mrow, pager.pages_list[idx]+offset, 56);

            if (mrow.table_name[0]==0)
            {
                uint32_t temp;
                memcpy(&temp, pager.pages_list[idx]+(page_size-4), 4);
                if (temp == 0) {end = 1; break;}
                page_num = temp;
                offset = 0;
                continue;
            }
            mrow.read_row(pager.pages_list[idx], offset);
            break;
        }
    }

    // this function loads the second last position of cursor only.
    void prevOnce(Pager &pager)
    {
        page_num = prev_page_num;
        offset = prev_offset;
        end = 0;

        uint32_t idx;
        if (!pager.pages_index.count(page_num))
        {
            char* buffer = new char[page_size];
            pager.pages_index[page_num] = pager.pages_list.size();
            fstream file("mydb.db", ios::in);
            file.seekg(page_num*page_size);
            file.read(buffer, page_size);
            file.close();
            pager.pages_list.emplace_back(buffer);
        }

        idx = pager.pages_index[page_num];
        memcpy(&mrow, pager.pages_list[idx]+offset, 56);
        mrow.read_row(pager.pages_list[idx], offset);
    }
};

struct gCursor
{
    GeneralRow row;
    MasterRow mrow;
    uint32_t page_num;
    uint32_t offset;
    bool end;

    void init(uint32_t pg_no, uint32_t ofst, MasterRow m_row, Pager &pager)
    {
        page_num = pg_no;
        offset = ofst;
        mrow = m_row;
    }
};

void create_table(Pager &pager, EmptyList &empty_list, string s)
{
    MasterRow mrow;
    mrow.create_table(s);

    uint32_t row_size = mrow.row_size();

    uint32_t page_num = 0, offset = 0;

    mCursor iter;
    bool init = 0;
    if (pager.first_page.num_tables>0) {iter.init(1, 0, pager); init = 1;}

    while (init)
    {
        uint32_t idx = pager.pages_index[iter.page_num];
        uint32_t mem_used;
        memcpy(&mem_used, pager.pages_list[idx]+(page_size-8), 4);

        mem_used += 9;

        if (page_size-mem_used>=mrow.row_size())
        {
            uint32_t pg_no = iter.page_num;
            while (!iter.end)
                iter.advance(pager);
            page_num = iter.page_num;
            offset = iter.offset;
            break;
        }
        uint32_t pg_no = iter.page_num;
        while (!iter.end && pg_no == iter.page_num)
            iter.advance(pager);
        if (iter.end) {page_num = 0, offset = 0; break;}
    }

    if (page_num == 0)
    {
        page_num = empty_list.find_page();
        offset = 0;
        // The current last page says there is no next page, but we just made one.
        // The following if statement adds the new page given by empty_list to the
        // current last page.
        if (init) 
        {
            uint32_t idx;
            if (!pager.pages_index.count(iter.page_num))
            {
                fstream file("mydb.db", ios::in);
                char* buffer = new char[page_size];
                file.seekg(iter.page_num * page_size);
                file.read(buffer, page_size);
                file.close();
                pager.pages_index[iter.page_num] = pager.pages_list.size();
                pager.pages_list.emplace_back(buffer);
                file.close();
            }
            idx = pager.pages_index[iter.page_num];
            memcpy(pager.pages_list[idx]+page_size-4, &page_num, 4);
        }
    }

    if (!pager.pages_index.count(page_num))
    {
        pager.pages_index[page_num] = pager.pages_list.size();
        char *temp = new char[page_size];
        fstream file("mydb.db", ios::in);
        file.seekg(page_num * page_size);
        file.read(temp, page_size);
        file.close();
        pager.pages_list.emplace_back(temp);
    }

    cout << page_num << '\n';
    uint32_t idx = pager.pages_index[page_num];

    memcpy(pager.pages_list[idx]+offset, &mrow, 56);
    memcpy(pager.pages_list[idx]+offset+56, mrow.names, mrow.names_size);
    memcpy(pager.pages_list[idx]+offset+56+mrow.names_size, mrow.types, mrow.types_size);
    uint32_t temp;
    memcpy(&temp, pager.pages_list[idx]+page_size-8, 4);
    temp+=row_size;
    memcpy(pager.pages_list[idx]+page_size-8, &temp, 4);
    pager.first_page.num_tables++;
}

void show_all_tables(Pager &pager)
{
    mCursor iter;
    cout << pager.first_page.num_tables << '\n';
    if (pager.first_page.num_tables == 0) return;

    iter.init(1, 0, pager);
    while(iter.end != 1)
    {
        cout << iter.mrow.table_name << '\n';

        uint32_t offset = 0;
        for (uint32_t i = 0; i < iter.mrow.num_col; i++)
        {
            uint32_t size;
            memcpy(&size, iter.mrow.names+offset, 4);
            offset += 4;
            cout << iter.mrow.names+offset << ' ';
            offset += size;
 
            uint32_t temp;
            memcpy(&temp, iter.mrow.types+i*5, 1);
            cout << temp << ' ';
            memcpy(&temp, iter.mrow.types+i*5+1, 4);
            cout << temp << '\n';
        }
        cout << '\n';

        iter.advance(pager);
    }
}

pair<uint32_t, uint32_t> find_table(string table_name, Pager &pager)
{
    bool found = 0;
    uint32_t page_num = 0, offset = 0;

    if (pager.first_page.num_tables>0)
    {
        mCursor iter;
        iter.init(1, 0, pager);
        while (!iter.end)
        {
            if (iter.mrow.table_name == table_name)
            {
                page_num = iter.page_num;
                offset = iter.offset;
                break;
            }
            iter.advance(pager); 
        }
    }
    return {page_num, offset};
}

GeneralRow serialize_row(string s, string table_name, Pager &pager)
{
    pair<uint32_t, uint32_t> temp = find_table(table_name, pager);
    if (temp.first == 0) {cout << "No such table\n"; return;}
    mCursor iter;
    iter.init(temp.first, temp.second, pager);

    GeneralRow row;
    row.read_row(s, iter.mrow);

    return row;
}


int main()
{
    Pager pager;
    pager.init();

    EmptyList empty_list;
    empty_list.init(pager.first_page.num_pages, page_size);
    

    MasterRow mrow;

    string input;
    while (1)
    {
        cout << "db >> ";

        cin >> input;

        if (input == "quit")
        {
            break;
        }

        if (input == "show")
        {
            cin >> input;
            show_all_tables(pager);
        }

        else if (input == "create")
        {
            cin >> input; // read "table"
            string s = "";
            getline(cin, s);
            create_table(pager, empty_list, s);
        }

        else if (input == "insert")
        {
            cin >> input; // read "into"
            string table_name;
            cin >> table_name;

            string s = "";
            getline(cin, s);
            serialize_row(s, table_name, pager);
        }

        else if (input == "find")
        {
            cin >> input;
            pair<uint32_t, uint32_t> P = find_table(input, pager);
            cout << P.first << ' ' << P.second << '\n';
        }
    }


    pager.first_page.num_pages = empty_list.num_pages;
    empty_list.close();
    pager.close();
}