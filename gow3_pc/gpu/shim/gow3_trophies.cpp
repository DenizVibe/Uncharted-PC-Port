// SPDX-License-Identifier: GPL-2.0-or-later
// gow3 21 Oct 2026: trophies (gow3_trophies.h).

#include "gow3_trophies.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>
#include <SDL3/SDL.h>
#include "gow3_overlay.h"
#include "ps4_trophy_sound.h"

namespace Gow3Trophies {
namespace {

struct Entry {
    u64 offset = 0, size = 0;
};

std::mutex mutex;
std::string pack;
int list_number = 0;
u32 flags[4]{};
std::map<std::string, Entry> entries; // the pack's files by name

u32 Be32(const unsigned char* p) {
    return u32(p[0]) << 24 | u32(p[1]) << 16 | u32(p[2]) << 8 | p[3];
}
u64 Be64(const unsigned char* p) {
    return u64(Be32(p)) << 32 | Be32(p + 4);
}

// A TRP pack: a big-endian header (magic 0xDCA24D00, version, file size, file count, entry
// size) and from 0x60 one entry per file: a 32-byte name, its offset and size.
bool ReadIndex(const std::string& path) {
    entries.clear();
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return false;
    }
    unsigned char header[0x60];
    bool ok = std::fread(header, 1, sizeof(header), file) == sizeof(header) &&
              Be32(header) == 0xDCA24D00u;
    const u32 count = ok ? Be32(header + 16) : 0, entry_size = ok ? Be32(header + 20) : 0;
    ok = ok && count < 4096 && entry_size >= 48 && entry_size <= 256;
    std::vector<unsigned char> entry(entry_size);
    for (u32 i = 0; ok && i < count; ++i) {
        if (std::fread(entry.data(), 1, entry_size, file) != entry_size) {
            ok = false;
            break;
        }
        const std::string name(reinterpret_cast<const char*>(entry.data()),
                               strnlen(reinterpret_cast<const char*>(entry.data()), 32));
        entries[name] = Entry{Be64(entry.data() + 32), Be64(entry.data() + 40)};
    }
    std::fclose(file);
    return ok && !entries.empty();
}

std::string GameName(int list) {
    switch (list) {
    case 1: return "Uncharted: Drake's Fortune";
    case 2: return "Uncharted 2: Among Thieves";
    case 3: return "Uncharted 3: Drake's Deception";
    default: return "Uncharted";
    }
}

bool IsEarned(const u32 bits[4], int id) {
    return id >= 0 && id < 128 && (bits[id / 32] >> (id % 32) & 1);
}

// The trophies of the pack, from its pictures (TROPnnn.PNG); mutex held.
std::vector<int> Ids() {
    std::vector<int> ids;
    for (const auto& [name, entry] : entries) {
        int id = 0;
        char tail[8]{};
        if (name.size() == 11 && std::sscanf(name.c_str(), "TROP%3d.%3s", &id, tail) == 2 &&
            !std::strcmp(tail, "PNG")) {
            ids.push_back(id);
        }
    }
    return ids;
}

// Authentic PS4 trophy unlock sound on the default output (the runtime opened SDL audio).
// GOW3_TROPHY_SOUND=0: none.
void Chime() {
    static const bool enabled = [] {
        const char* env = std::getenv("GOW3_TROPHY_SOUND");
        return !(env && env[0] == '0');
    }();
    if (!enabled || !SDL_WasInit(SDL_INIT_AUDIO)) {
        return;
    }
    static SDL_AudioStream* stream = [] {
        const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 48000};
        SDL_AudioStream* s =
            SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (s) {
            SDL_ResumeAudioStreamDevice(s);
        }
        return s;
    }();
    if (!stream) {
        return;
    }

    bool played_custom = false;
    const char* custom_paths[] = {
        "user/custom_trophy/trophy.wav",
        "u3mp/assets/ps4-trophy.wav",
    };
    for (const char* path : custom_paths) {
        SDL_AudioSpec wav_spec{};
        Uint8* wav_buf = nullptr;
        Uint32 wav_len = 0;
        if (SDL_LoadWAV(path, &wav_spec, &wav_buf, &wav_len)) {
            const SDL_AudioSpec target_spec{SDL_AUDIO_F32, 2, 48000};
            SDL_AudioStream* converter = SDL_CreateAudioStream(&wav_spec, &target_spec);
            if (converter) {
                SDL_PutAudioStreamData(converter, wav_buf, int(wav_len));
                SDL_FlushAudioStream(converter);
                const int conv_bytes = SDL_GetAudioStreamAvailable(converter);
                if (conv_bytes > 0) {
                    std::vector<char> converted(size_t(conv_bytes));
                    if (SDL_GetAudioStreamData(converter, converted.data(), conv_bytes) > 0) {
                        SDL_PutAudioStreamData(stream, converted.data(), conv_bytes);
                        played_custom = true;
                    }
                }
                SDL_DestroyAudioStream(converter);
            }
            SDL_free(wav_buf);
            if (played_custom) {
                break;
            }
        }
    }

    if (!played_custom) {
        // Authentic PS4 trophy unlock chime from embedded PCM samples
        SDL_PutAudioStreamData(stream, TrophySoundPcm, int(sizeof(TrophySoundPcm)));
    }
}

} // namespace

void SetList(const std::string& pack_path, int list, const u32 earned[4]) {
    std::scoped_lock lock{mutex};
    if (pack_path != pack || entries.empty()) {
        pack = pack_path;
        if (!ReadIndex(pack)) {
            std::printf("Trophies: no trophy pack at %s\n", pack.c_str());
        }
    }
    list_number = list;
    std::memcpy(flags, earned, sizeof(flags));
    int count = 0;
    for (int id = 0; id < 128; ++id) {
        count += IsEarned(flags, id);
    }
    std::printf("Trophies: %s, %zu trophies, %d earned\n", GameName(list).c_str(), Ids().size(), count);
    std::fflush(stdout);
}

void Earned(int id) {
    if (id < 0 || id >= 128) {
        return;
    }
    Gow3Png::Image picture;
    std::string line;
    {
        std::scoped_lock lock{mutex};
        flags[id / 32] |= 1u << (id % 32);
        const std::vector<int> ids = Ids();
        int count = 0;
        for (const int other : ids) {
            count += IsEarned(flags, other);
        }
        char text[160];
        std::snprintf(text, sizeof(text), "%s\n%d of %zu trophies", GameName(list_number).c_str(),
                      count, ids.size());
        line = text;
    }
    Picture(id, picture);
    Gow3Overlay::ShowTrophy(std::move(picture), "Trophy earned", line);
    Chime();
}

Summary List() {
    std::scoped_lock lock{mutex};
    Summary summary;
    if (entries.empty()) {
        return summary;
    }
    summary.game = GameName(list_number);
    summary.list = list_number;
    summary.ids = Ids();
    for (const int id : summary.ids) {
        summary.earned.push_back(IsEarned(flags, id));
    }
    return summary;
}

bool Picture(int id, Gow3Png::Image& out) {
    out = {};
    std::string path;
    Entry entry;
    {
        std::scoped_lock lock{mutex};
        char name[16];
        std::snprintf(name, sizeof(name), "TROP%03d.PNG", id);
        const auto it = entries.find(name);
        if (it == entries.end()) {
            return false;
        }
        path = pack;
        entry = it->second;
    }
    if (entry.size == 0 || entry.size > (16u << 20)) {
        return false;
    }
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return false;
    }
    std::vector<unsigned char> bytes(entry.size);
    const bool read = std::fseek(file, long(entry.offset), SEEK_SET) == 0 &&
                      std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    return read && Gow3Png::DecodeRgba(bytes.data(), bytes.size(), out);
}

} // namespace Gow3Trophies
