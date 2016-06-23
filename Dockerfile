FROM ubuntu:14.04
MAINTAINER Christian Parpart <trapni@gmail.com>

ENV DEBIAN_FRONTEND="noninteractive" \
    DOCROOT="/var/www" \
    PORT="80"

RUN echo "force-unsafe-io" > /etc/dpkg/dpkg.cfg.d/02apt-speedup && \
    echo "Acquire::http {No-Cache=True;};" > /etc/apt/apt.conf.d/no-cache && \
    apt-get update && \
    apt-get install -y \
        make automake autoconf libtool \
        clang++-3.5 libssl-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools-dev libpam-dev && \
    apt-get install -y libssl1.0.0 zlib1g libbz2-1.0 libpcre3 \
        libpam0g

#ADD . /usr/src/x0
ADD 3rdparty          /usr/src/x0/3rdparty
ADD mimetypes2cc.sh   /usr/src/x0/mimetypes2cc.sh
ADD Makefile.am       /usr/src/x0/Makefile.am
ADD configure.ac      /usr/src/x0/configure.ac
ADD docker-x0d.conf   /usr/src/x0/docker-x0d.conf
ADD src               /usr/src/x0/src

RUN cd /usr/src/x0 && autoreconf --verbose --force --install
RUN cd /usr/src/x0 && \
    CC="/usr/bin/clang-3.5" \
    CXX="/usr/bin/clang++-3.5" \
      ./configure && \
    make && \
    make check && \
    mkdir -p /etc/x0d /var/log/x0d /var/lib/x0d /var/www && \
    ./xzero_test && \
    cp -v xzero_test /usr/bin/xzero_test && \
    cp -v x0d /usr/bin/x0d

RUN apt-get purge -y \
        make automake autoconf libtool \
        clang++-3.5 libssl-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools-dev libpam-dev && \
    apt-get purge -y perl && \
    echo 'Yes, do as I say!' | apt-get remove -y --force-yes \
        initscripts util-linux e2fsprogs systemd-sysv && \
    apt-get autoremove -y && \
    rm -rvf /var/lib/apt/lists/* && \
    rm -rf /usr/src

ADD docker-x0d.conf /etc/x0d/x0d.conf
VOLUME /etc/x0d /var/www /var/lib/x0d /var/log/x0d

ENTRYPOINT ["/usr/bin/x0d"]
CMD ["--log-target=console", "--log-level=info"]
