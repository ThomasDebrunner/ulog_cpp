/****************************************************************************
 * Copyright (c) 2023 PX4 Development Team.
 * SPDX-License-Identifier: BSD-3-Clause
 ****************************************************************************/
#pragma once

#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "messages.hpp"

namespace ulog_cpp {

class FormattedData {
 public:
  FormattedData(Data data, MessageFormat &format) :
    _data{std::move(data)}, _format_ref(format) {}

//  void get(const std::string &key) {
//    _format_ref.fields().operator[](key)
//  }

 private:
  Data _data;
  MessageFormat& _format_ref;
};

}  // namespace ulog_cpp
