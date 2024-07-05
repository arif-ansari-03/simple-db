#include <bits/stdc++.h>

using namespace std;

string to_bits(int a)
{
    string s = "00000000";
    for (int i = 0; i < 8; i++)
        if (a&(1<<i)) s[i] = '1';
    
    for (int i = 0; i < 4; i++)
    swap(s[i], s[7-i]);
    return s;
}

void prt(int start, int row_size)
{
    fstream file("mydb.db", ios::in);
    
    int a = 0;
    char *b = new char[2];
    b[1] = 0;
    file.seekg(start);
    for (int i = 0; i < row_size; i++)
    {
        file.read(b, 1);
        memcpy(&a, b, 1);
        cout << to_bits(a) << ' ';
    }
    cout << '\n';

    file.close();
}

int main()
{

    int start = 790;
    int rows = 10;
    int row_size = 10;

    while (rows--)
    {
        cout << start << ":\t";
        prt(start, row_size);
        start += row_size;
    }
}