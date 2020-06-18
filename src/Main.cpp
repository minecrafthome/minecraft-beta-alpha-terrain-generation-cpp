#include <iostream>
#include <chrono>
#include <fstream>
#include <algorithm>
#include "biome/biomeGen.h"
#include "heightmap/heightmapGen.h"

# define OFFSET 12

void filterDownSeeds(const uint64_t *worldSeeds, int32_t posX, int64_t nbSeeds) {
    uint8_t mapWat[] = {77, 78, 77, 75}; // from z 12 to z15 in chunk
    int chunkX = (int32_t) ((posX + 16u) >> 4u) - 1;
    int chunkZ = -3;
    for (int i = 0; i < nbSeeds; ++i) {
        int64_t seed = worldSeeds[i];
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
        uint8_t *chunkCache = TerrainInternalWrapper(seed, chunkX, chunkZ, biomeResult);
        for (int x = 0; x < 16; x++) {
            bool flag = true;
            for (uint8_t z = 0; z < (16 - OFFSET); z++) {
                uint32_t pos = 128 * x * 16 + 128 * (z + OFFSET);
                uint32_t y;

                for (y = 80; y >= 70 && chunkCache[pos + y] == 0; y--);
                if ((y + 1) != mapWat[z]) {
                    flag = false;
                    break;
                }

            }
            if (flag) {
                std::cout << "Found seed: " << seed << " at x: " << posX << " and z:-30" << std::endl;
            }
        }
        delete[] chunkCache;
        delete_biome_result(biomeResult);
    }

}

int main(int argc, char *argv[]) {

    std::ifstream file("test.txt");
    if (!file.is_open()) {
        std::cout << "file was not loaded" << std::endl;
        throw std::runtime_error("file was not loaded");
    }
    int64_t length = std::count(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), '\n');
    file.seekg(file.beg);
    auto *worldSeeds = new uint64_t[length];
    std::string line;
    size_t sz;
    int64_t seed;
    int64_t index = 0;
    while (std::getline(file, line).good()) {
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
    std::cout << "Running " << length << " seeds" <<std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    filterDownSeeds(worldSeeds,99, length);
    auto finish = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / 1e9 << " s\n";
    delete[] worldSeeds;
}
/*
 * SHOULD OUTPUT:
 *  Running 100000 seeds
 *  Found seed: 90389547180974 at x: 99 and z:-30
 *  Found seed: 171351315692858 at x: 99 and z:-30
 *  Found seed: 189587791856572 at x: 99 and z:-30
 *  Found seed: 66697851806768 at x: 99 and z:-30
 *  Found seed: 162899168234811 at x: 99 and z:-30
 *  XX.XXX s
 */