// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_XzeroCore_h
#define sw_x0_XzeroCore_h 1

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpStatus.h>
#include <x0/Api.h>

namespace x0 {
    class Instr;
}

namespace x0d {

//! \addtogroup http
//@{

class XzeroDaemon;

class X0_API XzeroCore :
    public XzeroPlugin
{
private:
    bool emitIR_;

public:
    explicit XzeroCore(XzeroDaemon* d);
    ~XzeroCore();

    x0::Property<unsigned long long> max_fds;

    virtual bool post_config();

    bool redirectOnIncompletePath(x0::HttpRequest *r);

private:
    // setup properties
    void plugin_directory(x0::FlowVM::Params& args);
    void mimetypes(x0::FlowVM::Params& args);
    void mimetypes_default(x0::FlowVM::Params& args);
    void etag_mtime(x0::FlowVM::Params& args);
    void etag_size(x0::FlowVM::Params& args);
    void etag_inode(x0::FlowVM::Params& args);
    void fileinfo_cache_ttl(x0::FlowVM::Params& args);
    void server_advertise(x0::FlowVM::Params& args);
    void server_tags(x0::FlowVM::Params& args);

    void max_read_idle(x0::FlowVM::Params& args);
    void max_write_idle(x0::FlowVM::Params& args);
    void max_keepalive_idle(x0::FlowVM::Params& args);
    void max_keepalive_requests(x0::FlowVM::Params& args);
    void max_conns(x0::FlowVM::Params& args);
    void max_files(x0::FlowVM::Params& args);
    void max_address_space(x0::FlowVM::Params& args);
    void max_core(x0::FlowVM::Params& args);
    void tcp_cork(x0::FlowVM::Params& args);
    void tcp_nodelay(x0::FlowVM::Params& args);
    void lingering(x0::FlowVM::Params& args);
    void max_request_uri_size(x0::FlowVM::Params& args);
    void max_request_header_size(x0::FlowVM::Params& args);
    void max_request_header_count(x0::FlowVM::Params& args);
    void max_request_body_size(x0::FlowVM::Params& args);
    void request_header_buffer_size(x0::FlowVM::Params& args);
    void request_body_buffer_size(x0::FlowVM::Params& args);

    // debugging
    void dump_ir(x0::FlowVM::Params& args);

    // setup functions
    void listen(x0::FlowVM::Params& args);
    void workers(x0::FlowVM::Params& args);
    void workers_affinity(x0::FlowVM::Params& args);

    // shared properties
    void systemd_controlled(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void systemd_booted(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void sys_cpu_count(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void sys_env(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void sys_cwd(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void sys_pid(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void sys_now(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void sys_now_str(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void sys_hostname(x0::HttpRequest* r, x0::FlowVM::Params& args);

    // shared functions
    void log_err(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void log_warn(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void log_notice(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void log_info(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void log_diag(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void log_debug(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void sleep(x0::HttpRequest* r, x0::FlowVM::Params& args);

    void file_exists(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void file_is_reg(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void file_is_dir(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void file_is_exe(x0::HttpRequest* r, x0::FlowVM::Params& args);

    // main handlers
    bool verify_docroot(x0::Instr* call);
    bool docroot(x0::HttpRequest* r, x0::FlowVM::Params& args);
    bool alias(x0::HttpRequest* r, x0::FlowVM::Params& args);
    bool redirect_with_to(x0::HttpRequest *r, x0::FlowVM::Params& args);
    bool return_with(x0::HttpRequest *r, x0::FlowVM::Params& args);
    bool echo(x0::HttpRequest *r, x0::FlowVM::Params& args);
    bool blank(x0::HttpRequest *r, x0::FlowVM::Params& args);
    bool staticfile(x0::HttpRequest *r, x0::FlowVM::Params& args);
    bool precompressed(x0::HttpRequest *r, x0::FlowVM::Params& args);

    // main functions
    void autoindex(x0::HttpRequest* r, x0::FlowVM::Params& args);
    bool matchIndex(x0::HttpRequest *r, const x0::BufferRef& arg);
    void rewrite(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void pathinfo(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void error_handler(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void header_add(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void header_append(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void header_overwrite(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void header_remove(x0::HttpRequest* r, x0::FlowVM::Params& args);

    // main properties
    void req_method(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_url(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_path(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_query(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_header(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_cookie(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_host(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_pathinfo(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_is_secure(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_scheme(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void req_status_code(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void conn_remote_ip(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void conn_remote_port(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void conn_local_ip(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void conn_local_port(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_path(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_exists(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_is_reg(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_is_dir(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_is_exe(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_mtime(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_size(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_etag(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void phys_mimetype(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void regex_group(x0::HttpRequest* in, x0::FlowVM::Params& args);

    void req_accept_language(x0::HttpRequest* r, x0::FlowVM::Params& args);
    bool verify_req_accept_language(x0::Instr* call);

    // {{{ legacy
private:
    unsigned long long getrlimit(int resource);
    unsigned long long setrlimit(int resource, unsigned long long max);
    // }}}
};

//@}

} // namespace x0

#endif
