#include <cstdint>

#include "App/Map.hpp"
#include "App/utils.hpp"

std::map<TileType, uint32_t> colorMap = {
    { Grass, rgba(0x98, 0xdd, 0x00, 0xff) },
    { Water, rgba(0x00, 0xdd, 0xca, 0xff) },
    { Sand , rgba(0xc2, 0xb2, 0x80, 0xff) },
    { DesertSand, rgba(0xf4, 0xa4, 0x60, 0xff) },
    { Dirt, rgba(0xbb, 0x8b, 0x00, 0xff) },
    { Stone, rgba(0x8d, 0x8d, 0x8d, 0xff) },
    { Ice, rgba(0xb9, 0xe8, 0xea, 0xff) },
    { Snow, rgba(0xff, 0xfa, 0xfa, 0xff) },
    { Lava, rgba(0xcf, 0x10, 0x20, 0xff) },
    { VolcanicRock, rgba(0x3d, 0x3f, 0x3e, 0xff) },
};

uint32_t getColor(TileType type) {
    return colorMap[type];
}

Map::Map() {
    baselineBrightness = 20;
    sunlightBrightness = 100;
    sunlightAngle = M_PI/3;
}

Map::Map(uint32_t seed) : heightNoise(seed), tempNoise(seed + 2), humidNoise(seed + 3), extraNoise(seed + 4) {
    baselineBrightness = 20;
    sunlightBrightness = 100;
    sunlightAngle = M_PI/3;
}

void Map::genTile(TileData& res, double x, double y, bool calculateLight) {
    TileBiome biome = BNormal;
    TileBehavior behavior = TSolid;
    TileType type = Water;

    double noise = 16 * (heightNoise.octaveNoise(x/HEIGHT_RATIO, y/HEIGHT_RATIO, OCTAVES) + 1) - 1;
    double humid = 50 * (humidNoise.octaveNoise(x/HUMID_RATIO, y/HUMID_RATIO, OCTAVES) + 1);
    double temp  = 32.5 * (tempNoise.octaveNoise(x/TEMP_RATIO, y/TEMP_RATIO, OCTAVES) + 1) - 15;

    // range for values:
    // noise = 0-32
    // humid = 0 to 100
    // temp = -15 to 50
    if (temp <= 0) {
        biome = BIce;
        // icy 
        if (noise <= 6) {
            type = Ice;
            noise = 5;
        } else {
            type = Snow;
        }
    } else if (temp <= 40) {
        biome = BNormal;
        // normal
        if (humid <= 25 && noise <= 7) {
            // adjust water level due to humidity
            noise += 5 * (25-humid)/25;
        }
        if (noise <= 6) {
            if (temp <= 2.5) {
                biome = BIce;
                type = Ice;
            } else if (temp <= 38) {
                type = Water;
                behavior = TReflectiveLiquid;
            } else {
                biome = BHell;
                type = Lava;
                behavior = TLiquid;
            }
            noise = 5;
        } else if (humid <= 25) {
            biome = BDesert;
            type = DesertSand;
        } else if (noise <= 15) {
            biome = BDesert;
            type = Sand;
        } else if (noise <= 25) {
            type = Grass;
        } else {
            type = Stone;
        }
    } else {
        biome = BHell;
        // hell-ish
        if (noise <= 6) {
            type = Lava;
            noise = 5;
            behavior = TLiquid;
        } else {
            type = VolcanicRock;
        }
    }

    int32_t height = noise * 6;
    double light = sunlightBrightness;

    if (calculateLight) {
        TileData temp;
        double maxDistance = 42/sin(sunlightAngle * M_PI/180);
        // cap the shadow stuff at this distance
        if (maxDistance > 20) maxDistance = 20;
        for (double dy = 0; dy < maxDistance; dy += 1) {
            genTile(temp, x, y + dy, false);
            double delta = maxDistance + 20;
            if (temp.height > height) {
                light = (sunlightBrightness - baselineBrightness) * (delta - dy) / delta;
                break;
            }
        }
    }

    res.biome = biome;
    res.behavior = behavior;
    res.type = type;
    res.color = getColor(type);
    res.height = height;
    res.light = light;
}
void Map::mapgen(MapChunk& chunk, int32_t chunkX, int32_t chunkY) {

    for (int32_t dy = 0; dy < CHUNK_SIZE; dy++) {
        for (int32_t dx = 0; dx < CHUNK_SIZE; dx++) {
            double x = chunkX * CHUNK_SIZE + dx;
            double y = chunkY * CHUNK_SIZE + dy;
            genTile(chunk.data[dx * CHUNK_SIZE + dy], x, y, true);
        }
    }
    chunk.initialized = true;
}

TileData Map::get(double _x, double _y) {
    int32_t x = _x;
    int32_t y = _y;

    int32_t chunkX = x / CHUNK_SIZE;
    int32_t chunkY = y / CHUNK_SIZE;
    // https://stackoverflow.com/questions/12276675/modulus-with-negative-numbers-in-c/21470301
    uint32_t offsetX = mod(x, CHUNK_SIZE);
    uint32_t offsetY = mod(y, CHUNK_SIZE);

    Vector2D pos = std::make_pair(chunkX, chunkY);
    const auto& chunkIter = cacheMap.find(pos);
    if (chunkIter != cacheMap.end()) {
        return chunkIter->second.data[offsetX * CHUNK_SIZE + offsetY];
    } else {
        MapChunk chunk;
        mapgen(chunk, chunkX, chunkY);
        cacheMap.insert({ pos, chunk });
        return chunk.data[offsetX * CHUNK_SIZE + offsetY];
    }
}