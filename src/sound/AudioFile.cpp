#pragma once

#include <sndfile.hh>
#include "AudioFile.hpp"
#include <spdlog/spdlog.h>

void AudioFile::load(const std::string& path) {
  fh = SndfileHandle(path.c_str());
  int N = fh.frames();
  data = new int[N];
  spdlog::get("console")->warn("sound {} frame count {}", path, N);
  fh.read(data, N);
}
