#pragma once

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

struct WriteSerializer {
  rapidjson::PrettyWriter<rapidjson::StringBuffer>& w;
};

struct ReadSerializer {
  const rapidjson::Value& d;
};