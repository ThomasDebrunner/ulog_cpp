/****************************************************************************
 * Copyright (c) 2023 PX4 Development Team.
 * SPDX-License-Identifier: BSD-3-Clause
 ****************************************************************************/
#pragma once

#include <cstdint>
#include <cstring>
#include <exception>
#include <variant>

#include "messages.hpp"

namespace ulog_cpp {

using NativeTypeVariant = std::variant<
    int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t,
    float, double, bool, char, std::vector<int8_t>, std::vector<uint8_t>, std::vector<int16_t>,
        std::vector<uint16_t>, std::vector<int32_t>, std::vector<uint32_t>, std::vector<int64_t>,
            std::vector<uint64_t>, std::vector<float>, std::vector<double>,
                std::string, MessageFormat>;

class FormattedMessage {
 public:
  FormattedData(Data &data, MessageFormat &format) :
    _data_ref(data), _format_ref(format) {}


  NativeTypeVariant getAsNativeTypeVariant(const Field &field) {

  }

 private:
  Data& _data_ref;
  MessageFormat& _format_ref;
};

}  // namespace ulog_cpp
