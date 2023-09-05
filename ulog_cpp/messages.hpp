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
#include <variant>
#include <vector>
#include <map>
#include <memory>

#include "exception.hpp"
#include "raw_messages.hpp"

namespace ulog_cpp {

// ULog format: https://docs.px4.io/main/en/dev_log/ulog_file_format.html

using DataWriteCB = std::function<void(const uint8_t* data, int length)>;

class FileHeader {
 public:
  FileHeader(const ulog_file_header_s& header, const ulog_message_flag_bits_s& flag_bits);
  explicit FileHeader(const ulog_file_header_s& header);

  explicit FileHeader(uint64_t timestamp = 0, bool has_default_parameters = false);

  const ulog_file_header_s& header() const { return _header; }
  const ulog_message_flag_bits_s& flagBits() const { return _flag_bits; }

  void serialize(const DataWriteCB& writer) const;

  bool operator==(const FileHeader& h) const
  {
    return memcmp(&_header, &h._header, sizeof(_header)) == 0 &&
           (!_has_flag_bits || memcmp(&_flag_bits, &h._flag_bits, sizeof(_flag_bits)) == 0);
  }

 private:
  ulog_file_header_s _header{};
  ulog_message_flag_bits_s _flag_bits{};
  bool _has_flag_bits{true};
};

class MessageFormat; // forward declaration

class Field {
 public:

  enum class BasicType {
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64,
    FLOAT,
    DOUBLE,
    CHAR,
    BOOL,
    NESTED,
  };

  struct TypeAttributes {
    std::string name;
    BasicType type;
    int size;
    std::shared_ptr<MessageFormat> nested_message{nullptr};

    TypeAttributes() = default;

    TypeAttributes(std::string name, BasicType type_, int size_) : name(std::move(name)),
          type(type_), size(size_), nested_message(nullptr) {}
  };

  static const std::map<std::string, TypeAttributes> kBasicTypes;

  Field() = default;

  Field(const char* str, int len);

  Field(const std::string &type_str, std::string name_str, int array_length_int = -1)
      : _array_length(array_length_int), _name(std::move(name_str))
  {
        auto it = kBasicTypes.find(type_str);
        if (it != kBasicTypes.end()) {
          _type = it->second;
        }
        // if not a basic type, set it to recursive
        _type = TypeAttributes(type_str, BasicType::NESTED, 0);
  }

  std::string encode() const;


  bool operator==(const Field& field) const
  {
    return _type.name == field._type.name && _array_length == field._array_length && _name == field._name;
  }

  inline const TypeAttributes& type() const {
    return _type;
  }

  inline int arrayLength() const {
    return _array_length;
  }

  inline int offsetInMessage() const {
    return _offset_in_message_bytes;
  }

  inline const std::string& name() const {
    return _name;
  }

  inline int sizeBytes() const;

  inline bool definitionResolved() const {
      return _offset_in_message_bytes >= 0 && (_type.type != BasicType::NESTED || _type.nested_message != nullptr);
  };

  void resolveDefinition(const std::map<std::string, std::shared_ptr<MessageFormat>>& existing_formats, int offset);

  void resolveDefinition(int offset);

 private:
  TypeAttributes _type;
  int _array_length{-1};  ///< -1 means not-an-array
  int _offset_in_message_bytes{-1}; ///< default to begin of message. Gets filled on MessageFormat resolution
  std::string _name;
};

class Value {
 public:
  using NativeTypeVariant = std::variant<
      int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t,
      float, double, bool, char, std::vector<int8_t>, std::vector<uint8_t>, std::vector<int16_t>,
      std::vector<uint16_t>, std::vector<int32_t>, std::vector<uint32_t>, std::vector<int64_t>,
      std::vector<uint64_t>, std::vector<float>, std::vector<double>, std::vector<bool>,
      std::string>;

  Value(const Field& field_ref, const std::vector<uint8_t>& backing_ref) : _field_ref(field_ref), _backing_ref(backing_ref) {}

  NativeTypeVariant asNativeTypeVariant() const;

  template <typename T>
  T as() const {
    T res;
    std::visit([](auto&& arg) {
      T res = static_cast<T>(arg);
    }, asNativeTypeVariant());
    return res;
  }

  template <typename T>
  operator T() {
    return as<T>();
  }

  private:
  const Field &_field_ref;
  const std::vector<uint8_t> &_backing_ref;

  template <typename T>
  static T deserialize(const std::vector<uint8_t>& backing, int offset)
  {
    T v;
    if (backing.size() - offset < sizeof(v)) throw ParsingException("Unexpected data type size");
    memcpy(&v, backing.data(), sizeof(v));
    return v;
  }

  template <typename T>
  static std::vector<T> deserializeVector(const std::vector<uint8_t>& backing, int offset, int size) {
    std::vector<T> res;
    res.resize(size);
    for (int i=0; i<size; i++) {
      res[i] = deserialize<T>(backing, offset + i * sizeof(T));
    }
    return res;
  }
};

class MessageInfo {
 public:
  explicit MessageInfo(const uint8_t* msg, bool is_multi = false);

  MessageInfo(Field field, std::vector<uint8_t> value, bool is_multi = false,
              bool continued = false);

  MessageInfo(const std::string& key, const std::string& value);
  MessageInfo(const std::string& key, int32_t value);
  MessageInfo(const std::string& key, float value);

  const Field& field() const { return _field; }
  std::vector<uint8_t>& valueRaw() { return _value; }
  const std::vector<uint8_t>& valueRaw() const { return _value; }
  Value value() const { return Value(_field, _value); }
  bool isContinued() const { return _continued; }
  bool isMulti() const { return _is_multi; }

  void serialize(const DataWriteCB& writer, ULogMessageType type = ULogMessageType::INFO) const;

  bool operator==(const MessageInfo& info) const
  {
    return _field == info._field && _value == info._value && _continued == info._continued &&
           _is_multi == info._is_multi;
  }

 private:
  void initValues(const char* values, int len);
  Field _field;
  std::vector<uint8_t> _value;
  bool _continued{false};
  bool _is_multi{false};
};

class MessageFormat {
 public:
  explicit MessageFormat(const uint8_t* msg);

  explicit MessageFormat(std::string name, const std::vector<Field> &fields);

  const std::string& name() const { return _name; }
  const std::map<std::string, std::shared_ptr<Field>>& fields() const { return _fields; }

  void serialize(const DataWriteCB& writer) const;
  bool operator==(const MessageFormat& format) const
  {
    return _name == format._name && _fields == format._fields;
  }

  void resolveDefinition(const std::map<std::string, std::shared_ptr<MessageFormat>>& existing_formats) const;

  int sizeBytes() const;

 private:
  std::string _name;
  std::map<std::string, std::shared_ptr<Field>> _fields;
  std::vector<std::shared_ptr<Field>> _fields_ordered;
};

using Parameter = MessageInfo;

class ParameterDefault {
 public:
  explicit ParameterDefault(const uint8_t* msg);

  ParameterDefault(Field field, std::vector<uint8_t> value,
                   ulog_parameter_default_type_t default_types);

  const Field& field() const { return _field; }
  const std::vector<uint8_t>& valueRaw() const { return _value; }
  Value value() const { return Value(_field, _value); }

  void serialize(const DataWriteCB& writer) const;

  ulog_parameter_default_type_t defaultType() const { return _default_types; }

 private:
  void initValues(const char* values, int len);
  Field _field;
  std::vector<uint8_t> _value;
  ulog_parameter_default_type_t _default_types{};
};

class AddLoggedMessage {
 public:
  explicit AddLoggedMessage(const uint8_t* msg);

  AddLoggedMessage(uint8_t multi_id, uint16_t msg_id, std::string message_name);

  const std::string& messageName() const { return _message_name; }
  uint8_t multiId() const { return _multi_id; }
  uint16_t msgId() const { return _msg_id; }

  void serialize(const DataWriteCB& writer) const;

 private:
  uint8_t _multi_id{};
  uint16_t _msg_id{};
  std::string _message_name;
};

class Logging {
 public:
  enum class Level : uint8_t {
    Emergency = '0',
    Alert = '1',
    Critical = '2',
    Error = '3',
    Warning = '4',
    Notice = '5',
    Info = '6',
    Debug = '7'
  };
  explicit Logging(const uint8_t* msg, bool is_tagged = false);

  Logging(Level level, std::string message, uint64_t timestamp);

  Level logLevel() const { return _log_level; }
  std::string logLevelStr() const;
  uint16_t tag() const { return _tag; }
  bool hasTag() const { return _has_tag; }
  uint64_t timestamp() const { return _timestamp; }
  const std::string& message() const { return _message; }

  void serialize(const DataWriteCB& writer) const;

  bool operator==(const Logging& logging) const
  {
    return _log_level == logging._log_level && _tag == logging._tag &&
           _has_tag == logging._has_tag && _timestamp == logging._timestamp &&
           _message == logging._message;
  }

 private:
  Level _log_level{};
  uint16_t _tag{};
  bool _has_tag{false};
  uint64_t _timestamp{};
  std::string _message;
};

class Data {
 public:
  explicit Data(const uint8_t* msg);

  Data(uint16_t msg_id, std::vector<uint8_t> data);

  uint16_t msgId() const { return _msg_id; }
  const std::vector<uint8_t>& data() const { return _data; }

  void serialize(const DataWriteCB& writer) const;

  bool operator==(const Data& data) const { return _msg_id == data._msg_id && _data == data._data; }

 private:
  uint16_t _msg_id{};
  std::vector<uint8_t> _data;
};

class Dropout {
 public:
  explicit Dropout(const uint8_t* msg);

  explicit Dropout(uint16_t duration_ms);

  uint16_t durationMs() const { return _duration_ms; }

  void serialize(const DataWriteCB& writer) const;

 private:
  uint16_t _duration_ms{};
};

class Sync {
 public:
  explicit Sync(const uint8_t* msg);

  explicit Sync() = default;

  void serialize(const DataWriteCB& writer) const;

 private:
  static constexpr uint8_t kSyncMagicBytes[] = {0x2F, 0x73, 0x13, 0x20, 0x25, 0x0C, 0xBB, 0x12};
};

}  // namespace ulog_cpp
