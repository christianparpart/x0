// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/Module.h>
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

class CoreModule : public Module {
 public:
  explicit CoreModule(Daemon* d);
  ~CoreModule();

  static size_t cpuCount();

 private:
  xzero::Random rng_;

  // helper
  bool redirectOnIncompletePath(Context* cx);
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
  void sys_cpu_count(Context* cx, Params& args);
  bool preproc_sys_env(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
  void sys_env(Context* cx, Params& args);
  bool preproc_sys_env2(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
  void sys_env2(Context* cx, Params& args);
  void sys_cwd(Context* cx, Params& args);
  void sys_pid(Context* cx, Params& args);
  void sys_now(Context* cx, Params& args);
  void sys_now_str(Context* cx, Params& args);
  void sys_hostname(Context* cx, Params& args);
  void sys_domainname(Context* cx, Params& args);
  void sys_max_conn(Context* cx, Params& args);

  // {{{ shared functions
  void error_page_(Params& args);
  void error_page(Context* cx, Params& args);
  void log_err(Context* cx, Params& args);
  void log_warn(Context* cx, Params& args);
  void log_notice(Context* cx, Params& args);
  void log_info(Context* cx, Params& args);
  void log_diag(Context* cx, Params& args);
  void log_debug(Context* cx, Params& args);
  void sleep(Context* cx, Params& args);
  void rand(Context* cx, Params& args);
  void randAB(Context* cx, Params& args);

  void file_exists(Context* cx, Params& args);
  void file_is_reg(Context* cx, Params& args);
  void file_is_dir(Context* cx, Params& args);
  void file_is_exe(Context* cx, Params& args);
  // }}}

  // main handlers
  bool verify_docroot(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
  bool docroot(Context* cx, Params& args);
  bool alias(Context* cx, Params& args);
  bool redirect_with_to(Context* cx, Params& args);
  bool return_with(Context* cx, Params& args);
  bool echo(Context* cx, Params& args);
  bool blank(Context* cx, Params& args);
  bool staticfile(Context* cx, Params& args);
  bool precompressed(Context* cx, Params& args);

  // main functions
  void autoindex(Context* cx, Params& args);
  bool matchIndex(Context* cx, const std::string& arg);
  void rewrite(Context* cx, Params& args);
  void pathinfo(Context* cx, Params& args);
  void header_add(Context* cx, Params& args);
  void header_append(Context* cx, Params& args);
  void header_overwrite(Context* cx, Params& args);
  void header_remove(Context* cx, Params& args);
  void expire(Context* cx, Params& args);

  // main properties
  void req_method(Context* cx, Params& args);
  void req_url(Context* cx, Params& args);
  void req_path(Context* cx, Params& args);
  void req_query(Context* cx, Params& args);
  void req_header(Context* cx, Params& args);
  void req_cookie(Context* cx, Params& args);
  void req_host(Context* cx, Params& args);
  void req_pathinfo(Context* cx, Params& args);
  void req_is_secure(Context* cx, Params& args);
  void req_scheme(Context* cx, Params& args);
  void req_status_code(Context* cx, Params& args);
  void conn_remote_ip(Context* cx, Params& args);
  void conn_remote_port(Context* cx, Params& args);
  void conn_local_ip(Context* cx, Params& args);
  void conn_local_port(Context* cx, Params& args);
  void phys_path(Context* cx, Params& args);
  void phys_exists(Context* cx, Params& args);
  void phys_is_reg(Context* cx, Params& args);
  void phys_is_dir(Context* cx, Params& args);
  void phys_is_exe(Context* cx, Params& args);
  void phys_mtime(Context* cx, Params& args);
  void phys_size(Context* cx, Params& args);
  void phys_etag(Context* cx, Params& args);
  void phys_mimetype(Context* cx, Params& args);
  void regex_group(Context* cx, Params& args);
  void req_accept_language(Context* cx, Params& args);
  bool verify_req_accept_language(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
};

} // namespace x0d
