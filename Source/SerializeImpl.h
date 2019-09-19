#pragma once

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include <filesystem>

struct WriteSerializer {
  rapidjson::PrettyWriter<rapidjson::StringBuffer>& w;

  std::filesystem::path rootPath;
};

struct ReadSerializer {
  const rapidjson::Value& d;

  std::string fileName;
};

// Generic serialization tags
static constexpr const char* kClassTag("class");
