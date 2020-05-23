#if defined _MSC_VER
  #define nullptr __nullptr
  #include <nlohmann/json.hpp>
  #undef nullptr
#else
  #include <nlohmann/json.hpp>
#endif
