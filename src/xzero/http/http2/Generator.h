#pragma once

namespace xzero {

class EndPointWriter;

namespace http {
namespace http2 {

class Generator {
 public:
  Generator(EndPointWriter* writer);

  void generateData(StreamID sid, const BufferRef& chunk, bool last);
  void generateHeaders(StreamID sid, const HeaderFieldList& headers,
                       bool last);
  void generatePriority(StreamID sid, bool exclusive,
                        StreamID dependantStreamID, unsigned weight); 
  void generatePing(StreamID sid, ); 
  void generateGoAway(StreamID sid, ); 
  void generateResetStream(StreamID sid, ); 
  void generateSettings(StreamID sid, ); 
  void generatePushPromise(StreamID sid, const HttpResponseInfo&);
  void generateWindowUpdate(StreamID sid, size_t size);

  bool flush(EndPoint* output);

 private:
  EndPointWriter* writer_;
};

} // namespace http2
} // namespace http
} // namespace xzero
