// nvcc -std c++14 main.cu -o terrain_gen -O3
#include <iostream>
#include <chrono>
#include <string>
#include <fstream>
#include <algorithm>
#include <memory.h>

static_assert(std::numeric_limits<double>::is_iec559, "This code requires IEEE-754 doubles");

#define OFFSET 12
#define Random uint64_t
#define RANDOM_MULTIPLIER 0x5DEECE66DULL
#define RANDOM_ADDEND 0xBULL
#define RANDOM_MASK ((1ULL << 48u) - 1)
#define RANDOM_SCALE 0x1.0p-53
#define get_random(seed) ((Random)((seed ^ RANDOM_MULTIPLIER) & RANDOM_MASK))

static inline void advance4(Random *random) {
    *random = (*random * 0x32EB772C5F11LLU + 0x2D3873C4CD04LLU) & RANDOM_MASK;
}

static inline void advance6(Random *random) {
    *random = (*random * 0x45D73749A7F9LLU + 0x17617168255ELLU) & RANDOM_MASK;
}

static inline int32_t random_next(Random *random, int bits) {
    *random = (*random * RANDOM_MULTIPLIER + RANDOM_ADDEND) & RANDOM_MASK;
    return (int32_t) (*random >> (48u - bits));
}

static inline int32_t random_next_int(Random *random, const uint16_t bound) {
    int32_t r = random_next(random, 31);
    const uint16_t m = bound - 1u;
    if ((bound & m) == 0) {
        r = (int32_t) ((bound * (uint64_t) r) >> 31u);
    } else {
        for (int32_t u = r;
             u - (r = u % bound) + m < 0;
             u = random_next(random, 31));
    }
    return r;
}

static inline double next_double(Random *random) {
    return (double) ((((uint64_t) ((uint32_t) random_next(random, 26)) << 27u)) + random_next(random, 27)) * RANDOM_SCALE;
}

inline uint64_t random_next_long(Random *random) {
    return (((uint64_t) random_next(random, 32)) << 32u) + (int32_t) random_next(random, 32);
}

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
        uint8_t j = 0;
        do {
            permutations[j] = j;
        } while (j++ != 255);
        uint8_t index = 0;
        do {
            uint32_t randomIndex = (uint32_t) random_next_int(random, 256u - index) + index;
            if (randomIndex != index) {
                // swap
                permutations[index] ^= permutations[randomIndex];
                permutations[randomIndex] ^= permutations[index];
                permutations[index] ^= permutations[randomIndex];
            }
            permutations[index + 256] = permutations[index];
        } while (index++ != 255);
    }
}

struct BiomeNoises {
    PermutationTable temperatureOctaves[4];
    PermutationTable humidityOctaves[4];
    PermutationTable precipitationOctaves[2];
};


enum Biomes {
    Rainforest,
    Swampland,
    Seasonal_forest,
    Forest,
    Savanna,
    Shrubland,
    Taiga,
    Desert,
    Plains,
    IceDesert,
    Tundra,
};

struct BiomeResult {
    double *temperature;
    double *humidity;
    Biomes *biomes;
};


#define F2 0.3660254037844386
#define G2 0.21132486540518713

struct TerrainNoises {
    PermutationTable minLimit[16];
    PermutationTable maxLimit[16];
    PermutationTable mainLimit[8];
    //PermutationTable shoresBottomComposition[4];
    PermutationTable surfaceElevation[4];
    PermutationTable scale[10];
    PermutationTable depth[16];
};
enum blocks {
    AIR,
    STONE,
    GRASS,
    DIRT,
    BEDROCK,
    MOVING_WATER,
    SAND,
    GRAVEL,
    ICE,
};

struct TerrainResult {
    BiomeResult *biomeResult;
    uint8_t *chunkHeights;
};


const char *biomesNames[] = {"Rainforest", "Swampland", "Seasonal Forest", "Forest", "Savanna", "Shrubland", "Taiga", "Desert", "Plains", "IceDesert", "Tundra"};
// @formatter:off
Biomes biomesTable[4096]={Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Desert, Desert, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Desert, Desert, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Savanna, Savanna, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Plains, Plains, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Shrubland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Seasonal_forest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Seasonal_forest, Rainforest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Rainforest, Rainforest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Rainforest, Rainforest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Rainforest, Rainforest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Rainforest, Rainforest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Forest, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Rainforest, Rainforest, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Tundra, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Taiga, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Swampland, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Forest, Rainforest, Rainforest,};
// @formatter:on


int grad2[12][2] = {{1,  1,},
                    {-1, 1,},
                    {1,  -1,},
                    {-1, -1,},
                    {1,  0,},
                    {-1, 0,},
                    {1,  0,},
                    {-1, 0,},
                    {0,  1,},
                    {0,  -1,},
                    {0,  1,},
                    {0,  -1,}};


static inline void simplexNoise(double **buffer, double chunkX, double chunkZ, int x, int z, double offsetX, double offsetZ, double octaveFactor, PermutationTable permutationTable) {
    int k = 0;
    uint8_t *permutations = permutationTable.permutations;
    for (int X = 0; X < x; X++) {
        double XCoords = (chunkX + (double) X) * offsetX + permutationTable.xo;
        for (int Z = 0; Z < z; Z++) {
            double ZCoords = (chunkZ + (double) Z) * offsetZ + permutationTable.yo;
            // Skew the input space to determine which simplex cell we're in
            double hairyFactor = (XCoords + ZCoords) * F2;
            auto tempX = static_cast<int32_t>(XCoords + hairyFactor);
            auto tempZ = static_cast<int32_t>(ZCoords + hairyFactor);
            int32_t xHairy = (XCoords + hairyFactor < tempX) ? (tempX - 1) : (tempX);
            int32_t zHairy = (ZCoords + hairyFactor < tempZ) ? (tempZ - 1) : (tempZ);
            double d11 = (double) (xHairy + zHairy) * G2;
            double X0 = (double) xHairy - d11; // Unskew the cell origin back to (x,y) space
            double Y0 = (double) zHairy - d11;
            double x0 = XCoords - X0; // The x,y distances from the cell origin
            double y0 = ZCoords - Y0;
            // For the 2D case, the simplex shape is an equilateral triangle.
            // Determine which simplex we are in.
            int offsetSecondCornerX, offsetSecondCornerZ; // Offsets for second (middle) corner of simplex in (i,j) coords

            if (x0 > y0) {  // lower triangle, XY order: (0,0)->(1,0)->(1,1)
                offsetSecondCornerX = 1;
                offsetSecondCornerZ = 0;
            } else { // upper triangle, YX order: (0,0)->(0,1)->(1,1)
                offsetSecondCornerX = 0;
                offsetSecondCornerZ = 1;
            }

            double x1 = (x0 - (double) offsetSecondCornerX) + G2; // Offsets for middle corner in (x,y) unskewed coords
            double y1 = (y0 - (double) offsetSecondCornerZ) + G2;
            double x2 = (x0 - 1.0) + 2.0 * G2; // Offsets for last corner in (x,y) unskewed coords
            double y2 = (y0 - 1.0) + 2.0 * G2;

            // Work out the hashed gradient indices of the three simplex corners
            uint32_t ii = (uint32_t) xHairy & 0xffu;
            uint32_t jj = (uint32_t) zHairy & 0xffu;
            uint8_t gi0 = permutations[(uint16_t) (ii + permutations[jj]) & 0xffu] % 12u;
            uint8_t gi1 = permutations[(uint16_t) (ii + offsetSecondCornerX + permutations[(uint16_t) (jj + offsetSecondCornerZ) & 0xffu]) & 0xffu] % 12u;
            uint8_t gi2 = permutations[(uint16_t) (ii + 1 + permutations[(uint16_t) (jj + 1) & 0xffu]) & 0xffu] % 12u;

            // Calculate the contribution from the three corners
            double t0 = 0.5 - x0 * x0 - y0 * y0;
            double n0;
            if (t0 < 0.0) {
                n0 = 0.0;
            } else {
                t0 *= t0;
                n0 = t0 * t0 * ((double) grad2[gi0][0] * x0 + (double) grad2[gi0][1] * y0);  // (x,y) of grad2 used for 2D gradient
            }
            double t1 = 0.5 - x1 * x1 - y1 * y1;
            double n1;
            if (t1 < 0.0) {
                n1 = 0.0;
            } else {
                t1 *= t1;
                n1 = t1 * t1 * ((double) grad2[gi1][0] * x1 + (double) grad2[gi1][1] * y1);
            }
            double t2 = 0.5 - x2 * x2 - y2 * y2;
            double n2;
            if (t2 < 0.0) {
                n2 = 0.0;
            } else {
                t2 *= t2;
                n2 = t2 * t2 * ((double) grad2[gi2][0] * x2 + (double) grad2[gi2][1] * y2);
            }
            // Add contributions from each corner to get the final noise value.
            // The result is scaled to return values in the interval [-1,1].
            (*buffer)[k] = (*buffer)[k] + 70.0 * (n0 + n1 + n2) * octaveFactor;
            k++;

        }

    }
}


static inline void getFixedNoise(double *buffer, double chunkX, double chunkZ, int sizeX, int sizeZ, double offsetX, double offsetZ, double ampFactor, PermutationTable *permutationTable, uint8_t octaves) {
    offsetX /= 1.5;
    offsetZ /= 1.5;
    // cache should be created by the caller
    memset(buffer, 0, sizeof(double) * sizeX * sizeZ);

    double octaveDiminution = 1.0;
    double octaveAmplification = 1.0;
    for (uint8_t j = 0; j < octaves; ++j) {
        simplexNoise(&buffer, chunkX, chunkZ, sizeX, sizeZ, offsetX * octaveAmplification, offsetZ * octaveAmplification, 0.55000000000000004 / octaveDiminution, permutationTable[j]);
        octaveAmplification *= ampFactor;
        octaveDiminution *= 0.5;
    }

}


static inline BiomeNoises *initBiomeGen(uint64_t worldSeed) {
    auto *pBiomeNoises = new BiomeNoises;
    Random worldRandom;
    PermutationTable *octaves;
    worldRandom = get_random(worldSeed * 9871L);
    octaves = pBiomeNoises->temperatureOctaves;
    initOctaves(octaves, &worldRandom, 4);
    worldRandom = get_random(worldSeed * 39811L);
    octaves = pBiomeNoises->humidityOctaves;
    initOctaves(octaves, &worldRandom, 4);
    worldRandom = get_random(worldSeed * 543321L);
    octaves = pBiomeNoises->precipitationOctaves;
    initOctaves(octaves, &worldRandom, 2);
    return pBiomeNoises;
}


static inline BiomeResult *getBiomes(int posX, int posZ, int sizeX, int sizeZ, BiomeNoises *biomesOctaves) {
    auto *biomes = new Biomes[16 * 16];
    auto *biomeResult = new BiomeResult;
    auto *temperature = new double[sizeX * sizeZ];
    auto *humidity = new double[sizeX * sizeZ];
    auto *precipitation = new double[sizeX * sizeZ];
    getFixedNoise(temperature, posX, posZ, sizeX, sizeZ, 0.02500000037252903, 0.02500000037252903, 0.25, (*biomesOctaves).temperatureOctaves, 4);
    getFixedNoise(humidity, posX, posZ, sizeX, sizeZ, 0.05000000074505806, 0.05000000074505806, 0.33333333333333331, (*biomesOctaves).humidityOctaves, 4);
    getFixedNoise(precipitation, posX, posZ, sizeX, sizeZ, 0.25, 0.25, 0.58823529411764708, (*biomesOctaves).precipitationOctaves, 2);
    int index = 0;
    for (int X = 0; X < sizeX; X++) {
        for (int Z = 0; Z < sizeZ; Z++) {
            double preci = precipitation[index] * 1.1000000000000001 + 0.5;
            double temp = (temperature[index] * 0.14999999999999999 + 0.69999999999999996) * (1.0 - 0.01) + preci * 0.01;
            temp = 1.0 - (1.0 - temp) * (1.0 - temp);
            if (temp < 0.0) {
                temp = 0.0;
            }
            if (temp > 1.0) {
                temp = 1.0;
            }
            double humi = (humidity[index] * 0.14999999999999999 + 0.5) * (1.0 - 0.002) + preci * 0.002;
            if (humi < 0.0) {
                humi = 0.0;
            }
            if (humi > 1.0) {
                humi = 1.0;
            }
            temperature[index] = temp;
            humidity[index] = humi;
            biomes[index] = biomesTable[(int) (temp * 63) + (int) (humi * 63) * 64];
            index++;
        }
    }
    delete[] precipitation;
    biomeResult->biomes = biomes;
    biomeResult->temperature = temperature;
    biomeResult->humidity = humidity;
    return biomeResult;
}

void delete_biome_result(BiomeResult *biomeResult) {
    delete[] biomeResult->biomes;
    delete[] biomeResult->temperature;
    delete[] biomeResult->humidity;
    delete biomeResult;
}

BiomeResult *BiomeWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
    BiomeNoises *biomesOctaves = initBiomeGen(worldSeed);
    auto *biomes = getBiomes(chunkX * 16, chunkZ * 16, 16, 16, biomesOctaves);
    delete biomesOctaves;
    return biomes;
}

static inline double lerp(double x, double a, double b) {
    return a + x * (b - a);
}

static inline double grad(uint8_t hash, double x, double y, double z) {
    switch (hash & 0xFu) {
        case 0x0:
            return x + y;
        case 0x1:
            return -x + y;
        case 0x2:
            return x - y;
        case 0x3:
            return -x - y;
        case 0x4:
            return x + z;
        case 0x5:
            return -x + z;
        case 0x6:
            return x - z;
        case 0x7:
            return -x - z;
        case 0x8:
            return y + z;
        case 0x9:
            return -y + z;
        case 0xA:
            return y - z;
        case 0xB:
            return -y - z;
        case 0xC:
            return y + x;
        case 0xD:
            return -y + z;
        case 0xE:
            return y - x;
        case 0xF:
            return -y - z;
        default:
            return 0; // never happens
    }
}

static inline double grad2D(uint8_t hash, double x, double z) {
    return grad(hash, x, 0, z);
}

//we care only about 60-61, 77-78, 145-146, 162-163, 230-231, 247-248, 315-316, 332-333, 400-401, 417-418
static inline void generatePermutations(double **buffer, double x, double y, double z, double noiseFactorX, double noiseFactorY, double noiseFactorZ, double octaveSize, PermutationTable permutationTable) {
    uint8_t *permutations = permutationTable.permutations;
    double octaveWidth = 1.0 / octaveSize;
    int32_t i2 = -1;
    double x1 = 0.0;
    double x2 = 0.0;
    double xx1 = 0.0;
    double xx2 = 0.0;
    double t;
    double w;
    int columnIndex = 0; // possibleX[0]*5*17+3*17
    int possibleX[10] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 4};
    int possibleZ[10] = {3, 4, 3, 4, 3, 4, 3, 4, 3, 4};
    for (int index = 0; index < 10; index++) {
        double xCoord = (x + (double) possibleX[index]) * noiseFactorX + permutationTable.xo;
        auto clampedXcoord = (int32_t) xCoord;
        if (xCoord < (double) clampedXcoord) {
            clampedXcoord--;
        }
        auto xBottoms = (uint8_t) ((uint32_t) clampedXcoord & 0xffu);
        xCoord -= clampedXcoord;
        t = xCoord * 6 - 15;
        w = (xCoord * t + 10);
        double fadeX = xCoord * xCoord * xCoord * w;
        double zCoord = (z + (double) possibleZ[index]) * noiseFactorZ + permutationTable.zo;
        auto clampedZCoord = (int32_t) zCoord;
        if (zCoord < (double) clampedZCoord) {
            clampedZCoord--;
        }
        auto zBottoms = (uint8_t) ((uint32_t) clampedZCoord & 0xffu);
        zCoord -= clampedZCoord;
        t = zCoord * 6 - 15;
        w = (zCoord * t + 10);
        double fadeZ = zCoord * zCoord * zCoord * w;
        for (int Y = 0; Y < 11; Y++) { // we cannot limit on lower bound without some issues later
            // ZCoord
            double yCoords = (y + (double) Y) * noiseFactorY + permutationTable.yo;
            auto clampedYCoords = (int32_t) yCoords;
            if (yCoords < (double) clampedYCoords) {
                clampedYCoords--;
            }
            auto yBottoms = (uint8_t) ((uint32_t) clampedYCoords & 0xffu);
            yCoords -= clampedYCoords;
            t = yCoords * 6 - 15;
            w = yCoords * t + 10;
            double fadeY = yCoords * yCoords * yCoords * w;
            // ZCoord

            if (Y == 0 || yBottoms != i2) { // this is wrong on so many levels, same ybottoms doesnt mean x and z were the same...
                i2 = yBottoms;
                uint16_t k2 = permutations[permutations[xBottoms] + yBottoms] + zBottoms;
                uint16_t l2 = permutations[permutations[xBottoms] + yBottoms + 1] + zBottoms;
                uint16_t k3 = permutations[permutations[xBottoms + 1] + yBottoms] + zBottoms;
                uint16_t l3 = permutations[permutations[xBottoms + 1] + yBottoms + 1] + zBottoms;
                x1 = lerp(fadeX, grad(permutations[k2], xCoord, yCoords, zCoord), grad(permutations[k3], xCoord - 1.0, yCoords, zCoord));
                x2 = lerp(fadeX, grad(permutations[l2], xCoord, yCoords - 1.0, zCoord), grad(permutations[l3], xCoord - 1.0, yCoords - 1.0, zCoord));
                xx1 = lerp(fadeX, grad(permutations[k2 + 1], xCoord, yCoords, zCoord - 1.0), grad(permutations[k3 + 1], xCoord - 1.0, yCoords, zCoord - 1.0));
                xx2 = lerp(fadeX, grad(permutations[l2 + 1], xCoord, yCoords - 1.0, zCoord - 1.0), grad(permutations[l3 + 1], xCoord - 1.0, yCoords - 1.0, zCoord - 1.0));
            }
            double y1 = lerp(fadeY, x1, x2);
            double y2 = lerp(fadeY, xx1, xx2);
            (*buffer)[columnIndex] = (*buffer)[columnIndex] + lerp(fadeZ, y1, y2) * octaveWidth;
            columnIndex++;
        }
    }
}

static inline void generateFixedPermutations(double **buffer, double x, double z, int sizeX, int sizeZ, double noiseFactorX, double noiseFactorZ, double octaveSize, PermutationTable permutationTable) {
    int index = 0;
    uint8_t *permutations = permutationTable.permutations;
    double octaveWidth = 1.0 / octaveSize;
    for (int X = 0; X < sizeX; X++) {
        double xCoord = (x + (double) X) * noiseFactorX + permutationTable.xo;
        int clampedXCoord = (int) xCoord;
        if (xCoord < (double) clampedXCoord) {
            clampedXCoord--;
        }
        auto xBottoms = (uint16_t) ((uint32_t) clampedXCoord & 0xffu);
        xCoord -= clampedXCoord;
        double fadeX = xCoord * xCoord * xCoord * (xCoord * (xCoord * 6.0 - 15.0) + 10.0);
        for (int Z = 0; Z < sizeZ; Z++) {
            double zCoord = (z + (double) Z) * noiseFactorZ + permutationTable.zo;
            int clampedZCoord = (int) zCoord;
            if (zCoord < (double) clampedZCoord) {
                clampedZCoord--;
            }
            auto zBottoms = (uint16_t) ((uint32_t) clampedZCoord & 0xffu);
            zCoord -= clampedZCoord;
            double fadeZ = zCoord * zCoord * zCoord * (zCoord * (zCoord * 6.0 - 15.0) + 10.0);
            uint16_t hhxz = ((permutations[permutations[xBottoms] & 0xffu] & 0xffu) + zBottoms) & 0xffu;
            uint16_t hhx1z = ((permutations[permutations[(xBottoms + 1u) & 0xffu] & 0xffu] & 0xffu) + zBottoms) & 0xffu;
            uint16_t Hhhxz = permutations[hhxz & 0xffu];
            uint16_t Hhhx1z = permutations[hhx1z & 0xffu];
            uint16_t Hhhxz1 = permutations[(hhxz + 1u) & 0xffu];
            uint16_t Hhhx1z1 = permutations[(hhx1z + 1u) & 0xffu];
            double x1 = lerp(fadeX, grad2D(Hhhxz, xCoord, zCoord), grad2D(Hhhx1z, xCoord - 1.0, zCoord));
            double x2 = lerp(fadeX, grad2D(Hhhxz1, xCoord, zCoord - 1.0), grad2D(Hhhx1z1, xCoord - 1.0, zCoord - 1.0));
            double y1 = lerp(fadeZ, x1, x2);
            (*buffer)[index] = (*buffer)[index] + y1 * octaveWidth;
            index++;
        }
    }
}

static inline void generateNormalPermutations(double **buffer, double x, double y, double z, int sizeX, int sizeY, int sizeZ, double noiseFactorX, double noiseFactorY, double noiseFactorZ, double octaveSize, PermutationTable permutationTable) {
    uint8_t *permutations = permutationTable.permutations;
    double octaveWidth = 1.0 / octaveSize;
    int32_t i2 = -1;
    double x1 = 0.0;
    double x2 = 0.0;
    double xx1 = 0.0;
    double xx2 = 0.0;
    double t;
    double w;
    int columnIndex = 0;
    for (int X = 0; X < sizeX; X++) {
        double xCoord = (x + (double) X) * noiseFactorX + permutationTable.xo;
        auto clampedXcoord = (int32_t) xCoord;
        if (xCoord < (double) clampedXcoord) {
            clampedXcoord--;
        }
        auto xBottoms = (uint8_t) ((uint32_t) clampedXcoord & 0xffu);
        xCoord -= clampedXcoord;
        t = xCoord * 6 - 15;
        w = (xCoord * t + 10);
        double fadeX = xCoord * xCoord * xCoord * w;
        for (int Z = 0; Z < sizeZ; Z++) {
            double zCoord = (z + (double) Z) * noiseFactorZ + permutationTable.zo;
            auto clampedZCoord = (int32_t) zCoord;
            if (zCoord < (double) clampedZCoord) {
                clampedZCoord--;
            }
            auto zBottoms = (uint8_t) ((uint32_t) clampedZCoord & 0xffu);
            zCoord -= clampedZCoord;
            t = zCoord * 6 - 15;
            w = (zCoord * t + 10);
            double fadeZ = zCoord * zCoord * zCoord * w;
            for (int Y = 0; Y < sizeY; Y++) {
                double yCoords = (y + (double) Y) * noiseFactorY + permutationTable.yo;
                auto clampedYCoords = (int32_t) yCoords;
                if (yCoords < (double) clampedYCoords) {
                    clampedYCoords--;
                }
                auto yBottoms = (uint8_t) ((uint32_t) clampedYCoords & 0xffu);
                yCoords -= clampedYCoords;
                t = yCoords * 6 - 15;
                w = yCoords * t + 10;
                double fadeY = yCoords * yCoords * yCoords * w;
                // ZCoord

                if (Y == 0 || yBottoms != i2) { // this is wrong on so many levels, same ybottoms doesnt mean x and z were the same...
                    i2 = yBottoms;
                    uint16_t k2 = permutations[permutations[xBottoms& 0xffu] + yBottoms] + zBottoms;
                    uint16_t l2 = permutations[permutations[xBottoms& 0xffu] + yBottoms + 1] + zBottoms;
                    uint16_t k3 = permutations[permutations[(xBottoms + 1)& 0xffu] + yBottoms] + zBottoms;
                    uint16_t l3 = permutations[permutations[(xBottoms + 1)& 0xffu] + yBottoms + 1] + zBottoms;
                    x1 = lerp(fadeX, grad(permutations[k2], xCoord, yCoords, zCoord), grad(permutations[k3], xCoord - 1.0, yCoords, zCoord));
                    x2 = lerp(fadeX, grad(permutations[l2], xCoord, yCoords - 1.0, zCoord), grad(permutations[l3], xCoord - 1.0, yCoords - 1.0, zCoord));
                    xx1 = lerp(fadeX, grad(permutations[k2 + 1], xCoord, yCoords, zCoord - 1.0), grad(permutations[k3 + 1], xCoord - 1.0, yCoords, zCoord - 1.0));
                    xx2 = lerp(fadeX, grad(permutations[l2 + 1], xCoord, yCoords - 1.0, zCoord - 1.0), grad(permutations[l3 + 1], xCoord - 1.0, yCoords - 1.0, zCoord - 1.0));
                }
                double y1 = lerp(fadeY, x1, x2);
                double y2 = lerp(fadeY, xx1, xx2);
                (*buffer)[columnIndex] = (*buffer)[columnIndex] + lerp(fadeZ, y1, y2) * octaveWidth;
                columnIndex++;
            }
        }
    }
}


static inline void generateNoise(double *buffer, double chunkX, double chunkY, double chunkZ, int sizeX, int sizeY, int sizeZ, double offsetX, double offsetY, double offsetZ, PermutationTable *permutationTable, int nbOctaves, int type) {
    memset(buffer, 0, sizeof(double) * sizeX * sizeZ * sizeY);
    double octavesFactor = 1.0;
    for (int octave = 0; octave < nbOctaves; octave++) {
        // optimized if 0
        if (type == 0) generatePermutations(&buffer, chunkX, chunkY, chunkZ, offsetX * octavesFactor, offsetY * octavesFactor, offsetZ * octavesFactor, octavesFactor, permutationTable[octave]);
        else generateNormalPermutations(&buffer, chunkX, chunkY, chunkZ, sizeX, sizeY, sizeZ, offsetX * octavesFactor, offsetY * octavesFactor, offsetZ * octavesFactor, octavesFactor, permutationTable[octave]);

        octavesFactor /= 2.0;
    }
}

static inline void generateFixedNoise(double *buffer, double chunkX, double chunkZ, int sizeX, int sizeZ, double offsetX, double offsetZ, PermutationTable *permutationTable, int nbOctaves) {
    memset(buffer, 0, sizeof(double) * sizeX * sizeZ);
    double octavesFactor = 1.0;
    for (int octave = 0; octave < nbOctaves; octave++) {
        generateFixedPermutations(&buffer, chunkX, chunkZ, sizeX, sizeZ, offsetX * octavesFactor, offsetZ * octavesFactor, octavesFactor, permutationTable[octave]);
        octavesFactor /= 2.0;
    }
}


static inline void fillNoiseColumn(double **NoiseColumn, int chunkX, int chunkZ, const double *temperature, const double *humidity, TerrainNoises terrainNoises) {

    // we only need
    // (60, 77, 145, 162, 61, 78, 146, 163)
    // (145, 162, 230, 247, 146, 163, 231, 248)
    // (230, 247, 315, 332, 231, 248, 316, 333)
    // (315, 332, 400, 417, 316, 333, 401, 418)
    // which is only 60-61, 77-78, 145-146, 162-163, 230-231, 247-248, 315-316, 332-333, 400-401, 417-418 // so 20
    // or as cellCounter 3,4,8,9,13,14,18,19,23,24
    // or as x indices 1 2 3 4 5 and fixed z to 3-4
    // 5 is the cellsize here and 17 the column size, they are inlined constants
    double d = 684.41200000000003;
    double d1 = 684.41200000000003;
    // this is super fast (but we only care about 3,4,8,9,13,14,18,19,23,24)
    auto *surfaceNoise = new double[5 * 5];
    auto *depthNoise = new double[5 * 5];
    generateFixedNoise(surfaceNoise, chunkX, chunkZ, 5, 5, 1.121, 1.121, terrainNoises.scale, 10);
    generateFixedNoise(depthNoise, chunkX, chunkZ, 5, 5, 200.0, 200.0, terrainNoises.depth, 16);

    auto *mainLimitPerlinNoise = new double[110];
    auto *minLimitPerlinNoise = new double[110];
    auto *maxLimitPerlinNoise = new double[110];
    // use optimized noise
    generateNoise(mainLimitPerlinNoise, chunkX, 0, chunkZ, 10, 11, 1, d / 80, d1 / 160, d / 80, terrainNoises.mainLimit, 8, 0);
    generateNoise(minLimitPerlinNoise, chunkX, 0, chunkZ, 10, 11, 1, d, d1, d, terrainNoises.minLimit, 16, 0);
    generateNoise(maxLimitPerlinNoise, chunkX, 0, chunkZ, 10, 11, 1, d, d1, d, terrainNoises.maxLimit, 16, 0);
    int possibleCellCounter[10] = {3, 4, 8, 9, 13, 14, 18, 19, 23, 24};
    int noiseIndex = 0;
    for (int indd = 0; indd < 10; indd++) {
        int cellCounter = possibleCellCounter[indd];
        int X = (cellCounter / 5) * 3 + 1; // 1 4 7 10 13
        int Z = (cellCounter % 5) * 3 + 1; // 7 13
        double aridityXZ = 1.0 - humidity[X * 16 + Z] * temperature[X * 16 + Z];
        aridityXZ *= aridityXZ;
        aridityXZ *= aridityXZ;
        aridityXZ = 1.0 - aridityXZ;  // 1-(1-X)*(1-X)*(1-X)*(1-X) with X=humidity*Temp
        double surface = (surfaceNoise[cellCounter] / 512.0 + 256.0 / 512.0) * aridityXZ;
        if (surface > 1.0) {
            surface = 1.0; // clamp
        }
        double depth = depthNoise[cellCounter] / 8000.0;
        if (depth < 0.0) {
            depth = -depth * 0.29999999999999999;
        }
        depth = depth * 3 - 2;
        if (depth < 0.0) {
            depth /= 2.0;
            if (depth < -1) {
                depth = -1;
            }
            depth /= 1.3999999999999999;
            depth /= 2.0;
            surface = 0.0;
        } else {
            if (depth > 1.0) {
                depth = 1.0;
            }
            depth /= 8.0;
        }
        if (surface < 0.0) {
            surface = 0.0;
        }
        surface += 0.5;
        depth = (depth * (double) 17) / 16.0;
        double depthColumn = (double) 17 / 2.0 + depth * 4.0;

        for (int column = 9; column < 11; column++) { // we only care at pos 9 and 10 in the column so 2 times
            double limit;
            double columnPerSurface = (((double) column - depthColumn) * 12.0) / surface;
            if (columnPerSurface < 0.0) {
                columnPerSurface *= 4.0;
            }
            double minLimit = minLimitPerlinNoise[indd * 11 + column] / 512.0;
            double maxLimit = maxLimitPerlinNoise[indd * 11 + column] / 512.0;
            double mainLimit = (mainLimitPerlinNoise[indd * 11 + column] / 10.0 + 1.0) / 2.0;
            if (mainLimit < 0.0) {
                limit = minLimit;
            } else if (mainLimit > 1.0) {
                limit = maxLimit;
            } else {
                limit = minLimit + (maxLimit - minLimit) * mainLimit; // interpolation
            }
            limit -= columnPerSurface;
            (*NoiseColumn)[noiseIndex++] = limit;
        }

    }
    delete[] surfaceNoise;
    delete[] depthNoise;
    delete[] mainLimitPerlinNoise;
    delete[] minLimitPerlinNoise;
    delete[] maxLimitPerlinNoise;
}

static inline void generateTerrain(int chunkX, int chunkZ, uint8_t **chunkCache, double *temperatures, double *humidity, TerrainNoises terrainNoises) {
    //uint8_t quadrant = 4;
    //uint8_t columnSize = 17;
    //uint8_t cellsize = 5;
    // we only need 60-61, 77-78, 145-146, 162-163, 230-231, 247-248, 315-316, 332-333, 400-401, 417-418 // so 20
    auto *NoiseColumn = new double[20];
    memset(NoiseColumn, 0, sizeof(double) * 20);
    fillNoiseColumn(&NoiseColumn, chunkX * 4, chunkZ * 4, temperatures, humidity, terrainNoises);
    for (uint8_t x = 0; x < 4; x++) {
        //uint8_t z = 3;
        double firstNoise_0_0 = NoiseColumn[x * 4]; //  (0,4,8,12)
        double firstNoise_0_1 = NoiseColumn[x * 4 + 2]; // (2,6,10,14)
        double firstNoise_1_0 = NoiseColumn[x * 4 + 4]; // (4,8,12,16)
        double firstNoise_1_1 = NoiseColumn[x * 4 + 6]; // (6,10,14,18)
        double stepFirstNoise_0_0 = (NoiseColumn[x * 4 + 1] - firstNoise_0_0) * 0.125;
        double stepFirstNoise_0_1 = (NoiseColumn[x * 4 + 3] - firstNoise_0_1) * 0.125;
        double stepFirstNoise_1_0 = (NoiseColumn[x * 4 + 5] - firstNoise_1_0) * 0.125;
        double stepFirstNoise_1_1 = (NoiseColumn[x * 4 + 7] - firstNoise_1_1) * 0.125;
        for (uint8_t heightOffset = 0; heightOffset < 8; heightOffset++) {
            double secondNoise_0_0 = firstNoise_0_0;
            double secondNoise_0_1 = firstNoise_0_1;
            double stepSecondNoise_1_0 = (firstNoise_1_0 - firstNoise_0_0) * 0.25;
            double stepSecondNoise_1_1 = (firstNoise_1_1 - firstNoise_0_1) * 0.25;
            for (uint8_t xOffset = 0; xOffset < 4; xOffset++) {
                double stoneLimit = secondNoise_0_0; // aka thirdNoise
                double stepThirdNoise_0_1 = (secondNoise_0_1 - secondNoise_0_0) * 0.25;
                for (uint8_t zOffset = 0; zOffset < 4; zOffset++) {
                    // we store 4*4 X 4 Z and 8 Y so indexes are 7 5 3 0
                    uint16_t index = x << 7u | xOffset << 5u | zOffset << 3u | heightOffset;
                    int block = AIR;
                    if (stoneLimit > 0.0) { //3d perlin condition
                        block = STONE;
                    }
                    (*chunkCache)[index] = block;
                    stoneLimit += stepThirdNoise_0_1;
                }

                secondNoise_0_0 += stepSecondNoise_1_0;
                secondNoise_0_1 += stepSecondNoise_1_1;
            }

            firstNoise_0_0 += stepFirstNoise_0_0;
            firstNoise_0_1 += stepFirstNoise_0_1;
            firstNoise_1_0 += stepFirstNoise_1_0;
            firstNoise_1_1 += stepFirstNoise_1_1;

        }
    }
    delete[]NoiseColumn;
}


static inline void replaceBlockForBiomes(int chunkX, int chunkZ, uint8_t **chunkCache, Random *worldRandom, TerrainNoises terrainNoises, uint8_t **chunkHeights) {
    auto *heightField = new double[16 * 16];
    generateNoise(heightField, chunkX * 16, chunkZ * 16, 0.0, 16, 16, 1, 0.03125 * 2.0, 0.03125 * 2.0, 0.03125 * 2.0, terrainNoises.surfaceElevation, 4, 1);

    for (uint8_t x = 0; x < 16; x++) {
        for (int k = 0; k < 12; k++) {
            advance6(worldRandom);
            // could be just advanced but i am afraid of nextInt
            for (int w = 0; w < 128; w++) {
                random_next_int(worldRandom, 5);
            }
        }
        for (uint8_t z = 12; z < 16; z++) {
            advance4(worldRandom);
            int elevation = (int) (heightField[x + z * 16] / 3.0 + 3.0 + next_double(worldRandom) * 0.25);
            int state = -1;
            for (uint8_t y = 79; y >= 72; y--) {
                int chunkCachePos = x << 5u | (z - OFFSET) << 3u | (y - 72);
                uint8_t previousBlock = (*chunkCache)[chunkCachePos];
                if (previousBlock == AIR) {
                    state = -1;
                    continue;
                }
                if (previousBlock != STONE) {
                    continue;
                }
                if (state == -1) { // AIR
                    if (elevation <= 0) { // if in a deep
                        // TODO remove that check to not pass the test and run in prod
                        (*chunkHeights)[x * 4 + (z - OFFSET)] = y;
                        break;
                    } else {
                        (*chunkHeights)[x * 4 + (z - OFFSET)] = y + 1;
                        break;
                    }
                }
            }
            // could be just advanced but i am afraid of nextInt
            for (int k = 0; k < 128; k++) {
                random_next_int(worldRandom, 5);
            }

        }
    }
    delete[] heightField;
}


static inline TerrainNoises *initTerrain(uint64_t worldSeed) {
    auto *terrainNoises = new TerrainNoises;
    Random worldRandom = get_random(worldSeed);
    PermutationTable *octaves = terrainNoises->minLimit;
    initOctaves(octaves, &worldRandom, 16);
    octaves = terrainNoises->maxLimit;
    initOctaves(octaves, &worldRandom, 16);
    octaves = terrainNoises->mainLimit;
    initOctaves(octaves, &worldRandom, 8);
    for (int j = 0; j < 4; j++) { //shore and river composition
        advance6(&worldRandom);
        // could be just advanced but i am afraid of nextInt
        uint8_t i = 0u;
        do {
            random_next_int(&worldRandom, 256u - i);
        } while (i++ != 255);
    }
    octaves = terrainNoises->surfaceElevation;
    initOctaves(octaves, &worldRandom, 4);
    octaves = terrainNoises->scale;
    initOctaves(octaves, &worldRandom, 10);
    octaves = terrainNoises->depth;
    initOctaves(octaves, &worldRandom, 16);
    return terrainNoises;
}

static inline uint8_t *provideChunk(int chunkX, int chunkZ, BiomeResult *biomeResult, TerrainNoises *terrainNoises) {
    Random worldRandom = get_random((uint64_t) ((long) chunkX * 0x4f9939f508L + (long) chunkZ * 0x1ef1565bd5L));
    auto *chunkCache = new uint8_t[64 * 8];
    generateTerrain(chunkX, chunkZ, &chunkCache, biomeResult->temperature, biomeResult->humidity, *terrainNoises);
    auto *chunkHeights = new uint8_t[64];
    memset(chunkHeights, 0, 64);
    replaceBlockForBiomes(chunkX, chunkZ, &chunkCache, &worldRandom, *terrainNoises, &chunkHeights);
    delete[] chunkCache;
    return chunkHeights;
}

void delete_terrain_result(TerrainResult *terrainResult) {
    delete[] terrainResult->chunkHeights;
    delete_biome_result(terrainResult->biomeResult);
    delete terrainResult;
}

uint8_t *TerrainInternalWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ, BiomeResult *biomeResult) {
    TerrainNoises *terrainNoises = initTerrain(worldSeed);
    uint8_t *chunkHeights = provideChunk(chunkX, chunkZ, biomeResult, terrainNoises);
    delete terrainNoises;
    return chunkHeights;
}

uint8_t *TerrainHeights(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ, BiomeResult *biomeResult) {
    TerrainNoises *terrainNoises = initTerrain(worldSeed);
    auto *chunkHeights = provideChunk(chunkX, chunkZ, biomeResult, terrainNoises);
    delete[] terrainNoises;
    return chunkHeights;
}


TerrainResult *TerrainWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
    BiomeResult *biomeResult = BiomeWrapper(worldSeed, chunkX, chunkZ);
    auto *chunkHeights = TerrainInternalWrapper(worldSeed, chunkX, chunkZ, biomeResult);
    auto *terrainResult = new TerrainResult;
    terrainResult->biomeResult = biomeResult;
    terrainResult->chunkHeights = chunkHeights;
    return terrainResult;
}

static void printHeights(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
    auto *terrainResult = TerrainWrapper(worldSeed, chunkX, chunkZ);
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 4; ++z) {
            std::cout << (int) terrainResult->chunkHeights[x * 4 + z] << " ";
        }
        std::cout << std::endl;
    }
}


void filterDownSeeds(const uint64_t *worldSeeds, int32_t chunkX, uint64_t nbSeeds) {
    uint8_t mapWat[] = {77, 78, 77, 75}; // from z 12 to z15 in chunk
    int chunkZ = -3;
    for (uint64_t i = 0; i < nbSeeds; ++i) {
        uint64_t seed = worldSeeds[i];
        BiomeResult *biomeResult = BiomeWrapper(seed, chunkX, chunkZ);
        bool skipIt = false;
        for (int j = 0; j < 16 * 16; ++j) {
            Biomes biome = biomeResult->biomes[j];
            if (biome == Rainforest || biome == Swampland || biome == Savanna || biome == Taiga || biome == Desert || biome == IceDesert || biome == Tundra) {
                skipIt = true;
                break;
            }
        }
        if (skipIt) {
            delete_biome_result(biomeResult);
            continue;
        }
        uint8_t *chunkHeights = TerrainInternalWrapper(seed, chunkX, chunkZ, biomeResult);
        for (int x = 0; x < 16; x++) {
            bool flag = true;
            for (uint8_t z = 0; z < (16 - OFFSET); z++) {
                if (chunkHeights[x * 4 + z] != mapWat[z]) {
                    flag = false;
                    break;
                }

            }
            if (flag) {
                std::cout << "Found seed: " << seed << " at x(relative): " << chunkX * 16 + 8 << " and z: -30" << std::endl;
            }
        }
        delete[] chunkHeights;
        delete_biome_result(biomeResult);
    }

}

std::istream &safeGetline(std::istream &is, std::string &t) {
    t.clear();
    std::istream::sentry se(is, true);
    std::streambuf *sb = is.rdbuf();
    for (;;) {
        int c = sb->sbumpc();
        switch (c) {
            case '\n':
                return is;
            case '\r':
                if (sb->sgetc() == '\n')
                    sb->sbumpc();
                return is;
            case std::streambuf::traits_type::eof():
                // Also handle the case when the last line has no line ending
                if (t.empty())
                    is.setstate(std::ios::eofbit);
                return is;
            default:
                t += (char) c;
        }
    }
}

int main(int argc, char *argv[]) {
    std::ifstream file("test.txt");
    if (!file.is_open()) {
        std::cout << "file was not loaded" << std::endl;
        throw std::runtime_error("file was not loaded");
    }
    uint64_t length = std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');
    file.seekg(file.beg);
    auto *worldSeeds = new uint64_t[length + 1];
    std::string line;
    size_t sz;
    uint64_t seed;
    uint64_t index = 0;
    while (safeGetline(file, line).good()) {
        errno = 0;
        try {
            seed = std::stoull(line, &sz, 10);
            if (sz != line.size()) {
                fprintf(stderr, "Size of parsed and size of line disagree, wrong type\n");
                exit(EXIT_FAILURE);
            }
        } catch (const std::invalid_argument &) {
            std::cout << "Error conversion" << std::endl;
            perror("strtol");
            exit(EXIT_FAILURE);
        } catch (const std::out_of_range &) {
            std::cout << "Out of range" << std::endl;
            exit(EXIT_FAILURE);
        }
        worldSeeds[index++] = seed;
    }
    file.close();
    std::cout << "Running " << length + 1 << " seeds" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    filterDownSeeds(worldSeeds, 6, length);
    auto finish = std::chrono::high_resolution_clock::now();
    uint64_t time = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / 1e9;
    std::cout << time << " s\n";
    std::cout << "Checked " << (length + 1) * 16 << " couple coordinates/seed or " << (length + 1) * 16 / time << " seedCoords/s" << std::endl;

    delete[] worldSeeds;
}

//Running 100000 seeds
//Found seed: 90389547180974 at x: 99 and z:-30
//Found seed: 171351315692858 at x: 99 and z:-30
//Found seed: 189587791856572 at x: 99 and z:-30
//Found seed: 66697851806768 at x: 99 and z:-30
//Found seed: 162899168234811 at x: 99 and z:-30
//31.9951 s