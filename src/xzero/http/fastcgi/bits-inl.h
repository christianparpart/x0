// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

namespace xzero {
namespace http {
namespace fastcgi {

// {{{
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
//}}}

inline Record::Record(
    Type type,
    uint16_t requestId,
    uint16_t contentLength,
    uint8_t paddingLength)
    : version_(1),
      type_(static_cast<uint8_t>(type)),
      requestId_(htons(requestId)),
      contentLength_(htons(contentLength)),
      paddingLength_(paddingLength),
      reserved_(0) {
}

// CgiParamStreamWriter
inline CgiParamStreamWriter::CgiParamStreamWriter() : buffer_() {
}

// CgiParamStreamReader
inline CgiParamStreamReader::CgiParamStreamReader()
    : name_(),
      value_() {
}

// AbortRequestRecord
inline AbortRequestRecord::AbortRequestRecord(uint16_t requestId)
    : Record(Type::AbortRequest, requestId, 0, 0) {
}

// UnknownTypeRecord
inline UnknownTypeRecord::UnknownTypeRecord(Type type, uint16_t requestId)
      : Record(Type::UnknownType, requestId, 8, 0),
        unknownType_(static_cast<uint8_t>(type)) {
  memset(&reserved_, 0, sizeof(reserved_));
}

inline int UnknownTypeRecord::unknownType() const noexcept {
  return unknownType_;
}

// EndRequestRecord
inline EndRequestRecord::EndRequestRecord(
    uint16_t requestId,
    uint32_t appStatus,
    ProtocolStatus protocolStatus)
    : Record(Type::EndRequest, requestId, 8, 0),
      appStatus_(appStatus),
      protocolStatus_(htonl(static_cast<uint8_t>(protocolStatus))) {
  reserved_[0] = 0;
  reserved_[1] = 0;
  reserved_[2] = 0;
}

inline uint32_t EndRequestRecord::appStatus() const {
  return ntohl(appStatus_);
}

inline ProtocolStatus EndRequestRecord::protocolStatus() const {
  return static_cast<ProtocolStatus>(protocolStatus_);
}

} // namespace fastcgi
} // namespace http
} // namespace xzero
