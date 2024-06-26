#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <map>
#include "empty_list.hpp"

using namespace std;

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

uint32_t page_size = 4096;

struct MasterRow
{
    char table[200];
    uint32_t page_num;
    uint32_t next_page;
    uint32_t offset;
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

        cout << first_page.num_pages << '\n';

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

void write_row(uint32_t page_num, uint32_t offset, Row &row, Pager &pager)
{
    if (!pager.pages_index.count(page_num))
    {
        pager.pages_index[page_num] = pager.pages_list.size();
        char *temp = new char[page_size];
        pager.pages_list.emplace_back(temp);
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

    for (auto &i : empty_list.empty_pages)
        cout << i << '\n';
        cout << '\n';
    Row row;

    string input;
    while (1)
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

    pager.close();
}