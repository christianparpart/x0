
### TODO

- [ ] proper HEAD support (don't wait for nothing)
- [ ] PUT/POST support
- [ ] HTTP over SSL (HTTPS) support
- [ ] implement `--ipv6` and `--ipv4`
- [ ] favor IPv6 if available and resolvable, unless somthing else was forced
- [ ] when a host resolves to multiple IP, try to connect to all and let
      the first completion win. discard any other connect() that is still
      in progress.
- [ ] `-v`, `--verbose` and `-q`, `--quiet`
