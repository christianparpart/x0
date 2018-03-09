FROM alpine:3.7 as build

ENV DOCROOT="/var/www" \
    PORT="80"

RUN apk update
RUN apk add musl-dev gcc g++ clang make \
      automake autoconf libtool pkgconfig \
      openssl-dev linux-pam-dev bzip2-dev fcgi-dev \
      \
      linux-pam openssl fcgi

COPY 3rdparty          /usr/src/x0/3rdparty
COPY docs              /usr/src/x0/docs
COPY src               /usr/src/x0/src
COPY Makefile.am configure.ac x0d.conf \
       XzeroBase.pc.in XzeroFlow.pc.in XzeroTest.pc.in mimetypes2cc.sh \
       /usr/src/x0/

WORKDIR /usr/src/x0
ARG CFLAGS="-O2"
ARG CXXFLAGS="-O2"
ARG LDFLAGS=""
RUN autoreconf --verbose --force --install && \
    CC="/usr/bin/clang" \
    CXX="/usr/bin/clang++" \
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
FROM alpine:3.7
RUN  apk add --update libgcc libstdc++ gmp openssl linux-pam
RUN  mkdir -p /etc/x0d /var/log/x0d /var/lib/x0d /var/www
COPY docker-x0d.conf /etc/x0d/x0d.conf
COPY --from=build /usr/bin/x0d /usr/bin/x0d

VOLUME /etc/x0d /var/www /var/log/x0d

ENTRYPOINT ["/usr/bin/x0d"]
CMD ["--log-target=console", "--log-level=info"]
