// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

// this file holds the protocol bits

#include <stdint.h>     // uint16_t, ...
#include <string.h>     // memset
#include <arpa/inet.h>  // htons/ntohs/ntohl/htonl

#include <string>
#include <vector>

#include <base/Buffer.h>

namespace FastCgi {

enum class Type {
  BeginRequest = 1,
  AbortRequest = 2,
  EndRequest = 3,
  Params = 4,
  StdIn = 5,
  StdOut = 6,
  StdErr = 7,
  Data = 8,
  GetValues = 9,
  GetValuesResult = 10,
  UnknownType = 11
};

enum class Role { Responder = 1, Authorizer = 2, Filter = 3 };

enum class ProtocolStatus {
  RequestComplete = 0,
  CannotMpxConnection = 1,
  Overloaded = 2,
  UnknownRole = 3
};

struct Record {
 protected:
  uint8_t version_;
  uint8_t type_;
  uint16_t requestId_;
  uint16_t contentLength_;
  uint8_t paddingLength_;
  uint8_t reserved_;

 public:
  Record(Type type, uint16_t requestId, uint16_t contentLength,
         uint8_t paddingLength)
      : version_(1),
        type_(static_cast<uint8_t>(type)),
        requestId_(htons(requestId)),
        contentLength_(htons(contentLength)),
        paddingLength_(paddingLength),
        reserved_(0) {}

  // static Record *create(Type type, uint16_t requestId, uint16_t
  // contentLength);
  // void destroy();

  int version() const { return version_; }
  Type type() const { return static_cast<Type>(type_); }
  int requestId() const { return ntohs(requestId_); }
  int contentLength() const { return ntohs(contentLength_); }
  int paddingLength() const { return paddingLength_; }

  const char *content() const {
    return reinterpret_cast<const char *>(this) + sizeof(*this);
  }

  const char *data() const { return reinterpret_cast<const char *>(this); }
  uint32_t size() const {
    return sizeof(Record) + contentLength() + paddingLength();
  }

  bool isManagement() const { return requestId() == 0; }
  bool isApplication() const { return requestId() != 0; }

  const char *type_str() const {
    static const char *map[] = {0,            "BeginRequest",    "AbortRequest",
                                "EndRequest", "Params",          "StdIn",
                                "StdOut",     "StdErr",          "Data",
                                "GetValues",  "GetValuesResult", "UnknownType"};

    return type_ > 0 && type_ < 12 ? map[type_] : "invalid";
  }
};

struct BeginRequestRecord : public Record {
 private:
  uint16_t role_;
  uint8_t flags_;
  uint8_t reserved_[5];

 public:
  BeginRequestRecord(Role role, uint16_t requestId, bool keepAlive)
      : Record(Type::BeginRequest, requestId, 8, 0),
        role_(htons(static_cast<uint16_t>(role))),
        flags_(keepAlive ? 0x01 : 0x00) {
    memset(reserved_, 0, sizeof(reserved_));
  }

  Role role() const { return static_cast<Role>(ntohs(role_)); }

  bool isKeepAlive() const { return flags_ & 0x01; }

  const char *role_str() const {
    switch (role()) {
      case Role::Responder:
        return "responder";
      case Role::Authorizer:
        return "authorizer";
      case Role::Filter:
        return "filter";
      default:
        return "invalid";
    }
  }
};

/** generates a PARAM stream. */
class CgiParamStreamWriter {
 private:
  base::Buffer buffer_;

  inline void encodeLength(size_t length);

 public:
  CgiParamStreamWriter();

  void encode(const char *name, size_t nameLength, const char *value,
              size_t valueLength);
  void encode(const char *name, size_t nameLength, const char *v1, size_t l1,
              const char *v2, size_t l2);

  void encode(const std::string &name, const std::string &value) {
    encode(name.data(), name.size(), value.data(), value.size());
  }
  void encode(const base::BufferRef &name, const base::BufferRef &value) {
    encode(name.data(), name.size(), value.data(), value.size());
  }
  void encode(const std::string &name, const base::BufferRef &value) {
    encode(name.data(), name.size(), value.data(), value.size());
  }

  template <typename V1, typename V2>
  void encode(const std::string &name, const V1 &v1, const V2 &v2) {
    encode(name.data(), name.size(), v1.data(), v1.size(), v2.data(),
           v2.size());
  }

  template <typename PodType, std::size_t N, typename ValuePType,
            std::size_t N2>
  void encode(PodType (&name)[N], const ValuePType (&value)[N2]) {
    encode(name, N - 1, value, N2 - 1);
  }

  template <typename PodType, std::size_t N, typename ValueType>
  void encode(PodType (&name)[N], const ValueType &value) {
    encode(name, N - 1, value.data(), value.size());
  }

  base::Buffer &&output() { return std::move(buffer_); }
};

/** parses a PARAM stream and reads out name/value paris. */
class CgiParamStreamReader {
 protected:
  enum { NAME, VALUE } state_;

  size_t length_;            // remaining length to read for current token
  std::vector<char> name_;   // name token
  std::vector<char> value_;  // value token

 public:
  CgiParamStreamReader();

  void processParams(const char *buf, size_t length);
  virtual void onParam(const char *name, size_t nameLen, const char *value,
                       size_t valueLen) = 0;
};

struct AbortRequestRecord : public Record {
 public:
  AbortRequestRecord(uint16_t requestId)
      : Record(Type::AbortRequest, requestId, 0, 0) {}
};

struct UnknownTypeRecord : public Record {
 protected:
  uint8_t unknownType_;
  uint8_t reserved_[7];

 public:
  UnknownTypeRecord(Type type, uint16_t requestId)
      : Record(Type::UnknownType, requestId, 8, 0),
        unknownType_(static_cast<uint8_t>(type)) {
    memset(&reserved_, 0, sizeof(reserved_));
  }
};

struct EndRequestRecord : public Record {
 private:
  uint32_t appStatus_;
  uint8_t protocolStatus_;
  uint8_t reserved_[3];

 public:
  EndRequestRecord(uint16_t requestId, uint32_t appStatus,
                   ProtocolStatus protocolStatus)
      : Record(Type::EndRequest, requestId, 8, 0),
        appStatus_(appStatus),
        protocolStatus_(htonl(static_cast<uint8_t>(protocolStatus))) {
    reserved_[0] = 0;
    reserved_[1] = 0;
    reserved_[2] = 0;
  }

  uint32_t appStatus() const { return ntohl(appStatus_); }
  ProtocolStatus protocolStatus() const {
    return static_cast<ProtocolStatus>(protocolStatus_);
  }
};

// {{{ inlines
// Record
// inline Record *Record::create(Type type, uint16_t requestId, uint16_t
// contentLength)
//{
//	int paddingLength = (sizeof(Record) + contentLength) % 8;
//	char *p = new char[sizeof(Record) + contentLength + paddingLength];
//	Record *r = new (p) Record(type, requestId, contentLength, paddingLength);
//	return r;
//}

// inline void Record::destroy()
//{
//	delete[] this;
//}

// CgiParamStreamWriter
inline CgiParamStreamWriter::CgiParamStreamWriter() : buffer_() {}

inline void CgiParamStreamWriter::encodeLength(size_t length) {
  if (length < 127)
    buffer_.push_back(static_cast<char>(length));
  else {
    unsigned char value[4];
    value[0] = ((length >> 24) & 0xFF) | 0x80;
    value[1] = (length >> 16) & 0xFF;
    value[2] = (length >> 8) & 0xFF;
    value[3] = length & 0xFF;

    buffer_.push_back(&value, sizeof(value));
  }
}

inline void CgiParamStreamWriter::encode(const char *name, size_t nameLength,
                                         const char *value,
                                         size_t valueLength) {
  encodeLength(nameLength);
  encodeLength(valueLength);

  buffer_.push_back(name, nameLength);
  buffer_.push_back(value, valueLength);
}

inline void CgiParamStreamWriter::encode(const char *name, size_t nameLength,
                                         const char *v1, size_t l1,
                                         const char *v2, size_t l2) {
  encodeLength(nameLength);
  encodeLength(l1 + l2);

  buffer_.push_back(name, nameLength);
  buffer_.push_back(v1, l1);
  buffer_.push_back(v2, l2);
}

// CgiParamStreamReader
inline CgiParamStreamReader::CgiParamStreamReader()
    : state_(NAME), length_(0), name_(), value_() {}

inline void CgiParamStreamReader::processParams(const char *buf,
                                                size_t length) {
  const char *i = buf;
  size_t pos = 0;

  while (pos < length) {
    size_t nameLength = 0;
    size_t valueLength = 0;

    if ((*i >> 7) == 0) {  // 11 | 14
      nameLength = *i & 0xFF;
      ++pos;
      ++i;

      if ((*i >> 7) == 0) {  // 11
        valueLength = *i & 0xFF;
        ++pos;
        ++i;
      } else {  // 14
        valueLength = (((i[0] & ~(1 << 7)) & 0xFF) << 24) +
                      ((i[1] & 0xFF) << 16) + ((i[2] & 0xFF) << 8) +
                      ((i[3] & 0xFF));
        pos += 4;
        i += 4;
      }
    } else {  // 41 || 44
      nameLength = (((i[0] & ~(1 << 7)) & 0xFF) << 24) + ((i[1] & 0xFF) << 16) +
                   ((i[2] & 0xFF) << 8) + ((i[3] & 0xFF));
      pos += 4;
      i += 4;

      if ((*i >> 7) == 0) {  // 41
        valueLength = *i;
        ++pos;
        ++i;
      } else {  // 44
        valueLength = (((i[0] & ~(1 << 7)) & 0xFF) << 24) +
                      ((i[1] & 0xFF) << 16) + ((i[2] & 0xFF) << 8) +
                      ((i[3] & 0xFF));
        pos += 4;
        i += 4;
      }
    }

    const char *name = i;
    pos += nameLength;
    i += nameLength;

    const char *value = i;
    pos += valueLength;
    i += valueLength;

    onParam(name, nameLength, value, valueLength);
  }
}
// }}}

}  // namespace FastCGI
