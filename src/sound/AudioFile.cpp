#include <sndfile.hh>
#include "AudioFile.hpp"
#include <spdlog/spdlog.h>

void AudioFile::load(const std::string& path) {
  fh = SndfileHandle(path.c_str());
  sf_count_t N = fh.frames();
  data = new int[N];
  spdlog::debug("sound {} frame count {}", path, N);
  fh.read(data, N);
}
