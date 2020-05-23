#if defined ROCKET_PLATFORM_WIN32
  #define nullptr __nullptr
  #include <nlohmann/json.hpp>
  #undef nullptr
#else
  #include <nlohmann/json.hpp>
#endif
