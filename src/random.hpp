#pragma once

// MT19937: Copyright (C) 1997-2002 Makoto Matsumoto and Takuji Nishimura; see third_party/LICENSE.mt19937.
// SeedSequence: Copyright (c) 2015 Melissa E. O'Neill and (c) 2019 NumPy Developers; see third_party/LICENSE.numpy-seed-sequence.

#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <utility>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../third_party/pcg64.h"

#if defined(_WIN32) && !defined(__MINGW32__)
#undef inline
#endif

namespace openqr {

class CPythonRandom {
public:
    explicit CPythonRandom(std::uint64_t seed) {
        state_[0] = 19650218;
        for (std::uint32_t i = 1; i < state_.size(); ++i)
            state_[i] = 1812433253U * (state_[i - 1] ^ (state_[i - 1] >> 30)) + i;
        const std::array<std::uint32_t, 2> key{static_cast<std::uint32_t>(seed), static_cast<std::uint32_t>(seed >> 32)};
        const std::size_t length = key[1] ? 2 : 1;
        std::size_t i = 1, j = 0;
        for (std::size_t k = state_.size(); k; --k) {
            state_[i] = (state_[i] ^ ((state_[i - 1] ^ (state_[i - 1] >> 30)) * 1664525U)) + key[j] + static_cast<std::uint32_t>(j);
            if (++i == state_.size()) {
                state_[0] = state_.back();
                i = 1;
            }
            if (++j == length)
                j = 0;
        }
        for (std::size_t k = state_.size() - 1; k; --k) {
            state_[i] = (state_[i] ^ ((state_[i - 1] ^ (state_[i - 1] >> 30)) * 1566083941U)) - static_cast<std::uint32_t>(i);
            if (++i == state_.size()) {
                state_[0] = state_.back();
                i = 1;
            }
        }
        state_[0] = 0x80000000U;
    }

    std::uint32_t getrandbits32() {
        if (index_ == state_.size()) {
            for (std::size_t i = 0; i < state_.size(); ++i) {
                const std::uint32_t bits = (state_[i] & 0x80000000U) | (state_[(i + 1) % state_.size()] & 0x7fffffffU);
                state_[i] = state_[(i + 397) % state_.size()] ^ (bits >> 1) ^ ((bits & 1) ? 0x9908b0dfU : 0);
            }
            index_ = 0;
        }
        std::uint32_t value = state_[index_++];
        value ^= value >> 11;
        value ^= (value << 7) & 0x9d2c5680U;
        value ^= (value << 15) & 0xefc60000U;
        value ^= value >> 18;
        return value;
    }

    double random() {
        const std::uint32_t a = getrandbits32() >> 5;
        const std::uint32_t b = getrandbits32() >> 6;
        return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0);
    }

    void shuffle(std::span<int> values) {
        for (std::size_t i = values.size(); i > 1; --i) {
            const int bits = static_cast<int>(std::bit_width(i));
            std::uint32_t chosen;
            do {
                chosen = getrandbits32() >> (32 - bits);
            } while (chosen >= i);
            std::swap(values[i - 1], values[chosen]);
        }
    }

private:
    std::array<std::uint32_t, 624> state_{};
    std::size_t index_ = state_.size();
};

class NumPyPcg64 {
public:
    explicit NumPyPcg64(std::uint32_t seed) {
        std::array<std::uint32_t, 4> pool{};
        std::uint32_t hash = 0x43b0d7e5U;
        for (std::size_t i = 0; i < pool.size(); ++i) {
            std::uint32_t value = (i == 0 ? seed : 0) ^ hash;
            hash *= 0x931e8875U;
            value *= hash;
            pool[i] = value ^ (value >> 16);
        }
        for (std::size_t source = 0; source < pool.size(); ++source) {
            for (std::size_t target = 0; target < pool.size(); ++target) {
                if (source != target) {
                    std::uint32_t value = pool[source] ^ hash;
                    hash *= 0x931e8875U;
                    value *= hash;
                    value ^= value >> 16;
                    value = 0xca01f9ddU * pool[target] - 0x4973f715U * value;
                    pool[target] = value ^ (value >> 16);
                }
            }
        }
        std::array<std::uint32_t, 8> words{};
        hash = 0x8b51f9ddU;
        for (std::size_t i = 0; i < words.size(); ++i) {
            std::uint32_t value = pool[i % pool.size()] ^ hash;
            hash *= 0x58f38dedU;
            value *= hash;
            words[i] = value ^ (value >> 16);
        }
        std::array<std::uint64_t, 4> entropy{};
        for (std::size_t i = 0; i < entropy.size(); ++i)
            entropy[i] = words[2 * i] | (static_cast<std::uint64_t>(words[2 * i + 1]) << 32);
        pcg64_srandom_r(&state_, PCG_128BIT_CONSTANT(entropy[0], entropy[1]), PCG_128BIT_CONSTANT(entropy[2], entropy[3]));
    }

    double random() {
        return (pcg64_random_r(&state_) >> 11) * (1.0 / 9007199254740992.0);
    }

private:
    pcg64_random_t state_{};
};

}
