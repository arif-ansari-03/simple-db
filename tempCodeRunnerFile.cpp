
            uint32_t len = s.length()+1;
            memcpy(names+cur_offset, &len, 4);
            cur_offset+=4;

            strcpy(names+cur_offset, s.c_str());
            cur_offset += 1+s.length();
        }
