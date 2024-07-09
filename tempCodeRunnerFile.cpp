ble(MasterRow &mrow)
// {
//     cout << mrow.table_name << '\n';
//     uint32_t offset = 0;
//     for (uint32_t i = 0; i < mrow.num_col; i++)
//     {
//         uint32_t size;
//         memcpy(&size, mrow.names+offset, 4);
//         offset += 4;
//         cout << mrow.names+offset << ' ';
//         offset += size;

//         uint32_t temp;
//         memcpy(&temp, mrow.types+i*5, 1);
//         cout << temp << ' ';
//         memcpy(&temp, mrow.types+i*5+1, 4);
//         cout << temp << '\n';
//     }
//     cout << '\n';
// }