FROM ubuntu:15.10
MAINTAINER Christian Parpart <trapni@gmail.com>

ADD . /usr/src/x0

ENV DEBIAN_FRONTEND="noninteractive" \
    DOCROOT="/var/www" \
    PORT="80"

RUN echo "force-unsafe-io" > /etc/dpkg/dpkg.cfg.d/02apt-speedup && \
    echo "Acquire::http {No-Cache=True;};" > /etc/apt/apt.conf.d/no-cache && \
    apt-get update && \
    apt-get install -y \
        make cmake clang++-3.5 libssl-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools-dev \
        libpam-dev libgtest-dev ninja-build && \
    apt-get install -y libssl1.0.0 zlib1g libbz2-1.0 libpcre3 \
        libpam0g && \
    cd /usr/src/gtest && cmake . && make && \
        cp -vpi libgtest*.a /usr/local/lib/ && \
    cd /usr/src/x0 && cmake -GNinja \
        -DCMAKE_BUILD_TYPE=release \
        -DCMAKE_C_COMPILER=/usr/bin/clang-3.5 \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++-3.5 \
        -DX0D_CLUSTERDIR=/var/lib/x0d \
        -DX0D_LOGDIR=/var/log/x0d && \
    ninja && \
    mkdir -p /etc/x0d /var/log/x0d /var/lib/x0d /var/www && \
    cp xzero/test-base /usr/bin/test-xzero-base && \
    cp xzero-flow/test-flow /usr/bin/test-xzero-flow && \
    cp src/x0d/x0d /usr/bin/x0d && \
    apt-get purge -y \
        make cmake clang++-3.5 libssl-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools-dev \
        libpam-dev libgtest-dev ninja-build && \
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
