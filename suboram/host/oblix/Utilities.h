#ifndef UTILITIES_H
#define UTILITIES_H
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <chrono>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <math.h>
#include "../../../common/block.h"

using namespace std;

class Utilities {
private:
    static int parseLine(char* line);
public:
    Utilities();
    static std::string XOR(std::string value, std::string key);
    static void startTimer(int id);
    static double stopTimer(int id);
    static std::map<int, std::chrono::time_point<std::chrono::high_resolution_clock> > m_begs;
    static std::array<uint8_t, BLOCK_LEN> convertToArray(std::string value);
    virtual ~Utilities();
};

#endif /* UTILITIES_H */

