// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "HaproxyApi.h"
#include "Director.h"
#include "Backend.h"

#include <base/io/BufferSource.h>
#include <base/Tokenizer.h>
#include <base/Url.h>

using namespace base;
using namespace xzero;

HaproxyApi::HaproxyApi(DirectorMap* directors) : directors_(directors) {}

HaproxyApi::~HaproxyApi() {}

void HaproxyApi::monitor(HttpRequest* r) {
  r->responseHeaders.push_back("Content-Type", "text/html");
  r->responseHeaders.push_back("Cache-Control", "no-cache");
  r->write<BufferSource>(
      "<html><body><h1>200 OK</h1>\r\nHAProxy: service "
      "ready.\r\n</body></html>\r\n");
  r->finish();
}

void HaproxyApi::stats(HttpRequest* r, const std::string& prefix) { csv(r); }

void HaproxyApi::buildFrontendCSV(Buffer& buf, Director* director) {
  // [01] pxname
  buf << director->name() << ',';
  // [02] svname
  buf << "FRONTEND,";
  // [03] qcur
  buf << director->queued().current() << ',';
  // [04] qmax
  buf << director->queued().max() << ',';
  // [05] scur
  buf << director->load().current() << ',';
  // [06] smax
  buf << director->load().max() << ',';
  // [07] slim
  buf << ',';
  // [08] stot
  buf << director->load().total() << ',';
  // [09] bin
  buf << ',';
  // [10] bout
  buf << ',';
  // [11] dreq
  buf << ',';
  // [12] dresp
  buf << ',';
  // [13] ereq
  buf << ',';
  // [14] econ
  buf << ',';
  // [15] eresp
  buf << ',';
  // [16] wretr
  buf << ',';
  // [17] wredis
  buf << ',';
  // [18] status
  buf << "OPEN,";
  // [19] weight
  buf << ',';
  // [20] act
  buf << ',';
  // [21] bck
  buf << ',';
  // [22] chkfail
  buf << ',';
  // [23] chkdown
  buf << ',';
  // [24] lastchg
  buf << ',';
  // [25] downtime
  buf << ',';
  // [26] qlimit
  buf << ',';
  // [27] pid
  buf << ',';
  // [28] iid
  buf << ',';
  // [29] sid
  buf << ',';
  // [30] throttle
  buf << ',';
  // [31] lbtot
  buf << ',';
  // [32] tracked
  buf << ',';
  // [33] type
  buf << ',';
  // [34] rate
  buf << ',';
  // [35] rate_lim
  buf << ',';
  // [36] rate_max
  buf << ',';
  // [37] check_status
  buf << ',';
  // [38] check_code
  buf << ',';
  // [39] check_duration
  buf << ',';
  // [40] hrsp_1xx
  buf << ',';
  // [41] hrsp_2xx
  buf << ',';
  // [42] hrsp_3xx
  buf << ',';
  // [43] hrsp_4xx
  buf << ',';
  // [44] hrsp_5xx
  buf << ',';
  // [45] hrsp_other
  buf << ',';
  // [46] hanafail
  buf << ',';
  // [47] req_rate
  buf << ',';
  // [48] req_rate_max
  buf << ',';
  // [49] req_tot
  buf << ',';
  // [50] cli_abrt
  buf << ',';
  // [51] srv_abrt
  buf << ',';

  buf << '\n';
}

/*
 * FIELD DESCRIPTION:
 * https://code.google.com/p/haproxy-docs/wiki/StatisticsMonitoring
 */

void HaproxyApi::csv(HttpRequest* r) {
  Buffer buf;

  buf.push_back(
      "# "
      "pxname,svname,qcur,qmax,scur,smax,slim,stot,bin,bout,dreq,dresp,ereq,"
      "econ,eresp,wretr,wredis,status,weight,act,bck,chkfail,chkdown,lastchg,"
      "downtime,qlimit,pid,iid,sid,throttle,lbtot,tracked,type,rate,rate_lim,"
      "rate_max,check_status,check_code,check_duration,hrsp_1xx,hrsp_2xx,hrsp_"
      "3xx,hrsp_4xx,hrsp_5xx,hrsp_other,hanafail,req_rate,req_rate_max,req_tot,"
      "cli_abrt,srv_abrt,\n");

  for (auto& di : *directors_) {
    Director* director = di.second.get();

    buildFrontendCSV(buf, director);

    director->eachBackend([&](Backend* backend) {
      // 01 pxname: proxy name
      buf.push_back(director->name());
      buf.push_back(',');

      // 02 svname: service name (FRONTEND for frontend, BACKEND for backend,
      // any name for server)
      buf.push_back(backend->name());  // svname

      // 03 qcur: current queued requests
      // 04 qmax: max queued requests
      buf.push_back(",0,0,");  // qcur, qmax

      // 05 scur: current sessions
      buf.push_back(backend->load().current());
      buf.push_back(',');

      // 06 smax: max sessions
      buf.push_back(backend->load().max());
      buf.push_back(",");

      // 07 slim: sessions limit
      buf.push_back(",");

      // 08 stot: total sessions
      buf.push_back(backend->load().total());

      // 09 bin: bytes in
      // 10 bout: bytes out
      buf.push_back(",0,0,");  // bin, bout

      // 11 dreq: denied requests
      // 12 dresp: denied responses
      buf.push_back("0,0,");  // dreq, dresp

      // 13 ereq: request errors
      // 14 econ: connection errors
      // 15 eresp: response errors (among which srv_abrt)
      buf.push_back("0,0,0,");  // ereq, econ, resp

      // 16 wretr: retries (warning)
      // 17 wredis: redispatches (warning)
      buf.push_back("0,0,");  // wretr, wredis

      // 18 status
      if (!backend->isEnabled()) {  // status
        buf.push_back("MAINT");
      } else if (backend->healthMonitor()) {
        switch (backend->healthMonitor()->state()) {
          case HealthState::Online:
            buf.push_back("UP");
            break;
          case HealthState::Offline:
            buf.push_back("DOWN");
            break;
          case HealthState::Undefined:
            buf.push_back("UNKNOWN");
            break;
        }
      } else {
        buf.push_back("UP");
      }

      // weight: server weight (server), total weight (backend)
      buf.push_back(",0,");  // weight

      // 20 act: server is active (server), number of active servers (backend)
      // 21 bck: server is backup (server), number of backup servers (backend)
      switch (director->backendRole(backend)) {  // act, bck
        case BackendRole::Active:
          buf.push_back("1,0,");
          break;
        case BackendRole::Backup:
          buf.push_back("0,1,");
          break;
        case BackendRole::Terminate:
          buf.push_back("0,0,");
          break;
      }
      // 22 chkfail: number of failed checks
      buf.push_back(',');  // TODO

      // 23 chkdown: number of UP->DOWN transitions
      buf.push_back(',');  // TODO

      // 24 lastchg: last status change (in seconds)
      buf.push_back(',');  // TODO

      // 25 downtime: total downtime (in seconds)
      buf.push_back(',');  // TODO

      // 26 qlimit: queue limit
      buf.push_back(',');

      // 27 pid: process id (0 for first instance, 1 for second, ...)
      buf.push_back("0,");

      // 28 iid: unique proxy id
      buf.push_back(',');

      // 29 sid: service id (unique inside a proxy)
      buf.push_back(',');

      // 30 throttle: warm up status
      buf.push_back(',');

      // 31 lbtot: total number of times a server was selected
      buf.push_back(',');  // TODO

      // 32 tracked: id of proxy/server if tracking is enabled
      buf.push_back(',');

      // 33 type (0=frontend, 1=backend, 2=server, 3=socket)
      buf.push_back("2,");

      // 34 rate: number of sessions per second over last elapsed second
      buf.push_back(',');  // TODO

      // 35 rate_lim: limit on new sessions per second
      buf.push_back(',');

      // 36 rate_max: max number of new sessions per second
      buf.push_back(',');

      // 37 check_status: status of last health check, one of:
      // - UNK -> unknown
      // - INI -> initializing
      // - SOCKERR -> socket error
      // - L4OK -> check passed on layer 4, no upper layers testing enabled
      // - L4TMOUT -> layer 1-4 timeout
      // - L4CON -> layer 1-4 connection problem, for example "Connection
      // refused" (tcp rst) or "No route to host" (icmp)
      // - L6OK -> check passed on layer 6
      // - L6TOUT -> layer 6 (SSL) timeout
      // - L6RSP -> layer 6 invalid response - protocol error
      // - L7OK -> check passed on layer 7
      // - L7OKC -> check conditionally passed on layer 7, for example 404 with
      // disable-on-404
      // - L7TOUT -> layer 7 (HTTP/SMTP) timeout
      // - L7RSP -> layer 7 invalid response - protocol error
      // - L7STS -> layer 7 response error, for example HTTP 5xx
      buf.push_back("UNK,");  // TODO

      // 38 check_code: layer5-7 code, if available
      buf.push_back(',');

      // 39 check_duration: time in ms took to finish last health check
      buf.push_back(',');  // TODO

      // 40 hrsp_1xx: http responses with 1xx code
      buf.push_back(',');  // TODO

      // 41 hrsp_2xx: http responses with 2xx code
      buf.push_back(',');  // TODO

      // 42 hrsp_3xx: http responses with 3xx code
      buf.push_back(',');  // TODO

      // 43 hrsp_4xx: http responses with 4xx code
      buf.push_back(',');  // TODO

      // 44 hrsp_5xx: http responses with 5xx code
      buf.push_back(',');  // TODO

      // 45 hrsp_other: http responses with other codes (protocol error)
      buf.push_back(',');  // TODO

      // 46 hanafail: failed health checks details
      buf.push_back(',');  // TODO

      // 47 req_rate: HTTP requests per second over last elapsed second
      buf.push_back(',');

      // 48 req_rate_max: max number of HTTP requests per second observed
      buf.push_back(',');

      // 49 req_tot: total number of HTTP requests received
      buf.push_back(backend->load().total());
      buf.push_back(',');

      // 50 cli_abrt: number of data transfers aborted by the client
      buf.push_back(',');  // TODO

      // 51 srv_abrt: number of data transfers aborted by the server (inc. in
      // eresp)
      buf.push_back(',');  // TODO

      buf.push_back('\n');
    });
  }

  char slen[32];
  snprintf(slen, sizeof(slen), "%zu", buf.size());
  r->responseHeaders.push_back("Content-Length", slen);
  r->responseHeaders.push_back("Content-Type",
                               "text/plain");  // HAproxy software does sent
                                               // this one instead of text/csv.
  r->responseHeaders.push_back("Cache-Control", "no-cache");
  r->write<BufferSource>(buf);
  r->finish();
}
