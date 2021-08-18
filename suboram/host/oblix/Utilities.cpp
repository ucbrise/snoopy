#include "Utilities.h"
#include <iostream>
#include <sstream>
#include <map>
#include <fstream>
#include "sys/types.h"
#include "sys/sysinfo.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "../../../common/block.h"

std::map<int, std::chrono::time_point<std::chrono::high_resolution_clock>> Utilities::m_begs;
std::map<int, double> timehist;

Utilities::Utilities() {
}

Utilities::~Utilities() {
}

void Utilities::startTimer(int id) {
    std::chrono::time_point<std::chrono::high_resolution_clock> m_beg = std::chrono::high_resolution_clock::now();
    m_begs[id] = m_beg;

}

double Utilities::stopTimer(int id) {
    double t = (double) std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_begs[id]).count();
    timehist.erase(id);
    timehist[id] = t;
    return t;
}

std::string Utilities::XOR(std::string value, std::string key) {
    std::string retval(value);

    auto klen = key.length();
    auto vlen = value.length();
    unsigned int k = 0;
    if (klen < vlen) {
        for (long unsigned int i = klen; i < vlen; i++) {
            key += " ";
        }
    } else {
        for (long unsigned int i = vlen; i < klen; i++) {
            value += " ";
        }
    }
    klen = vlen;

    for (unsigned int v = 0; v < vlen; v++) {
        retval[v] = value[v]^key[k];
        k = (++k < klen ? k : 0);
    }

    return retval;
}

std::array<uint8_t, BLOCK_LEN> Utilities::convertToArray(std::string addr) {
    std::array<uint8_t, BLOCK_LEN> res;
    for (int i = 0; i < BLOCK_LEN; i++) {
        res[i] = addr[i];
    }
    return res;
}

