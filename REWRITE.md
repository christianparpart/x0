
### Migration Tasks: This Weekend

- [x] migrate DateTime to UnixTime/CivilTime
- [x] backport PosixScheduler changes from stx
- [x] make use of MonotonicTimer/Clock
- [x] fix logging
- [ ] compose SafeCall over inheritance
- [ ] pull InputStream/OutputStream API from stx (away from istream/ostream)?
- [ ] (make thread safe) File::lastModified()
- [ ] how to convert from UnixTime to CivilTime to get the current

### Migration Tasks

- [ ] FileRef to be renamed to FileHandle/FileHandle or alike?
- [ ] revive threading

### Thread Safety

- [ ] ...

###  Feature Stories

- [ ] port director plugin, yeah
- [ ] port stx::HTTPConnectionPool into HttpClient / HttpClientProxy
- [ ] x0d-signals: graceful shutdown (INT, TERM)
- [ ] x0d-signals: logfile rotating (HUP)
- [ ] x0d-signals: all the others (USR1, USR2, ...?)
