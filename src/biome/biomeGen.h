#include "../utilities/perlinCommon.h"
#ifndef BIOME_GEN_HEADER
#define BIOME_GEN_HEADER

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

BiomeResult *BiomeWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ);

void delete_biome_result(BiomeResult *biomeResult);

#endif //BIOME_GEN_HEADER
