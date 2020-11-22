#include <iostream>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <sstream>

static_assert(std::numeric_limits<double>::is_iec559, "This code requires IEEE-754 doubles");

#define Random uint64_t
#define RANDOM_MULTIPLIER 0x5DEECE66DULL
#define RANDOM_ADDEND 0xBULL
#define RANDOM_MASK ((1ULL << 48u) - 1)
#define RANDOM_SCALE 0x1.0p-53
#define get_random(seed) ((Random)((seed ^ RANDOM_MULTIPLIER) & RANDOM_MASK))

struct data {
    uint64_t seed;
    int32_t x;
    int32_t z;
};

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
            uint32_t randomIndex = random_next_int(random, 256u - index) + index;
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

struct TerrainResult {
    uint8_t *chunkCache;
    uint8_t *chunkHeights;
};

struct TerrainNoises {
    PermutationTable minLimit[16];
    PermutationTable maxLimit[16];
    PermutationTable mainLimit[8];
    PermutationTable shoresBottomComposition[4];
    PermutationTable surfaceElevation[4];
    PermutationTable scale[10];
    PermutationTable depth[16];
    //PermutationTable forest[8];
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

void delete_terrain_result(TerrainResult *terrainResult);

uint8_t *TerrainInternalWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ);

TerrainResult *TerrainWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ);

uint8_t *TerrainHeights(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ);

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
        auto xBottoms = (int32_t) ((uint32_t) clampedXCoord & 0xffu);
        xCoord -= clampedXCoord;
        double fadeX = xCoord * xCoord * xCoord * (xCoord * (xCoord * 6.0 - 15.0) + 10.0);
        for (int Z = 0; Z < sizeZ; Z++) {
            double zCoord = (z + (double) Z) * noiseFactorZ + permutationTable.zo;
            int clampedZCoord = (int) zCoord;
            if (zCoord < (double) clampedZCoord) {
                clampedZCoord--;
            }
            auto zBottoms = (int32_t) ((uint32_t) clampedZCoord & 0xffu);
            zCoord -= clampedZCoord;
            double fadeZ = zCoord * zCoord * zCoord * (zCoord * (zCoord * 6.0 - 15.0) + 10.0);
            int hashXZ = permutations[permutations[xBottoms]] + zBottoms;
            int hashOffXZ = permutations[permutations[xBottoms + 1]] + zBottoms;
            double x1 = lerp(fadeX, grad2D(permutations[hashXZ], xCoord, zCoord), grad2D(permutations[hashOffXZ], xCoord - 1.0, zCoord));
            double x2 = lerp(fadeX, grad2D(permutations[hashXZ + 1], xCoord, zCoord - 1.0), grad2D(permutations[hashOffXZ + 1], xCoord - 1.0, zCoord - 1.0));
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
}

static inline void generateNoise(double *buffer, double chunkX, double chunkY, double chunkZ, int sizeX, int sizeY, int sizeZ, double offsetX, double offsetY, double offsetZ, PermutationTable *permutationTable, int nbOctaves) {
    memset(buffer, 0, sizeof(double) * sizeX * sizeZ * sizeY);
    double octavesFactor = 1.0;
    for (int octave = 0; octave < nbOctaves; octave++) {
        generateNormalPermutations(&buffer, chunkX, chunkY, chunkZ, sizeX, sizeY, sizeZ, offsetX * octavesFactor, offsetY * octavesFactor, offsetZ * octavesFactor, octavesFactor, permutationTable[octave]);
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

static inline void fillNoiseColumn(double **NoiseColumn, int chunkX, int chunkZ, TerrainNoises terrainNoises) {
    int cellSizeX = 5;
    int columnSize = 17;
    int cellSizeZ = 5;
    double noiseSize = 684.412;

    auto *surfaceNoise = new double[cellSizeX * cellSizeZ];
    generateFixedNoise(surfaceNoise, chunkX, chunkZ, cellSizeX, cellSizeZ, 1.0, 1.0, terrainNoises.scale, 10);
    auto *depthNoise = new double[cellSizeX * cellSizeZ];
    generateFixedNoise(depthNoise, chunkX, chunkZ, cellSizeX, cellSizeZ, 100.0, 100.0, terrainNoises.depth, 16);

    auto *mainLimitPerlinNoise = new double[5 * 17 * 5];
    auto *minLimitPerlinNoise = new double[5 * 17 * 5];
    auto *maxLimitPerlinNoise = new double[5 * 17 * 5];
    // use optimized noise
    generateNoise(mainLimitPerlinNoise, chunkX, 0, chunkZ, cellSizeX, columnSize, cellSizeZ, noiseSize / 80, noiseSize / 160, noiseSize / 80, terrainNoises.mainLimit, 8);
    generateNoise(minLimitPerlinNoise, chunkX, 0, chunkZ, cellSizeX, columnSize, cellSizeZ, noiseSize, noiseSize, noiseSize, terrainNoises.minLimit, 16);
    generateNoise(maxLimitPerlinNoise, chunkX, 0, chunkZ, cellSizeX, columnSize, cellSizeZ, noiseSize, noiseSize, noiseSize, terrainNoises.maxLimit, 16);
    int index2d = 0;
    int index3d = 0;
    for (int cellX = 0; cellX < cellSizeX; ++cellX) {
        for (int cellZ = 0; cellZ < cellSizeZ; ++cellZ) {
            double surface = (surfaceNoise[index2d] / 512.0 + 256.0 / 512.0);
            if (surface > 1.0) {
                surface = 1.0; // clamp
            }
            double depth = depthNoise[index2d] / 8000.0;
            if (depth < 0.0) {
                depth = -depth;
            }
            depth = depth * 3.0 - 3.0;
            if (depth < 0.0) {
                depth /= 2.0;
                if (depth < -1.0) {
                    depth = -1.0;
                }
                depth /= 1.4;
                depth /= 2.0;
                surface = 0.0;
            } else {
                if (depth > 1.0) {
                    depth = 1.0;
                }
                depth /= 6.0;
            }
            surface += 0.5;
            depth = (depth * (double) columnSize) / 16.0;
            double depthColumn = (double) columnSize / 2.0 + depth * 4.0;
            ++index2d;
            for (int cellY = 0; cellY < columnSize; cellY++) {
                double limit;
                double columnPerSurface = (((double) cellY - depthColumn) * 12.0) / surface;
                if (columnPerSurface < 0.0) {
                    columnPerSurface *= 4.0;
                }
                double minLimit = minLimitPerlinNoise[index3d] / 512.0;
                double maxLimit = maxLimitPerlinNoise[index3d] / 512.0;
                double mainLimit = (mainLimitPerlinNoise[index3d] / 10.0 + 1.0) / 2.0;
                if (mainLimit < 0.0) {
                    limit = minLimit;
                } else if (mainLimit > 1.0) {
                    limit = maxLimit;
                } else {
                    limit = minLimit + (maxLimit - minLimit) * mainLimit; // interpolation
                }
                limit -= columnPerSurface;
                double correction;
                if (cellY > columnSize - 4) {
                    correction = (float) (cellY - (columnSize - 4)) / 3.0f;
                    limit = limit * (1.0 - correction) + -10.0 * correction;
                }
                if ((double) cellY < 0.0) {
                    correction = (0.0 - (double) cellY) / 4.0;
                    if (correction < 0.0) {
                        correction = 0.0;
                    }
                    if (correction > 1.0) {
                        correction = 1.0;
                    }
                    limit = limit * (1.0 - correction) + -10.0 * correction;
                }
                (*NoiseColumn)[index3d] = limit;
                ++index3d;
            }
        }
    }
    delete[] surfaceNoise;
    delete[] depthNoise;
    delete[] mainLimitPerlinNoise;
    delete[] minLimitPerlinNoise;
    delete[] maxLimitPerlinNoise;
}

static inline void generateTerrain(int chunkX, int chunkZ, uint8_t **chunkCache, TerrainNoises terrainNoises) {
    uint8_t quadrant = 4;
    uint8_t columnSize = 17;
    uint8_t cellsize = 5;
    int seaLevel = 64;
    double interpFirst = 0.125;
    double interpSecond = 0.25;
    double interpThird = 0.25;
    auto *NoiseColumn = new double[cellsize * cellsize * columnSize];
    memset(NoiseColumn, 0, sizeof(double) * cellsize * cellsize * columnSize);
    fillNoiseColumn(&NoiseColumn, chunkX * quadrant, chunkZ * quadrant, terrainNoises);
    for (uint8_t x = 0; x < quadrant; x++) {
        for (uint8_t z = 0; z < quadrant; ++z) {
            for (uint8_t height = 0; height < columnSize - 1; height++) {
                int off_0_0 = x * cellsize + z;
                int off_0_1 = x * cellsize + (z + 1);
                int off_1_0 = (x + 1) * cellsize + z;
                int off_1_1 = (x + 1) * cellsize + (z + 1);
                double firstNoise_0_0 = NoiseColumn[(off_0_0) * columnSize + (height)];
                double firstNoise_0_1 = NoiseColumn[(off_0_1) * columnSize + (height)];
                double firstNoise_1_0 = NoiseColumn[off_1_0 * columnSize + (height)];
                double firstNoise_1_1 = NoiseColumn[off_1_1 * columnSize + (height)];
                double stepFirstNoise_0_0 = (NoiseColumn[(off_0_0) * columnSize + (height + 1)] - firstNoise_0_0) * interpFirst;
                double stepFirstNoise_0_1 = (NoiseColumn[(off_0_1) * columnSize + (height + 1)] - firstNoise_0_1) * interpFirst;
                double stepFirstNoise_1_0 = (NoiseColumn[off_1_0 * columnSize + (height + 1)] - firstNoise_1_0) * interpFirst;
                double stepFirstNoise_1_1 = (NoiseColumn[off_1_1 * columnSize + (height + 1)] - firstNoise_1_1) * interpFirst;
                for (uint8_t heightOffset = 0; heightOffset < 8; heightOffset++) {
                    double secondNoise_0_0 = firstNoise_0_0;
                    double secondNoise_0_1 = firstNoise_0_1;
                    double stepSecondNoise_1_0 = (firstNoise_1_0 - firstNoise_0_0) * interpSecond;
                    double stepSecondNoise_1_1 = (firstNoise_1_1 - firstNoise_0_1) * interpSecond;
                    for (uint8_t xOffset = 0; xOffset < 4; xOffset++) {
                        uint8_t currentHeight = height * 8u + heightOffset; // max is 128
                        uint16_t index = (xOffset + x * 4u) << 11u | (z * 4u) << 7u | currentHeight;
                        double stoneLimit = secondNoise_0_0; // aka thirdNoise
                        double stepThirdNoise_0_1 = (secondNoise_0_1 - secondNoise_0_0) * interpThird;
                        for (int zOffset = 0; zOffset < 4; zOffset++) {
                            int block = AIR;
                            if (currentHeight < seaLevel) {
                                block = MOVING_WATER;
                            }
                            if (stoneLimit > 0.0) { //3d perlin condition
                                block = STONE;
                            }
                            (*chunkCache)[index] = block;
                            index += 128;
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
        }
    }
    delete[]NoiseColumn;
}

static inline void replaceBlockForBiomes(int chunkX, int chunkZ, uint8_t **chunkCache, Random *worldRandom, TerrainNoises terrainNoises) {
    uint8_t oceanLevel = 64;
    uint8_t MIN = 0;
    double noiseFactor = 0.03125;
    auto *sandFields = new double[16 * 16];
    auto *gravelField = new double[16 * 16];
    auto *heightField = new double[16 * 16];
    generateNoise(sandFields, chunkX * 16, chunkZ * 16, 0.0, 16, 16, 1, noiseFactor, noiseFactor, 1.0, terrainNoises.shoresBottomComposition, 4);
    // beware this error in alpha ;)
    generateNoise(gravelField, chunkZ * 16, 109.0134, chunkX * 16, 16, 1, 16, noiseFactor, 1.0, noiseFactor, terrainNoises.shoresBottomComposition, 4);
    generateNoise(heightField, chunkX * 16, chunkZ * 16, 0.0, 16, 16, 1, noiseFactor * 2.0, noiseFactor * 2.0, noiseFactor * 2.0, terrainNoises.surfaceElevation, 4);

    for (int x = 0; x < 16; x++) {
        // in case you need to advance
        //for (int k = 0; k < WWW; k++) {
        //    next_double(worldRandom);
        //    next_double(worldRandom);
        //    next_double(worldRandom);
        //    for (int w = 0; w < 128; w++) {
        //        random_next_int(worldRandom, 6);
        //    }
        //}
        for (int z = 0; z < 16; z++) {
            bool sandy = sandFields[x + z * 16] + next_double(worldRandom) * 0.2 > 0.0;
            bool gravelly = gravelField[x + z * 16] + next_double(worldRandom) * 0.2 > 3;
            int elevation = (int) (heightField[x + z * 16] / 3.0 + 3.0 + next_double(worldRandom) * 0.25);
            int state = -1;
            uint8_t upperBlock = GRASS;
            uint8_t lowerBlock = DIRT;
            for (int y = 127; y >= MIN; y--) {
                int chunkCachePos = (x * 16 + z) * 128 + y;
                uint8_t previousBlock = (*chunkCache)[chunkCachePos];
                // we dont generate bedrock here, see end of loop
                if (previousBlock == 0) {
                    state = -1;
                    continue;
                }
                if (previousBlock != STONE) {
                    continue;
                }
                if (state == -1) { // AIR
                    if (elevation <= 0) { // if in a deep
                        upperBlock = 0;
                        lowerBlock = STONE;
                    } else if (y >= oceanLevel - 4 && y <= oceanLevel + 1) { // if at sea level do the shore and rivers
                        upperBlock = GRASS;
                        lowerBlock = DIRT;
                    }
                    if (gravelly) {
                        upperBlock = 0;
                    }
                    if (gravelly) {
                        lowerBlock = GRAVEL;
                    }
                    if (sandy) {
                        upperBlock = SAND;
                        lowerBlock = SAND;
                    }
                }
                state = elevation;
                if (y >= oceanLevel - 1) {
                    (*chunkCache)[chunkCachePos] = upperBlock;
                    continue;
                    (*chunkCache)[chunkCachePos] = upperBlock;
                    continue;
                }
                if (state > 0) {
                    state--;
                    (*chunkCache)[chunkCachePos] = lowerBlock;

                }
            }
            for (int k = 0; k < 128; k++) {
                random_next_int(worldRandom, 6); // can be done with and advance
            }

        }
    }
    delete[] sandFields;
    delete[] gravelField;
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
    octaves = terrainNoises->shoresBottomComposition;
    initOctaves(octaves, &worldRandom, 4);
    octaves = terrainNoises->surfaceElevation;
    initOctaves(octaves, &worldRandom, 4);
    octaves = terrainNoises->scale;
    initOctaves(octaves, &worldRandom, 10);
    octaves = terrainNoises->depth;
    initOctaves(octaves, &worldRandom, 16);
    // not needed
    //octaves = terrainNoises->forest;
    //initOctaves(octaves, &worldRandom, 8);
    return terrainNoises;
}

static inline uint8_t *provideChunk(int chunkX, int chunkZ, TerrainNoises *terrainNoises) {
    Random worldRandom = get_random((uint64_t) ((long) chunkX * 341873128712L + (long) chunkZ * 132897987541L));
    auto *chunkCache = new uint8_t[32768];
    generateTerrain(chunkX, chunkZ, &chunkCache, *terrainNoises);
    replaceBlockForBiomes(chunkX, chunkZ, &chunkCache, &worldRandom, *terrainNoises);
    return chunkCache;
}

void delete_terrain_result(TerrainResult *terrainResult) {
    delete[] terrainResult->chunkCache;
    delete[] terrainResult->chunkHeights;
    delete terrainResult;
}

uint8_t *TerrainInternalWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
    TerrainNoises *terrainNoises = initTerrain(worldSeed);
    uint8_t *chunkCache = provideChunk(chunkX, chunkZ, terrainNoises);
    delete terrainNoises;
    return chunkCache;
}

uint8_t *TerrainHeights(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
    TerrainNoises *terrainNoises = initTerrain(worldSeed);
    auto *chunkCache = provideChunk(chunkX, chunkZ, terrainNoises);
    delete[] terrainNoises;
    auto *chunkHeights = new uint8_t[4 * 16];
    for (int x = 0; x < 16; ++x) {
        for (int z = 12; z < 16; ++z) {
            int pos = 128 * x * 16 + 128 * z;
            int y;
            for (y = 80; y >= 70 && chunkCache[pos + y] == AIR; y--);
            chunkHeights[x * 4 + (z - 12)] = (y + 1);
        }
    }
    delete[] chunkCache;
    return chunkHeights;
}

TerrainResult *TerrainWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
    auto *chunkCache = TerrainInternalWrapper(worldSeed, chunkX, chunkZ);
    auto *chunkHeights = new uint8_t[16 * 16];
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            int pos = 128 * x * 16 + 128 * z;
            int y;
            for (y = 128; y >= 0 && chunkCache[pos + y] == 0; y--);
            //std::cout<<(y + 1)<<" ";
            chunkHeights[x * 16 + z] = y;
        }
        //std::cout<<std::endl;
    }
    auto *terrainResult = new TerrainResult;
    terrainResult->chunkHeights = chunkHeights;
    terrainResult->chunkCache = chunkCache;
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

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

void filterDownSeeds(const data *data, int32_t posX, int32_t posZ, int64_t nbSeeds) {


#define OFFSET_X_PLUS 2
#define OFFSET_X_NEG -2
#define OFFSET_Z -17

// X negative ->
// Y, Y+1, Y+2, Y+3, Y+1
    int8_t mapX[] = {-1,0,0,-1};

    for (int i = 0; i < nbSeeds; ++i) {
        uint8_t *chunkCache2 = nullptr;

        bool shouldDoExtraChunkLower = false;
        bool shoulDoExtraChunkUpper = false;
        int64_t seed = data[i].seed;
        int32_t chunkX = data[i].x / 16;
        if (chunkX < 0) {
            chunkX--;
        }
        uint8_t chunkPosX = ((data[i].x % 16) + 16) % 16;
        if (chunkPosX < OFFSET_X_PLUS - OFFSET_X_NEG + 1) { // 5 here
            shouldDoExtraChunkLower = true;
        }
        if (chunkPosX > 16 - OFFSET_X_PLUS - OFFSET_X_NEG) { // 12 here
            shoulDoExtraChunkUpper = true;
        }
        int32_t chunkZ = (data[i].z + OFFSET_Z) / 16;
        uint8_t chunkPosZ = (((data[i].z + OFFSET_Z) % 16) + 16) % 16;
        if (chunkZ < 0) {
            chunkZ--;
        }
        uint8_t *chunkCache = TerrainInternalWrapper(seed, chunkX, chunkZ);
        if (shouldDoExtraChunkLower) {
            chunkCache2 = TerrainInternalWrapper(seed, chunkX - 1, chunkZ);
        }
        if (shoulDoExtraChunkUpper) {
            chunkCache2 = TerrainInternalWrapper(seed, chunkX + 1, chunkZ);
        }
        if (i % 1000 == 0) {
            std::cout << i << std::endl;
        }
        uint32_t lastY = 0;
        int index = 0;
        int count = 0;
        uint8_t z = chunkPosZ; // fixed Z
        int32_t diff;
        if (shouldDoExtraChunkLower) {
            for (uint8_t x = chunkPosX + OFFSET_X_NEG + 16; x <= 15u; x++) {
                uint32_t pos = 128 * x * 16 + 128 * z;
                uint32_t y;

                for (y = 128; y >= 0 && chunkCache2[pos + y] == 0; y--);
                if (lastY == 0) {
                    lastY = y;
                } else {
                    diff = lastY - y;
                    if (diff == mapX[index++]) {
                        count++;
                    }
                }
            }
        }
        // making sure to clamp to the main chunk
        for (uint8_t x = MAX(chunkPosX + OFFSET_X_NEG, 0); x <= MIN((uint8_t) (chunkPosX + OFFSET_X_PLUS), (uint8_t) 15u); x++) {
            uint32_t pos = 128 * x * 16 + 128 * z;
            uint32_t y;
            for (y = 128; y >= 0 && chunkCache[pos + y] == 0; y--);
            if (lastY == 0) {
                lastY = y;
            } else {
                diff = lastY - y;
                if (diff == mapX[index++]) {
                    count++;
                }
            }

        }
        if (shoulDoExtraChunkUpper) {
            for (uint8_t x = 0; x <= (uint8_t) (chunkPosX + OFFSET_X_PLUS - 16u); x++) {
                uint32_t pos = 128 * x * 16 + 128 * z;
                uint32_t y;
                for (y = 128; y >= 0 && chunkCache2[pos + y] == 0; y--);
                if (lastY == 0) {
                    lastY = y;
                } else {
                    diff = lastY - y;
                    if (diff == mapX[index++]) {
                        count++;
                    }
                }
            }
        }

        if (count >= 1) {
            std::cout << "Found seed: " <<  data->seed << " at x: " << data->x << " and z:" << data->z <<std::endl;
        }
        delete[] chunkCache;
        delete[] chunkCache2;

    }

}

int main(int argc, char *argv[]) {

    std::ifstream file("seeds.csv");
    if (!file.is_open()) {
        std::cout << "file was not loaded" << std::endl;
        throw std::runtime_error("file was not loaded");
    }

    auto start_file = std::chrono::steady_clock::now();
    int64_t startValue  = 1282613228000;
    int64_t total       = 1282706397225;
    int64_t amount = total - startValue;
    auto *worldSeeds = new data[amount];
    int64_t hardcoded = 8682522807148012LL * 181783497276652981LL;
    for(int64_t offset = 0; offset < amount; offset++){
        int64_t value = startValue + offset;
        value *=1000;
        int64_t xorValue = hardcoded ^ value;
        worldSeeds[offset].seed = xorValue^RANDOM_MULTIPLIER;
        worldSeeds[offset].x = x;
        worldSeeds[offset].z = z;

    }

    auto end_file = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_file - start_file;
    std::cout << "Took  " << elapsed_seconds.count() << "s to parse the file\n";
    file.close();
    std::cout << "Running " << amount << " seeds" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    filterDownSeeds(worldSeeds, 0, 0, amount);
    auto finish = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / 1e9 << " s\n";
    delete[] worldSeeds;
}
