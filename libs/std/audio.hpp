#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <algorithm>
#include <map>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#endif

namespace viss {
namespace audio {

    const int SAMPLE_RATE = 44100;
    const int NUM_CHANNELS = 1; // Mono mixing for high performance
    const double PI = 3.14159265358979323846;

    enum WaveType {
        WAVE_SINE = 0,
        WAVE_SQUARE = 1,
        WAVE_TRIANGLE = 2,
        WAVE_SAWTOOTH = 3,
        WAVE_NOISE = 4,
        WAVE_PULSE_25 = 5,
        WAVE_PULSE_12 = 6
    };

    inline WaveType parseWaveType(const std::string& name) {
        if (name == "square") return WAVE_SQUARE;
        if (name == "triangle") return WAVE_TRIANGLE;
        if (name == "saw" || name == "sawtooth") return WAVE_SAWTOOTH;
        if (name == "noise") return WAVE_NOISE;
        if (name == "pulse25" || name == "pulse_25") return WAVE_PULSE_25;
        if (name == "pulse12" || name == "pulse_12") return WAVE_PULSE_12;
        return WAVE_SINE;
    }

    struct ADSR {
        double attack_ms = 10.0;
        double decay_ms = 40.0;
        double sustain_level = 0.7; // 0.0 - 1.0
        double release_ms = 50.0;
    };

    struct Voice {
        double frequency = 440.0;
        WaveType wave = WAVE_SINE;
        double volume = 0.5;
        double phase = 0.0;
        int total_samples = 0;
        int current_sample = 0;
        ADSR adsr;
        bool active = false;

        // Pitch slide / frequency sweep (for SFX)
        double freq_slide = 0.0; 

        // Smooth Low-Pass Filter
        double lpf_cutoff = 2500.0;
        double lpf_val = 0.0;
    };

    // Note to Frequency mapping
    inline double noteToFrequency(const std::string& note_str) {
        if (note_str.empty()) return 440.0;
        
        static const std::map<std::string, int> note_offsets = {
            {"C", 0}, {"C#", 1}, {"Db", 1},
            {"D", 2}, {"D#", 3}, {"Eb", 3},
            {"E", 4},
            {"F", 5}, {"F#", 6}, {"Gb", 6},
            {"G", 7}, {"G#", 8}, {"Ab", 8},
            {"A", 9}, {"A#", 10}, {"Bb", 10},
            {"B", 11}
        };

        std::string n = "";
        size_t idx = 0;
        while (idx < note_str.size() && !std::isdigit(note_str[idx])) {
            n += note_str[idx++];
        }
        int octave = 4;
        if (idx < note_str.size()) {
            octave = note_str[idx] - '0';
        }

        auto it = note_offsets.find(n);
        if (it == note_offsets.end()) return 440.0;

        int semitone = it->second + (octave * 12);
        // MIDI 69 is A4 (440Hz). C0 is MIDI 12.
        int midi = semitone + 12;
        return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
    }

    // =========================================================================
    // PROFESSIONAL REAL-TIME AUDIO SYNTHESIZER & MIXER
    // =========================================================================
    class Engine {
    private:
        std::atomic<bool> running{false};
        std::atomic<float> master_volume{0.8f};
        std::thread audio_thread;
        std::mutex voice_mutex;
        std::vector<Voice> voices;
        static const int MAX_VOICES = 32;

        #ifdef _WIN32
        HWAVEOUT hWaveOut = NULL;
        static const int BUFFER_COUNT = 4;
        static const int BUFFER_SIZE = 1024; // Samples per buffer (~23ms latency)
        int16_t audio_buffers[BUFFER_COUNT][BUFFER_SIZE];
        WAVEHDR wave_headers[BUFFER_COUNT];
        #endif

        double generateSample(Voice& v) {
            double raw = 0.0;
            double p = v.phase;

            switch (v.wave) {
                case WAVE_SINE:
                    raw = std::sin(2.0 * PI * p);
                    break;
                case WAVE_SQUARE:
                    raw = (p < 0.5) ? 1.0 : -1.0;
                    break;
                case WAVE_PULSE_25:
                    raw = (p < 0.25) ? 1.0 : -1.0;
                    break;
                case WAVE_PULSE_12:
                    raw = (p < 0.125) ? 1.0 : -1.0;
                    break;
                case WAVE_TRIANGLE:
                    raw = (p < 0.5) ? (4.0 * p - 1.0) : (3.0 - 4.0 * p);
                    break;
                case WAVE_SAWTOOTH:
                    raw = 2.0 * p - 1.0;
                    break;
                case WAVE_NOISE:
                    raw = ((double)rand() / (double)RAND_MAX) * 2.0 - 1.0;
                    break;
            }

            // Envelope calculation
            double env = 1.0;
            double t_ms = (v.current_sample * 1000.0) / SAMPLE_RATE;
            double total_ms = (v.total_samples * 1000.0) / SAMPLE_RATE;

            double a_ms = v.adsr.attack_ms;
            double d_ms = v.adsr.decay_ms;
            double s_lvl = v.adsr.sustain_level;
            double r_ms = v.adsr.release_ms;
            double release_start_ms = total_ms - r_ms;
            if (release_start_ms < a_ms + d_ms) release_start_ms = a_ms + d_ms;

            if (t_ms < a_ms) {
                env = (a_ms > 0) ? (t_ms / a_ms) : 1.0;
            } else if (t_ms < a_ms + d_ms) {
                double dec_progress = (t_ms - a_ms) / (d_ms > 0 ? d_ms : 1.0);
                env = 1.0 - (1.0 - s_lvl) * dec_progress;
            } else if (t_ms < release_start_ms) {
                env = s_lvl;
            } else {
                double rel_progress = (t_ms - release_start_ms) / (r_ms > 0 ? r_ms : 1.0);
                env = s_lvl * (1.0 - rel_progress);
                if (env < 0.0) env = 0.0;
            }

            // Advance voice phase and pitch
            v.phase += v.frequency / SAMPLE_RATE;
            if (v.phase >= 1.0) v.phase -= std::floor(v.phase);

            v.frequency += v.freq_slide;
            if (v.frequency < 20.0) v.frequency = 20.0;

            v.current_sample++;
            if (v.current_sample >= v.total_samples) {
                v.active = false;
            }

            // Apply 1-pole Low-Pass Filter to eliminate harsh digital aliasing
            double alpha = 2.0 * PI * (v.lpf_cutoff / SAMPLE_RATE);
            if (alpha > 0.85) alpha = 0.85;
            if (alpha < 0.01) alpha = 0.01;
            v.lpf_val += alpha * (raw - v.lpf_val);
            raw = v.lpf_val;

            return raw * env * v.volume;
        }

        void audioLoop() {
            #ifdef _WIN32
            WAVEFORMATEX wfx;
            wfx.wFormatTag = WAVE_FORMAT_PCM;
            wfx.nChannels = NUM_CHANNELS;
            wfx.nSamplesPerSec = SAMPLE_RATE;
            wfx.wBitsPerSample = 16;
            wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
            wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
            wfx.cbSize = 0;

            if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
                running = false;
                return;
            }

            for (int i = 0; i < BUFFER_COUNT; ++i) {
                ZeroMemory(&wave_headers[i], sizeof(WAVEHDR));
                wave_headers[i].lpData = (LPSTR)audio_buffers[i];
                wave_headers[i].dwBufferLength = BUFFER_SIZE * sizeof(int16_t);
                waveOutPrepareHeader(hWaveOut, &wave_headers[i], sizeof(WAVEHDR));
                wave_headers[i].dwFlags |= WHDR_DONE; // Mark ready
            }

            int cur_buf = 0;

            while (running) {
                WAVEHDR* hdr = &wave_headers[cur_buf];

                // Wait until buffer is released by hardware
                while ((hdr->dwFlags & WHDR_DONE) == 0 && running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }

                if (!running) break;

                // Synthesize next chunk of audio
                {
                    std::lock_guard<std::mutex> lock(voice_mutex);
                    for (int s = 0; s < BUFFER_SIZE; ++s) {
                        double mixed = 0.0;
                        for (auto& v : voices) {
                            if (v.active) {
                                mixed += generateSample(v);
                            }
                        }

                        // Apply master volume and soft limiter (tanh saturation)
                        mixed *= master_volume.load();
                        if (mixed > 1.2) mixed = 1.2;
                        if (mixed < -1.2) mixed = -1.2;
                        double clipped = std::tanh(mixed);

                        audio_buffers[cur_buf][s] = (int16_t)(clipped * 32767.0);
                    }
                }

                hdr->dwFlags &= ~WHDR_DONE;
                waveOutWrite(hWaveOut, hdr, sizeof(WAVEHDR));
                cur_buf = (cur_buf + 1) % BUFFER_COUNT;
            }

            // Cleanup Win32 audio
            waveOutReset(hWaveOut);
            for (int i = 0; i < BUFFER_COUNT; ++i) {
                waveOutUnprepareHeader(hWaveOut, &wave_headers[i], sizeof(WAVEHDR));
            }
            waveOutClose(hWaveOut);
            hWaveOut = NULL;
            #endif
        }

    public:
        Engine() {
            voices.resize(MAX_VOICES);
            start();
        }

        ~Engine() {
            stop();
        }

        void start() {
            if (running) return;
            running = true;
            std::thread t(&Engine::audioLoop, this);
            t.detach();
        }

        void stop() {
            if (!running) return;
            running = false;
            #ifdef _WIN32
            if (hWaveOut) {
                waveOutReset(hWaveOut);
            }
            #endif
        }

        void set_volume(double vol) {
            if (vol < 0.0) vol = 0.0;
            if (vol > 1.0) vol = 1.0;
            master_volume = (float)vol;
        }

        double get_volume() const {
            return (double)master_volume.load();
        }

        void stop_all() {
            std::lock_guard<std::mutex> lock(voice_mutex);
            for (auto& v : voices) v.active = false;
        }

        bool is_playing() {
            std::lock_guard<std::mutex> lock(voice_mutex);
            for (const auto& v : voices) {
                if (v.active) return true;
            }
            return false;
        }

        void play_tone(double freq, int duration_ms, const std::string& wave_name = "sine", double volume = 0.5) {
            std::lock_guard<std::mutex> lock(voice_mutex);
            for (auto& v : voices) {
                if (!v.active) {
                    v.frequency = freq;
                    v.wave = parseWaveType(wave_name);
                    v.volume = volume;
                    v.phase = 0.0;
                    v.total_samples = (int)((duration_ms / 1000.0) * SAMPLE_RATE);
                    v.current_sample = 0;
                    v.freq_slide = 0.0;
                    v.adsr = {5.0, 20.0, 0.8, 20.0};
                    v.active = true;
                    return;
                }
            }
        }

        void play_note(const std::string& note_name, int duration_ms, const std::string& wave_name = "sine", double volume = 0.5) {
            double freq = noteToFrequency(note_name);
            play_tone(freq, duration_ms, wave_name, volume);
        }

        void synth(double freq, int duration_ms, const std::string& wave_name,
                   double a_ms, double d_ms, double s_lvl, double r_ms, double volume = 0.5) {
            std::lock_guard<std::mutex> lock(voice_mutex);
            for (auto& v : voices) {
                if (!v.active) {
                    v.frequency = freq;
                    v.wave = parseWaveType(wave_name);
                    v.volume = volume;
                    v.phase = 0.0;
                    v.total_samples = (int)((duration_ms / 1000.0) * SAMPLE_RATE);
                    v.current_sample = 0;
                    v.freq_slide = 0.0;
                    v.adsr = {a_ms, d_ms, s_lvl, r_ms};
                    v.active = true;
                    return;
                }
            }
        }

        // Procedural Retro & Modern Sound Effects
        void sfx(const std::string& effect_name) {
            std::lock_guard<std::mutex> lock(voice_mutex);
            Voice* slot = nullptr;
            for (auto& v : voices) {
                if (!v.active) { slot = &v; break; }
            }
            if (!slot) return;

            slot->phase = 0.0;
            slot->current_sample = 0;
            slot->active = true;

            if (effect_name == "laser" || effect_name == "shoot") {
                slot->frequency = 650.0;
                slot->wave = WAVE_PULSE_25;
                slot->volume = 0.35;
                slot->total_samples = (int)(0.12 * SAMPLE_RATE);
                slot->freq_slide = -7.0;
                slot->lpf_cutoff = 1800.0;
                slot->adsr = {2.0, 30.0, 0.2, 30.0};
            } else if (effect_name == "jump") {
                slot->frequency = 220.0;
                slot->wave = WAVE_TRIANGLE;
                slot->volume = 0.35;
                slot->total_samples = (int)(0.18 * SAMPLE_RATE);
                slot->freq_slide = 4.0;
                slot->lpf_cutoff = 1500.0;
                slot->adsr = {5.0, 30.0, 0.4, 30.0};
            } else if (effect_name == "coin" || effect_name == "gem") {
                slot->frequency = 987.77; // B5
                slot->wave = WAVE_SINE;
                slot->volume = 0.30;
                slot->total_samples = (int)(0.20 * SAMPLE_RATE);
                slot->freq_slide = 1.5;
                slot->lpf_cutoff = 3200.0;
                slot->adsr = {2.0, 40.0, 0.4, 40.0};
            } else if (effect_name == "powerup" || effect_name == "level_up") {
                slot->frequency = 392.0; // G4
                slot->wave = WAVE_TRIANGLE;
                slot->volume = 0.35;
                slot->total_samples = (int)(0.35 * SAMPLE_RATE);
                slot->freq_slide = 4.5;
                slot->lpf_cutoff = 2200.0;
                slot->adsr = {8.0, 60.0, 0.6, 60.0};
            } else if (effect_name == "explosion" || effect_name == "boom") {
                slot->frequency = 80.0;
                slot->wave = WAVE_NOISE;
                slot->volume = 0.45;
                slot->total_samples = (int)(0.35 * SAMPLE_RATE);
                slot->freq_slide = -0.3;
                slot->lpf_cutoff = 650.0;
                slot->adsr = {4.0, 80.0, 0.2, 120.0};
            } else if (effect_name == "hit" || effect_name == "hurt") {
                slot->frequency = 150.0;
                slot->wave = WAVE_TRIANGLE;
                slot->volume = 0.35;
                slot->total_samples = (int)(0.10 * SAMPLE_RATE);
                slot->freq_slide = -4.0;
                slot->lpf_cutoff = 900.0;
                slot->adsr = {2.0, 20.0, 0.2, 20.0};
            } else if (effect_name == "dash") {
                slot->frequency = 120.0;
                slot->wave = WAVE_NOISE;
                slot->volume = 0.30;
                slot->total_samples = (int)(0.20 * SAMPLE_RATE);
                slot->freq_slide = -0.5;
                slot->lpf_cutoff = 750.0;
                slot->adsr = {5.0, 60.0, 0.2, 80.0};
            } else if (effect_name == "blip" || effect_name == "select") {
                slot->frequency = 520.0;
                slot->wave = WAVE_SINE;
                slot->volume = 0.25;
                slot->total_samples = (int)(0.05 * SAMPLE_RATE);
                slot->freq_slide = 0.0;
                slot->lpf_cutoff = 2000.0;
                slot->adsr = {1.0, 10.0, 0.3, 10.0};
            } else {
                // Soft chime default
                slot->frequency = 440.0;
                slot->wave = WAVE_SINE;
                slot->volume = 0.25;
                slot->total_samples = (int)(0.08 * SAMPLE_RATE);
                slot->freq_slide = 0.0;
                slot->lpf_cutoff = 1800.0;
                slot->adsr = {4.0, 15.0, 0.4, 15.0};
            }
        }
    };

    inline Engine& getEngine() {
        static Engine engine;
        return engine;
    }

    // Public Viss API bindings:
    inline void set_volume(double vol) { getEngine().set_volume(vol); }
    inline double get_volume() { return getEngine().get_volume(); }
    inline void stop_all() { getEngine().stop_all(); }
    inline bool is_playing_q() { return getEngine().is_playing(); }
    inline void play_tone(double freq, int duration_ms, const std::string& wave = "sine", double volume = 0.5) {
        getEngine().play_tone(freq, duration_ms, wave, volume);
    }
    inline void play_note(const std::string& note, int duration_ms, const std::string& wave = "sine", double volume = 0.5) {
        getEngine().play_note(note, duration_ms, wave, volume);
    }
    inline void synth(double freq, int duration_ms, const std::string& wave,
                      double attack_ms, double decay_ms, double sustain_level, double release_ms, double volume = 0.5) {
        getEngine().synth(freq, duration_ms, wave, attack_ms, decay_ms, sustain_level, release_ms, volume);
    }
    inline void sfx(const std::string& effect_name) {
        getEngine().sfx(effect_name);
    }
    inline void laser(double freq = 650.0, int dur_ms = 120) { getEngine().sfx("laser"); }
    inline void hit() { getEngine().sfx("hit"); }
    inline void explosion() { getEngine().sfx("explosion"); }
    inline void powerup() { getEngine().sfx("powerup"); }
    inline void gem() { getEngine().sfx("gem"); }
    inline void jump() { getEngine().sfx("jump"); }
    inline void dash() { getEngine().sfx("dash"); }

#ifdef _WIN32
    inline std::string find_audio_ffmpeg() {
        std::vector<std::string> candidates = {
            "C:\\Users\\halva\\AppData\\Local\\Programs\\AudioCoverWatcher\\tools\\ffmpeg.exe",
            "C:\\Users\\halva\\OggCoverWatcherViss\\tools\\ffmpeg.exe",
            "tools\\ffmpeg.exe",
            "ffmpeg.exe"
        };
        for (const auto& c : candidates) {
            DWORD attr = GetFileAttributesA(c.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                return c;
            }
        }
        return "ffmpeg";
    }

    inline bool play_file(const std::string& path) {
        mciSendStringA("close acw_audio", NULL, 0, NULL);
        std::string target_path = path;

        std::string ext = "";
        size_t dot = path.rfind('.');
        if (dot != std::string::npos) {
            ext = path.substr(dot);
            for (auto& ch : ext) ch = (char)tolower(ch);
        }

        // For formats not natively supported by MCI (OGG, FLAC, OPUS), fast-decode to temporary WAV
        if (ext == ".ogg" || ext == ".flac" || ext == ".opus" || ext == ".m4a") {
            char temp_dir[MAX_PATH];
            GetTempPathA(MAX_PATH, temp_dir);
            std::string temp_wav = std::string(temp_dir) + "viss_preview_" + std::to_string(GetCurrentProcessId()) + ".wav";
            std::string ff = find_audio_ffmpeg();
            std::string cmd = "cmd.exe /c \"\"" + ff + "\" -y -i \"" + path + "\" -vn \"" + temp_wav + "\"\"";

            STARTUPINFOA si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = { 0 };
            if (CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0x08000000, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 4000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                target_path = temp_wav;
            }
        }

        std::string cmd = "open \"" + target_path + "\" type mpegvideo alias acw_audio";
        if (mciSendStringA(cmd.c_str(), NULL, 0, NULL) != 0) {
            cmd = "open \"" + target_path + "\" alias acw_audio";
            mciSendStringA(cmd.c_str(), NULL, 0, NULL);
        }
        int res = mciSendStringA("play acw_audio", NULL, 0, NULL);
        return (res == 0);
    }

    inline void stop_file() {
        mciSendStringA("stop acw_audio", NULL, 0, NULL);
        mciSendStringA("close acw_audio", NULL, 0, NULL);
    }

    inline bool is_file_playing() {
        char status[128] = {0};
        mciSendStringA("status acw_audio mode", status, sizeof(status), NULL);
        return (std::string(status) == "playing");
    }

    inline bool play_bgm(const std::string& path, double volume = 0.25) {
        mciSendStringA("close viss_bgm", NULL, 0, NULL);
        std::string cmd = "open \"" + path + "\" type mpegvideo alias viss_bgm";
        if (mciSendStringA(cmd.c_str(), NULL, 0, NULL) != 0) {
            cmd = "open \"" + path + "\" alias viss_bgm";
            mciSendStringA(cmd.c_str(), NULL, 0, NULL);
        }
        int vol = (int)(volume * 1000.0);
        if (vol < 0) vol = 0;
        if (vol > 1000) vol = 1000;
        std::string vol_cmd = "setaudio viss_bgm volume to " + std::to_string(vol);
        mciSendStringA(vol_cmd.c_str(), NULL, 0, NULL);
        mciSendStringA("play viss_bgm repeat", NULL, 0, NULL);
        return true;
    }

    inline void stop_bgm() {
        mciSendStringA("stop viss_bgm", NULL, 0, NULL);
        mciSendStringA("close viss_bgm", NULL, 0, NULL);
    }

    inline void set_bgm_volume(double volume) {
        int vol = (int)(volume * 1000.0);
        if (vol < 0) vol = 0;
        if (vol > 1000) vol = 1000;
        std::string vol_cmd = "setaudio viss_bgm volume to " + std::to_string(vol);
        mciSendStringA(vol_cmd.c_str(), NULL, 0, NULL);
    }
#else
    inline bool play_file(const std::string& path) { return false; }
    inline void stop_file() {}
    inline bool is_file_playing() { return false; }
    inline bool play_bgm(const std::string& path, double volume = 0.25) { return false; }
    inline void stop_bgm() {}
    inline void set_bgm_volume(double volume) {}
#endif

} // namespace audio
} // namespace viss
