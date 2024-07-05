#include<iostream>
#include <fstream>
#include <vector>

using namespace std;

struct EmptyList
{
    vector<uint32_t> empty_pages;
    uint32_t num_pages;
    uint32_t page_size;
    bool del = 0;
    bool ins = 0;

    void init(uint32_t num_of_pages, uint32_t page_sz)
    {
        page_size = page_sz;
        num_pages = num_of_pages;
        fstream file("el.db", ios::in);

        uint32_t n, page_num;
        file >> n;
        while (n--)
        {
            file >> page_num;
            empty_pages.emplace_back(page_num);
        }
    }

    void make_pages(uint32_t new_pages)
    {
        fstream file("mydb.db", ios::in|ios::out|ios::ate);

        char* temp = new char[page_size];
        for (uint32_t i = 0; i < page_size; i++) temp[i] = 0;

        for (uint32_t i = 0; i < new_pages; i++)
        {
            file.write(temp, page_size);
            empty_pages.emplace_back(num_pages+i);
        }

        file.close();

        num_pages += new_pages;
        ins = 1;
    }

    uint32_t find_page()
    {
        if (empty_pages.empty()) make_pages(1);

        uint32_t page_num = empty_pages.back();
        cout << "given page: " << page_num << '\n';
        empty_pages.pop_back();
        del = 1;

        return page_num;
    }

    void delete_page(uint32_t page_num)
    {
        empty_pages.emplace_back(page_num);
        ins = 1;
    }

    void close()
    {
        if (ins)
        {
            fstream file("el.db", ios::out);
            file << empty_pages.size() << '\n';
            for (uint32_t &i : empty_pages)
                file << i << '\n';
            file.close();
        }
        else if (del)
        {
            fstream file("el.db", ios::out|ios::in);
            file << empty_pages.size() << '\n';
            file.close();
        }
    }
};
