#include <iostream>
#include <chrono>
#include <cstring>
#include "../biome/biomeGen.h"
#include "heightmapGen.h"

static_assert(std::numeric_limits<double>::is_iec559, "This code requires IEEE-754 doubles");

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
static inline double grad2D(uint8_t hash,double x,double z){
    return grad(hash,x,0,z);
}

//we care only about 60-61, 77-78, 145-146, 162-163, 230-231, 247-248, 315-316, 332-333, 400-401, 417-418
static inline void generatePermutations(double **buffer, double x, double y, double z, int sizeX, int sizeY, int sizeZ, double noiseFactorX, double noiseFactorY, double noiseFactorZ, double octaveSize, PermutationTable permutationTable) {
    uint8_t *permutations = permutationTable.permutations;
    double octaveWidth = 1.0 / octaveSize;
    int32_t i2 = -1;
    double x1 = 0.0;
    double x2 = 0.0;
    double xx1 = 0.0;
    double xx2 = 0.0;
    double t;
    double w;
    int columnIndex = 51; // possibleX[0]*5*17+3*17
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
        if (index % 2 == 0) {
            columnIndex += 6; // 6 to complete Y
        } else {
            columnIndex += possibleZ[0] * 17 + 6; // 3*17 on Z +6 complete Y
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
        double xCoord = (x + (double) X)  * noiseFactorX + permutationTable.xo;
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


static inline void generateNoise(double *buffer, double chunkX, double chunkY, double chunkZ, int sizeX, int sizeY, int sizeZ, double offsetX, double offsetY, double offsetZ, PermutationTable *permutationTable, int nbOctaves,int type) {
    memset(buffer,0,sizeof(double)*sizeX * sizeZ*sizeY);
    double octavesFactor = 1.0;
    for (int octave = 0; octave < nbOctaves; octave++) {
        // optimized if 0
        if (type==0) generatePermutations(&buffer, chunkX, chunkY, chunkZ, sizeX, sizeY, sizeZ, offsetX * octavesFactor, offsetY * octavesFactor, offsetZ * octavesFactor, octavesFactor, permutationTable[octave]);
        else generateNormalPermutations(&buffer, chunkX, chunkY, chunkZ, sizeX, sizeY, sizeZ, offsetX * octavesFactor, offsetY * octavesFactor, offsetZ * octavesFactor, octavesFactor, permutationTable[octave]);

        octavesFactor /= 2.0;
    }
}

static inline void generateFixedNoise(double *buffer, double chunkX, double chunkZ, int sizeX, int sizeZ, double offsetX, double offsetZ, PermutationTable *permutationTable, int nbOctaves) {
    memset(buffer,0,sizeof(double)*sizeX * sizeZ);
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

    auto *mainLimitPerlinNoise = new double[5 * 17 * 5];
    auto *minLimitPerlinNoise = new double[5 * 17 * 5];
    auto *maxLimitPerlinNoise = new double[5 * 17 * 5];
    // use optimized noise
    generateNoise(mainLimitPerlinNoise, chunkX, 0, chunkZ, 5, 17, 5, d / 80, d1 / 160, d / 80, terrainNoises.mainLimit, 8,0);
    generateNoise(minLimitPerlinNoise, chunkX, 0, chunkZ, 5, 17, 5, d, d1, d, terrainNoises.minLimit, 16,0);
    generateNoise(maxLimitPerlinNoise, chunkX, 0, chunkZ, 5, 17, 5, d, d1, d, terrainNoises.maxLimit, 16,0);
    int possibleCellCounter[10] = {3, 4, 8, 9, 13, 14, 18, 19, 23, 24};
    for (int cellCounter : possibleCellCounter) {
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
            int columnCounter = cellCounter * 17 + column;
            double limit;
            double columnPerSurface = (((double) column - depthColumn) * 12.0) / surface;
            if (columnPerSurface < 0.0) {
                columnPerSurface *= 4.0;
            }
            double minLimit = minLimitPerlinNoise[columnCounter] / 512.0;
            double maxLimit = maxLimitPerlinNoise[columnCounter] / 512.0;
            double mainLimit = (mainLimitPerlinNoise[columnCounter] / 10.0 + 1.0) / 2.0;
            if (mainLimit < 0.0) {
                limit = minLimit;
            } else if (mainLimit > 1.0) {
                limit = maxLimit;
            } else {
                limit = minLimit + (maxLimit - minLimit) * mainLimit; // interpolation
            }
            limit -= columnPerSurface;
            (*NoiseColumn)[columnCounter] = limit;
        }

    }
    delete[] surfaceNoise;
    delete[] depthNoise;
    delete[] mainLimitPerlinNoise;
    delete[] minLimitPerlinNoise;
    delete[] maxLimitPerlinNoise;
}

static inline void generateTerrain(int chunkX, int chunkZ, uint8_t **chunkCache, double *temperatures, double *humidity, TerrainNoises terrainNoises) {
    uint8_t quadrant = 4;
    uint8_t columnSize = 17;
    uint8_t cellsize = 5;
    double interpFirstOctave = 0.125;
    double interpSecondOctave = 0.25;
    double interpThirdOctave = 0.25;
    // we only need 315 332 400 417 316 333 401 and 418
    auto *NoiseColumn = new double[5 * 5 * 17];
    memset(NoiseColumn, 0, sizeof(double ) * 5 * 5 * 17);
    fillNoiseColumn(&NoiseColumn, chunkX * quadrant, chunkZ * quadrant, temperatures, humidity, terrainNoises);
    for (uint8_t x = 0; x < quadrant; x++) {
        uint8_t z = 3;
        for (uint8_t height = 9; height < 10; height++) {
            int off_0_0 = x * cellsize + z;
            int off_0_1 = x * cellsize + (z + 1);
            int off_1_0 = (x + 1) * cellsize + z;
            int off_1_1 = (x + 1) * cellsize + (z + 1);
            double firstNoise_0_0 = NoiseColumn[(off_0_0) * columnSize + (height)];
            double firstNoise_0_1 = NoiseColumn[(off_0_1) * columnSize + (height)];
            double firstNoise_1_0 = NoiseColumn[off_1_0 * columnSize + (height)];
            double firstNoise_1_1 = NoiseColumn[off_1_1 * columnSize + (height)];
            double stepFirstNoise_0_0 = (NoiseColumn[(off_0_0) * columnSize + (height + 1)] - firstNoise_0_0) * interpFirstOctave;
            double stepFirstNoise_0_1 = (NoiseColumn[(off_0_1) * columnSize + (height + 1)] - firstNoise_0_1) * interpFirstOctave;
            double stepFirstNoise_1_0 = (NoiseColumn[off_1_0 * columnSize + (height + 1)] - firstNoise_1_0) * interpFirstOctave;
            double stepFirstNoise_1_1 = (NoiseColumn[off_1_1 * columnSize + (height + 1)] - firstNoise_1_1) * interpFirstOctave;

            //double firstNoise_0_0 = NoiseColumn[(x * cellsize + 3) * 17 + 9]; // should only take care of (x*5+3)*17+9
            //double firstNoise_0_1 = NoiseColumn[(x * cellsize + 4) * 17 + 9]; // should only take care of (x*5+4)*17+9
            //double firstNoise_1_0 = NoiseColumn[((x + 1) * cellsize + 3) * 17 + 9]; // should only take care of ((x+1)*5+3)*17+9
            //double firstNoise_1_1 = NoiseColumn[((x + 1) * cellsize + 4) * 17 + 9]; // should only take care of ((x+1)*5+)*17+9
            //double stepFirstNoise_0_0 = (NoiseColumn[(x * cellsize + 3) * 17 + 10] - firstNoise_0_0) * interpFirstOctave;
            //double stepFirstNoise_0_1 = (NoiseColumn[(x * cellsize + 4) * 17 + 10] - firstNoise_0_1) * interpFirstOctave;
            //double stepFirstNoise_1_0 = (NoiseColumn[((x + 1) * cellsize + 3) * 17 + 10] - firstNoise_1_0) * interpFirstOctave;
            //double stepFirstNoise_1_1 = (NoiseColumn[((x + 1) * cellsize + 4) * 17 + 10] - firstNoise_1_1) * interpFirstOctave;
            for (uint8_t heightOffset = 0; heightOffset < 8; heightOffset++) {
                double secondNoise_0_0 = firstNoise_0_0;
                double secondNoise_0_1 = firstNoise_0_1;
                double stepSecondNoise_1_0 = (firstNoise_1_0 - firstNoise_0_0) * interpSecondOctave;
                double stepSecondNoise_1_1 = (firstNoise_1_1 - firstNoise_0_1) * interpSecondOctave;
                for (uint8_t xOffset = 0; xOffset < 4; xOffset++) {
                    uint8_t currentHeight = height * 8u + heightOffset; // max is 128
                    uint16_t index = (xOffset + x * 4u) << 11u | (z * 4u) << 7u | currentHeight;
                    double stoneLimit = secondNoise_0_0; // aka thirdNoise
                    double stepThirdNoise_0_1 = (secondNoise_0_1 - secondNoise_0_0) * interpThirdOctave;
                    for (int zOffset = 0; zOffset < 4; zOffset++) {
                        int block = AIR;
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
    delete[]NoiseColumn;
}


static inline void replaceBlockForBiomes(int chunkX, int chunkZ, uint8_t **chunkCache, Random *worldRandom, TerrainNoises terrainNoises) {
    uint8_t oceanLevel = 64;
    uint8_t MIN = oceanLevel;
    double noiseFactor = 0.03125;
    auto *sandFields = new double[16 * 16];
    auto *gravelField = new double[16 * 16];
    auto *heightField = new double[16 * 16];
    generateNoise(sandFields, chunkX * 16, chunkZ * 16, 0.0, 16, 16, 1, noiseFactor, noiseFactor, 1.0, terrainNoises.shoresBottomComposition, 4,1);
    // beware this error in alpha ;)
    generateFixedNoise(gravelField, chunkZ * 16, chunkX * 16, 16, 16, noiseFactor, noiseFactor, terrainNoises.shoresBottomComposition, 4);
    generateNoise(heightField, chunkX * 16, chunkZ * 16, 0.0, 16, 16, 1, noiseFactor * 2.0, noiseFactor * 2.0, noiseFactor * 2.0, terrainNoises.surfaceElevation, 4,1);

    for (int x = 0; x < 16; x++) {
        for (int k = 0; k < 12; k++) {
            next_double(worldRandom);
            next_double(worldRandom);
            next_double(worldRandom);
            for (int w = 0; w < 128; w++) {
                random_next_int(worldRandom, 5);
            }
        }
        for (int z = 12; z < 16; z++) {
            bool sandy = sandFields[x + z * 16] + next_double(worldRandom) * 0.20000000000000001 > 0.0;
            bool gravelly = gravelField[x + z * 16] + next_double(worldRandom) * 0.20000000000000001 > 3;
            int elevation = (int) (heightField[x + z * 16] / 3.0 + 3.0 + next_double(worldRandom) * 0.25);
            int state = -1;
            uint8_t aboveOceanAkaLand = GRASS;
            uint8_t belowOceanAkaEarthCrust = DIRT;
            for (int y = 127; y >= MIN; y--) {
                int chunkCachePos = (x * 16 + z) * 128 + y;
                uint8_t previousBlock = (*chunkCache)[chunkCachePos];

                if (previousBlock == 0) {
                    state = -1;
                    continue;
                }
                if (previousBlock != STONE) {
                    continue;
                }
                if (state == -1) { // AIR
                    if (elevation <= 0) { // if in a deep
                        aboveOceanAkaLand = 0;
                        belowOceanAkaEarthCrust = STONE;
                    } else if (y <= oceanLevel + 1) { // if at sea level do the shore and rivers
                        aboveOceanAkaLand = GRASS;
                        belowOceanAkaEarthCrust = DIRT;
                        if (gravelly) {
                            aboveOceanAkaLand = 0;
                        }
                        if (gravelly) {
                            belowOceanAkaEarthCrust = GRAVEL;
                        }
                        if (sandy) {
                            aboveOceanAkaLand = SAND;
                        }
                        if (sandy) {
                            belowOceanAkaEarthCrust = SAND;
                        }
                    }
                    state = elevation;
                    // above ocean level
                    (*chunkCache)[chunkCachePos] = aboveOceanAkaLand;
                    continue;
                }
                if (state > 0) {
                    state--;
                    (*chunkCache)[chunkCachePos] = belowOceanAkaEarthCrust;

                }
            }
            for (int k = 0; k < 128; k++) {
                random_next_int(worldRandom, 5);
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
    octaves = terrainNoises->forest;
    initOctaves(octaves, &worldRandom, 8);
    return terrainNoises;
}

static inline uint8_t *provideChunk(int chunkX, int chunkZ, BiomeResult *biomeResult, TerrainNoises *terrainNoises) {
    Random worldRandom = get_random((uint64_t) ((long) chunkX * 0x4f9939f508L + (long) chunkZ * 0x1ef1565bd5L));
    auto *chunkCache = new uint8_t[32768];
    generateTerrain(chunkX, chunkZ, &chunkCache, biomeResult->temperature, biomeResult->humidity, *terrainNoises);
    replaceBlockForBiomes(chunkX, chunkZ, &chunkCache, &worldRandom, *terrainNoises);
    return chunkCache;
}

void delete_terrain_result(TerrainResult *terrainResult) {
    delete[] terrainResult->chunkCache;
    delete[] terrainResult->chunkHeights;
    delete_biome_result(terrainResult->biomeResult);
    delete terrainResult;
}

uint8_t *TerrainInternalWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ, BiomeResult *biomeResult) {
    TerrainNoises *terrainNoises = initTerrain(worldSeed);
    uint8_t *chunkCache = provideChunk(chunkX, chunkZ, biomeResult, terrainNoises);
    delete terrainNoises;
    return chunkCache;
}

uint8_t *TerrainHeights(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ, BiomeResult *biomeResult) {
    TerrainNoises *terrainNoises = initTerrain(worldSeed);
    auto *chunkCache = provideChunk(chunkX, chunkZ, biomeResult, terrainNoises);
    delete[] terrainNoises;
    auto *chunkHeights = new uint8_t[4 * 16];
    for (int x = 0; x < 16; ++x) {
        for (int z = 12; z < 16; ++z) {
            int pos = 128 * x * 16 + 128 * z;
            int y;
            for (y = 80; y >= 70 && chunkCache[pos + y] == AIR; y--);
            chunkHeights[x * 4 + (z-12)] = (y + 1);
        }
    }
    delete[] chunkCache;
    return chunkHeights;
}




TerrainResult *TerrainWrapper(uint64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
    BiomeResult *biomeResult = BiomeWrapper(worldSeed, chunkX, chunkZ);
    auto *chunkCache = TerrainInternalWrapper(worldSeed, chunkX, chunkZ, biomeResult);
    auto *chunkHeights = new uint8_t[4 * 16];
    for (int x = 0; x < 16; ++x) {
        for (int z = 12; z < 16; ++z) {
            int pos = 128 * x * 16 + 128 * z;
            int y;
            for (y = 80; y >= 70 && chunkCache[pos + y] == 0; y--);
            //std::cout<<(y + 1)<<" ";
            chunkHeights[x * 4 + (z-12)] = (y + 1);
        }
        //std::cout<<std::endl;
    }
    auto *terrainResult = new TerrainResult;
    terrainResult->biomeResult = biomeResult;
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

//int main() {TerrainWrapper(18420882071630 ,-3 ,6);}