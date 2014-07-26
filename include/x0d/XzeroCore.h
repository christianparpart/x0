// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_XzeroCore_h
#define sw_x0_XzeroCore_h 1

#include <x0d/Api.h>
#include <x0d/XzeroPlugin.h>
#include <xzero/HttpStatus.h>

namespace flow {
class Instr;
}

namespace x0d {

//! \addtogroup http
//@{

class XzeroDaemon;

class X0D_API XzeroCore : public XzeroPlugin {
 private:
  bool emitIR_;

 public:
  explicit XzeroCore(XzeroDaemon* d);
  ~XzeroCore();

  base::Property<unsigned long long> max_fds;

  virtual bool post_config();

  bool redirectOnIncompletePath(xzero::HttpRequest* r);

 private:
  // setup properties
  void plugin_directory(flow::vm::Params& args);
  void mimetypes(flow::vm::Params& args);
  void mimetypes_default(flow::vm::Params& args);
  void etag_mtime(flow::vm::Params& args);
  void etag_size(flow::vm::Params& args);
  void etag_inode(flow::vm::Params& args);
  void fileinfo_cache_ttl(flow::vm::Params& args);
  void server_advertise(flow::vm::Params& args);
  void server_tags(flow::vm::Params& args);

  void max_read_idle(flow::vm::Params& args);
  void max_write_idle(flow::vm::Params& args);
  void max_keepalive_idle(flow::vm::Params& args);
  void max_keepalive_requests(flow::vm::Params& args);
  void max_conns(flow::vm::Params& args);
  void max_files(flow::vm::Params& args);
  void max_address_space(flow::vm::Params& args);
  void max_core(flow::vm::Params& args);
  void tcp_cork(flow::vm::Params& args);
  void tcp_nodelay(flow::vm::Params& args);
  void lingering(flow::vm::Params& args);
  void max_request_uri_size(flow::vm::Params& args);
  void max_request_header_size(flow::vm::Params& args);
  void max_request_header_count(flow::vm::Params& args);
  void max_request_body_size(flow::vm::Params& args);
  void request_header_buffer_size(flow::vm::Params& args);
  void request_body_buffer_size(flow::vm::Params& args);

  // debugging
  void dump_ir(flow::vm::Params& args);

  // setup functions
  void listen(flow::vm::Params& args);
  void workers(flow::vm::Params& args);
  void workers_affinity(flow::vm::Params& args);

  // shared properties
  void systemd_controlled(xzero::HttpRequest* r, flow::vm::Params& args);
  void systemd_booted(xzero::HttpRequest* r, flow::vm::Params& args);
  void sys_cpu_count(xzero::HttpRequest* r, flow::vm::Params& args);
  void sys_env(xzero::HttpRequest* r, flow::vm::Params& args);
  void sys_cwd(xzero::HttpRequest* r, flow::vm::Params& args);
  void sys_pid(xzero::HttpRequest* r, flow::vm::Params& args);
  void sys_now(xzero::HttpRequest* r, flow::vm::Params& args);
  void sys_now_str(xzero::HttpRequest* r, flow::vm::Params& args);
  void sys_hostname(xzero::HttpRequest* r, flow::vm::Params& args);

  // shared functions
  void log_err(xzero::HttpRequest* r, flow::vm::Params& args);
  void log_warn(xzero::HttpRequest* r, flow::vm::Params& args);
  void log_notice(xzero::HttpRequest* r, flow::vm::Params& args);
  void log_info(xzero::HttpRequest* r, flow::vm::Params& args);
  void log_diag(xzero::HttpRequest* r, flow::vm::Params& args);
  void log_debug(xzero::HttpRequest* r, flow::vm::Params& args);
  void sleep(xzero::HttpRequest* r, flow::vm::Params& args);

  void file_exists(xzero::HttpRequest* r, flow::vm::Params& args);
  void file_is_reg(xzero::HttpRequest* r, flow::vm::Params& args);
  void file_is_dir(xzero::HttpRequest* r, flow::vm::Params& args);
  void file_is_exe(xzero::HttpRequest* r, flow::vm::Params& args);

  // main handlers
  bool verify_docroot(flow::Instr* call);
  bool docroot(xzero::HttpRequest* r, flow::vm::Params& args);
  bool alias(xzero::HttpRequest* r, flow::vm::Params& args);
  bool redirect_with_to(xzero::HttpRequest* r, flow::vm::Params& args);
  bool return_with(xzero::HttpRequest* r, flow::vm::Params& args);
  bool echo(xzero::HttpRequest* r, flow::vm::Params& args);
  bool blank(xzero::HttpRequest* r, flow::vm::Params& args);
  bool staticfile(xzero::HttpRequest* r, flow::vm::Params& args);
  bool precompressed(xzero::HttpRequest* r, flow::vm::Params& args);

  // main functions
  void autoindex(xzero::HttpRequest* r, flow::vm::Params& args);
  bool matchIndex(xzero::HttpRequest* r, const flow::BufferRef& arg);
  void rewrite(xzero::HttpRequest* r, flow::vm::Params& args);
  void pathinfo(xzero::HttpRequest* r, flow::vm::Params& args);
  void error_handler(xzero::HttpRequest* r, flow::vm::Params& args);
  void header_add(xzero::HttpRequest* r, flow::vm::Params& args);
  void header_append(xzero::HttpRequest* r, flow::vm::Params& args);
  void header_overwrite(xzero::HttpRequest* r, flow::vm::Params& args);
  void header_remove(xzero::HttpRequest* r, flow::vm::Params& args);

  // main properties
  void req_method(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_url(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_path(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_query(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_header(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_cookie(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_host(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_pathinfo(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_is_secure(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_scheme(xzero::HttpRequest* r, flow::vm::Params& args);
  void req_status_code(xzero::HttpRequest* r, flow::vm::Params& args);
  void conn_remote_ip(xzero::HttpRequest* r, flow::vm::Params& args);
  void conn_remote_port(xzero::HttpRequest* r, flow::vm::Params& args);
  void conn_local_ip(xzero::HttpRequest* r, flow::vm::Params& args);
  void conn_local_port(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_path(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_exists(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_is_reg(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_is_dir(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_is_exe(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_mtime(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_size(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_etag(xzero::HttpRequest* r, flow::vm::Params& args);
  void phys_mimetype(xzero::HttpRequest* r, flow::vm::Params& args);
  void regex_group(xzero::HttpRequest* in, flow::vm::Params& args);

  void req_accept_language(xzero::HttpRequest* r, flow::vm::Params& args);
  bool verify_req_accept_language(flow::Instr* call);

  // {{{ legacy
 private:
  unsigned long long getrlimit(int resource);
  unsigned long long setrlimit(int resource, unsigned long long max);
  // }}}
};

//@}

}  // namespace x0

#endif
