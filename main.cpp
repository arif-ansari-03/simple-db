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

uint32_t page_size = 4096;

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


struct GeneralRow
{
    vector<uint32_t> col_type;
    vector<uint32_t> col_offset;
    char* data;
    uint32_t row_size;

    void readGenRow(string s)
    {
        stringstream ss(s);
        for (uint32_t i = 0; i < col_type.size(); i++)
        {
            uint32_t col_size;
            if (i+1 < col_type.size()) col_size = col_offset[i+1]-col_offset[i];
            else col_size = row_size - col_offset[i];

            if (col_type[i]==0)
            {
                string temp;
                ss >> temp;
                if ((int)temp.length()>((int)col_size-1))
                {
                    cout << "String too big\n";
                    temp = "";
                }
                memcpy(data+col_offset[i], temp.c_str(), col_size);
            }
            else if (col_type[i]==1)
            {
                int temp;
                ss >> temp;
            }
            else if (col_type[i]==2)
            {
                float temp;
                ss >> temp;
                memcpy(data+col_offset[i], &temp, col_size);
            }
            else
            {
                cout << "invalid column type\n";
            }
        }
        cout << '\n';
    }

    void printGenRow()
    {
        for (uint32_t i = 0; i < col_type.size(); i++)
        {
            uint32_t col_size;
            if (i+1 < col_type.size()) col_size = col_offset[i+1]-col_offset[i];
            else col_size = row_size - col_offset[i];

            if (col_type[i]==0)
            {
                char *buffer = new char[col_size];
                memcpy(buffer, data+col_offset[i], col_size);
                cout << buffer << ' ';
            }
            else if (col_type[i]==1)
            {
                int temp;
                memcpy(&temp, data+col_offset[i], col_size);
                cout << temp << ' ';
            }
            else if (col_type[i]==2)
            {
                float temp;
                memcpy(&temp, data+col_offset[i], col_size);
                cout << temp << ' ';
            }
            else
            {
                cout << "invalid column type\n";
            }
        }
        cout << '\n';
    }
};

struct Row
{
    uint32_t id;
    char user[200];
    uint32_t next_page;
    uint32_t offset;
};

struct First_page
{
    uint32_t num_pages;
    uint32_t num_tables;
    uint32_t last_row_page_num;
    uint32_t last_row_offset;
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
        
        file.read(buffer, sizeof(buffer));
        memcpy(&first_page, buffer, sizeof(First_page));

        file.close();
        delete buffer;
    }

    void close()
    {
        fstream file("mydb.db", ios::out|ios::in);

        buffer = new char[sizeof(First_page)];
        
        memcpy(buffer, &first_page, sizeof(buffer));

        file.write(buffer, sizeof(buffer));

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
                memcpy(&temp, pager.pages_list[idx]+(4096-4), 4);
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

void create_table(Pager &pager, EmptyList &empty_list, string s)
{
    MasterRow mrow;
    mrow.create_table(s);

    uint32_t row_size = mrow.row_size();

    uint32_t page_num, offset;

    mCursor iter;
    iter.init(1, 0, pager);

    while (1)
    {
        uint32_t idx = pager.pages_index[iter.page_num];
        uint32_t mem_used;
        memcpy(&mem_used, pager.pages_list[idx]+(4096-8), 4);

        mem_used += 9;

        if (4096-mem_used>=mrow.row_size())
        {
            uint32_t pg_no = iter.page_num;
            while (!iter.end && pg_no == iter.page_num)
                iter.advance(pager);
            iter.prevOnce(pager);
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

    uint32_t idx = pager.pages_index[page_num];

    memcpy(pager.pages_list[idx], &mrow, 56);
    memcpy(pager.pages_list[idx]+56, mrow.names, mrow.names_size);
    memcpy(pager.pages_list[idx]+56+mrow.names_size, mrow.types, mrow.types_size);
    uint32_t temp;
    memcpy(&temp, pager.pages_list[idx]+4096-8, 4);
    temp+=row_size;
    memcpy(pager.pages_list[idx]+4096-8, &temp, 4);
    cout << temp << ' ';
    pager.first_page.num_tables++;
}

void write_row(uint32_t page_num, uint32_t offset, Row &row, Pager &pager)
{
    if (!pager.pages_index.count(page_num))
    {
        pager.pages_index[page_num] = pager.pages_list.size();
        char *temp = new char[page_size];
        pager.pages_list.emplace_back(temp);

        fstream file("mydb.db", ios::in);
        file.seekg(page_num * page_size);
        file.read(temp, page_size);
        file.close();
    }

    uint32_t idx = pager.pages_index[page_num];

    memcpy(pager.pages_list[idx]+offset, &row, sizeof(Row));
}

void read_row(uint32_t page_num, uint32_t offset, Row &row, Pager &pager)
{
    if (!pager.pages_index.count(page_num))
    {
        pager.pages_index[page_num] = pager.pages_list.size();
        char *temp = new char[page_size];
        pager.pages_list.emplace_back(temp);

        fstream file("mydb.db", ios::in);
        file.seekg(page_num * page_size);
        file.read(temp, page_size);
        file.close();
    }

    uint32_t idx = pager.pages_index[page_num];
    memcpy(&row, pager.pages_list[idx]+offset, sizeof(Row));
}

void insert()
{

}

int main()
{
    Pager pager;
    pager.init();

    EmptyList empty_list;
    empty_list.init(pager.first_page.num_pages);

    Row row;
    MasterRow mrow;
    
    // create_table(pager, empty_list, "User3 id int username2 varchar 20");

    char* buffer = new char[page_size];
    fstream file("mydb.db", ios::in);

    // first table at 0, then 86, then 172
    file.seekp(page_size);

    file.read(buffer, page_size);


    cout << mrow.num_col << '\n';
    file.close();



    string input;
    while (0)
    {
        cout << "db >> ";

        cin >> input;

        if (input == "quit")
        {
            break;
        }

        if (input == "insert")
        {
            cin >> row.id;
            string temp;
            cin >> temp;
            strcpy(row.user, temp.c_str());

            row.next_page = 0;
            row.offset = 0;

            write_row(2, 0, row, pager);
        }

        else if (input == "select")
        {
            read_row(1, 0, row, pager);
            cout << row.id << ' ' << row.user << '\n';
        }
    }

    empty_list.close();
    pager.close();
}