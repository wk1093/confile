#include "confile.h"

int main() {
    std::ifstream randomjson("MOCK_DATA.json");
    ConValue value;
    randomjson >> value;
    randomjson.close();

    std::ofstream randomjson2("MOCK_DATA2.json");
    randomjson2 << value;
    randomjson2.close();

    std::ofstream randomcon("MOCK_DATA.con", std::ios::binary);
    value.write(randomcon);
    randomcon.close();

}