FROM ubuntu:16.04 as build
MAINTAINER Christian Parpart <christian@parpart.family>

ENV DOCROOT="/var/www" \
    PORT="80"

RUN apt update -qq
RUN apt-get install -y make autoconf automake libtool clang++-5.0 \
    libmysqlclient-dev zlib1g-dev libbz2-dev pkg-config libssl-dev \
    libpcre3-dev libfcgi-dev libgoogle-perftools-dev libpam-dev git
RUN apt-get install -y clang-5.0

COPY 3rdparty          /usr/src/x0/3rdparty
COPY docs              /usr/src/x0/docs
COPY src               /usr/src/x0/src
COPY Makefile.am configure.ac x0d.conf \
       XzeroBase.pc.in XzeroFlow.pc.in XzeroTest.pc.in mimetypes2cc.sh \
       /usr/src/x0/

WORKDIR /usr/src/x0
ARG CFLAGS=""
ARG CXXFLAGS=""
ARG LDFLAGS=""
RUN autoreconf --verbose --force --install && \
    CC="/usr/bin/clang-5.0" \
    CXX="/usr/bin/clang++-5.0" \
    CFLAGS="$CFLAGS" \
    CXXFLAGS="$CXXFLAGS" \
    LDFLAGS="$LDFLAGS" \
      ./configure --prefix="/usr" \
                  --sysconfdir="/etc/x0d" \
                  --with-pidfile="/var/run/x0d.pid" \
                  --with-logdir="/var/log" \
                  --disable-maintainer-mode

RUN make -j$(grep -c ^processor /proc/cpuinfo)
RUN make -j$(grep -c ^processor /proc/cpuinfo) xzero_test
RUN ./xzero_test --exclude='raft_*'
RUN strip x0d && ldd x0d && cp -v x0d /usr/bin/x0d

# -----------------------------------------------------------------------------
FROM ubuntu:16.04
RUN apt update
RUN apt install -y libpcre3 libssl1.0.0 zlib1g
RUN mkdir -p /etc/x0d /var/log/x0d /var/lib/x0d /var/www
COPY docker-x0d.conf /etc/x0d/x0d.conf
COPY --from=build /usr/bin/x0d /usr/bin/x0d

VOLUME /etc/x0d /var/www /var/log/x0d

ENTRYPOINT ["/usr/bin/x0d"]
CMD ["--log-target=console", "--log-level=info"]

# vim:syntax=dockerfile
