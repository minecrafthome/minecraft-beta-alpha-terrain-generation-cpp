#ifndef NOISE_HEADER
#define NOISE_HEADER

#include "javaRnd.h"

struct PermutationTable {
    double xo;
    double yo;
    double zo; // this actually never used in fixed noise aka 2d noise;)
    uint8_t permutations[512];
};

static inline void initOctaves(PermutationTable octaves[], Random *random, int nbOctaves) {
    for (int i = 0; i < nbOctaves; ++i) {
        octaves[i].xo = next_double(random) * 256.0;
        octaves[i].yo = next_double(random) * 256.0;
        octaves[i].zo = next_double(random) * 256.0;
        uint8_t *permutations = octaves[i].permutations;
        uint8_t j=0;
        do{
            permutations[j] = j;
        }while(j++!=255);
        uint8_t index = 0;
        do {
            uint32_t randomIndex = random_next_int(random, 256u - index) + index;
            if (randomIndex != index) {
                // swap
                permutations[index] ^= permutations[randomIndex];
                permutations[randomIndex] ^= permutations[index];
                permutations[index] ^= permutations[randomIndex];
            }
            permutations[index + 256] = permutations[index];
        }while(index++!=255);
    }
}

#endif //NOISE_HEADER
