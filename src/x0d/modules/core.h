// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/XzeroModule.h>
#include <xzero/MimeTypes.h>
#include <xzero/Random.h>
#include <xzero/logging.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero-flow/AST.h>
#include <xzero-flow/ir/Instr.h>
#include <xzero-flow/ir/BasicBlock.h>
#include <xzero-flow/ir/IRHandler.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/IRBuilder.h>
#include <xzero-flow/ir/ConstantValue.h>
#include <xzero-flow/ir/ConstantArray.h>

namespace x0d {

class CoreModule : public XzeroModule {
 public:
  explicit CoreModule(XzeroDaemon* d);
  ~CoreModule();

  static size_t cpuCount();

 private:
  xzero::Random rng_;

  // helper
  bool redirectOnIncompletePath(XzeroContext* cx);
  unsigned long long setrlimit(int resource, unsigned long long value);

  // setup properties
  void mimetypes(Params& args);
  void mimetypes_default(Params& args);
  void etag_mtime(Params& args);
  void etag_size(Params& args);
  void etag_inode(Params& args);
  void fileinfo_cache_ttl(Params& args);
  void server_advertise(Params& args);
  void server_tags(Params& args);

  void tcp_fin_timeout(Params& args);
  void max_internal_redirect_count(Params& args);
  void max_read_idle(Params& args);
  void max_write_idle(Params& args);
  void max_keepalive_idle(Params& args);
  void max_keepalive_requests(Params& args);
  void max_conns(Params& args);
  void max_files(Params& args);
  void max_address_space(Params& args);
  void max_core(Params& args);
  void tcp_cork(Params& args);
  void tcp_nodelay(Params& args);
  void lingering(Params& args);
  void max_request_uri_size(Params& args);
  void max_request_header_size(Params& args);
  void max_request_header_count(Params& args);
  void max_request_body_size(Params& args);
  void request_header_buffer_size(Params& args);
  void request_body_buffer_size(Params& args);
  void response_body_buffer_size(Params& args);

  // setup functions
  void listen(Params& args);
  void workers(Params& args);
  void workers_affinity(Params& args);
  void ssl_listen(Params& args);
  void ssl_priorities(Params& args);
  void ssl_context(Params& args);

  // shared properties
  void sys_cpu_count(XzeroContext* cx, Params& args);
  bool preproc_sys_env(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
  void sys_env(XzeroContext* cx, Params& args);
  bool preproc_sys_env2(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
  void sys_env2(XzeroContext* cx, Params& args);
  void sys_cwd(XzeroContext* cx, Params& args);
  void sys_pid(XzeroContext* cx, Params& args);
  void sys_now(XzeroContext* cx, Params& args);
  void sys_now_str(XzeroContext* cx, Params& args);
  void sys_hostname(XzeroContext* cx, Params& args);
  void sys_domainname(XzeroContext* cx, Params& args);

  // {{{ shared functions
  void error_page(Params& args);
  void error_page(XzeroContext* cx, Params& args);
  void log_err(XzeroContext* cx, Params& args);
  void log_warn(XzeroContext* cx, Params& args);
  void log_notice(XzeroContext* cx, Params& args);
  void log_info(XzeroContext* cx, Params& args);
  void log_diag(XzeroContext* cx, Params& args);
  void log_debug(XzeroContext* cx, Params& args);
  void sleep(XzeroContext* cx, Params& args);
  void rand(XzeroContext* cx, Params& args);
  void randAB(XzeroContext* cx, Params& args);

  void file_exists(XzeroContext* cx, Params& args);
  void file_is_reg(XzeroContext* cx, Params& args);
  void file_is_dir(XzeroContext* cx, Params& args);
  void file_is_exe(XzeroContext* cx, Params& args);
  // }}}

  // main handlers
  bool verify_docroot(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
  bool docroot(XzeroContext* cx, Params& args);
  bool alias(XzeroContext* cx, Params& args);
  bool redirect_with_to(XzeroContext* cx, Params& args);
  bool return_with(XzeroContext* cx, Params& args);
  bool echo(XzeroContext* cx, Params& args);
  bool blank(XzeroContext* cx, Params& args);
  bool staticfile(XzeroContext* cx, Params& args);
  bool precompressed(XzeroContext* cx, Params& args);

  // main functions
  void autoindex(XzeroContext* cx, Params& args);
  bool matchIndex(XzeroContext* cx, const xzero::BufferRef& arg);
  void rewrite(XzeroContext* cx, Params& args);
  void pathinfo(XzeroContext* cx, Params& args);
  void header_add(XzeroContext* cx, Params& args);
  void header_append(XzeroContext* cx, Params& args);
  void header_overwrite(XzeroContext* cx, Params& args);
  void header_remove(XzeroContext* cx, Params& args);
  void expire(XzeroContext* cx, Params& args);

  // main properties
  void req_method(XzeroContext* cx, Params& args);
  void req_url(XzeroContext* cx, Params& args);
  void req_path(XzeroContext* cx, Params& args);
  void req_query(XzeroContext* cx, Params& args);
  void req_header(XzeroContext* cx, Params& args);
  void req_cookie(XzeroContext* cx, Params& args);
  void req_host(XzeroContext* cx, Params& args);
  void req_pathinfo(XzeroContext* cx, Params& args);
  void req_is_secure(XzeroContext* cx, Params& args);
  void req_scheme(XzeroContext* cx, Params& args);
  void req_status_code(XzeroContext* cx, Params& args);
  void conn_remote_ip(XzeroContext* cx, Params& args);
  void conn_remote_port(XzeroContext* cx, Params& args);
  void conn_local_ip(XzeroContext* cx, Params& args);
  void conn_local_port(XzeroContext* cx, Params& args);
  void phys_path(XzeroContext* cx, Params& args);
  void phys_exists(XzeroContext* cx, Params& args);
  void phys_is_reg(XzeroContext* cx, Params& args);
  void phys_is_dir(XzeroContext* cx, Params& args);
  void phys_is_exe(XzeroContext* cx, Params& args);
  void phys_mtime(XzeroContext* cx, Params& args);
  void phys_size(XzeroContext* cx, Params& args);
  void phys_etag(XzeroContext* cx, Params& args);
  void phys_mimetype(XzeroContext* cx, Params& args);
  void regex_group(XzeroContext* cx, Params& args);
  void req_accept_language(XzeroContext* cx, Params& args);
  bool verify_req_accept_language(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
};

} // namespace x0d
