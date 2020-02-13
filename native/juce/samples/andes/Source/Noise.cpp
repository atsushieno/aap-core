/*
    Andes - synthesiser plugin based on Perlin noise
    Copyright (C) 2017  Artem Popov <art@artfwo.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Noise.h"

Noise::Noise()
{
    setSeed(0x568fd34e);
}

Noise::~Noise()
{
}

uint32_t Noise::getSeed()
{
    return seed_;
}

void Noise::setSeed(uint32_t seed)
{
    seed_ = seed;
    random.seed(seed);
    float randomMultiplier = 1.0f / random.max();

    for (int z = 0; z < 65536; ++z)
    {
        gradients[z] = random() * randomMultiplier * 2.0f - 1.0f;
    }
}

float Noise::noise(float z, float offset)
{
    float g1, g2, d2;
    float result = 0;

    int z1 = (int) z % 65536U;
    int z2 = (z1 + 1) % 65536U;

    float dz1 = z - z1;
    float dz2 = dz1 - 1.0f;

    g1 = gradients[z1];
    g2 = gradients[z2];

    g1 = g1 + offset;
    if (g1 > 1.0f) { g1 = 2.0f - g1; }
    if (g1 < -1.0f) { g1 = -2.0f - g1; }

    g2 = g2 + offset;
    if (g2 > 1.0f) { g2 = 2.0f - g2; }
    if (g2 < -1.0f) { g2 = -2.0f - g2; }

    d2 = 1.0 - (dz1 * dz1);
    d2 = d2 * d2 * d2 * d2;
    result += d2 * (g1 * dz1);

    d2 = 1.0 - (dz2 * dz2);
    d2 = d2 * d2 * d2 * d2;
    result += d2 * (g2 * dz2);

    return result * 3.33; // temp normalization value
}

float Noise::fbm(float z, int octaves, float persistence, float offset, float warping)
{
    float result = 0;
    float multiplier = 1.0f;
    float warpOffset = noise(z, offset) * warping;

    for (int octave = 0; octave < octaves; ++octave)
    {
        float t = fmod(offset, 4);
        float value = noise(z * (1 << octave) + warpOffset, t) * multiplier;
        warpOffset = value * warping;
        result += value;
        multiplier *= persistence;
    }

    return result;
}
