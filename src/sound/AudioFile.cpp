#include <sndfile.hh>
#include "AudioFile.hpp"
#include <spdlog/spdlog.h>

void AudioFile::load(const std::string& path) {
  fh = SndfileHandle(path);
  if (fh.frames() == 0 || fh.channels() == 0) {
    spdlog::warn("sound: failed to load '{}' (missing or invalid)", path);
    return;
  }
  sf_count_t totalSamples = fh.frames() * fh.channels();
  data = new int[totalSamples];
  spdlog::debug("sound {} frames={} channels={} samples={}", path, fh.frames(), fh.channels(), totalSamples);
  fh.read(data, totalSamples);
}
