FROM ubuntu:15.04
MAINTAINER Christian Parpart <trapni@gmail.com>

# {{{ Base System
ENV HOME /root
ENV LANG en_US.utf8
ENV PATH /usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV DEBIAN_FRONTEND noninteractive

RUN dpkg-divert --local --rename --add /sbin/initctl && \
    rm -f /sbin/initctl && \
    ln -s /bin/true /sbin/initctl

RUN locale-gen en_US.UTF-8 && \
    update-locale en_US.UTF-8

RUN apt-mark hold initscripts

RUN echo "force-unsafe-io" > /etc/dpkg/dpkg.cfg.d/02apt-speedup
RUN echo "Acquire::http {No-Cache=True;};" > /etc/apt/apt.conf.d/no-cache
RUN apt-get -qq update && apt-get -qqy dist-upgrade
# }}}

ADD . /usr/src/x0

RUN apt-get install -y \
        make cmake clang++-3.5 libssl-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools-dev libtbb-dev \
        libpam-dev libgtest-dev ninja-build && \
    apt-get install -y libssl1.0.0 zlib1g libbz2-1.0 libpcre3 libtbb2 \
        libpam0g && \
    cd /usr/src/gtest && cmake . && make && \
        cp -vpi libgtest*.a /usr/local/lib/ && \
    cd /usr/src/x0 && cmake -GNinja \
        -DCMAKE_BUILD_TYPE=release \
        -DCMAKE_C_COMPILER=/usr/bin/clang-3.5 \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++-3.5 \
        -DX0D_CLUSTERDIR=/var/lib/x0d \
        -DX0D_LOGDIR=/var/log/x0d \
        -DX0D_TMPDIR=/tmp && \
    ninja && \
    mkdir -p /etc/x0d /var/log/x0d /var/lib/x0d /var/www && \
    cp src/x0d/x0d /usr/bin/x0d && \
    apt-get purge -y \
        make cmake clang++-3.5 libssl-dev zlib1g-dev libbz2-dev pkg-config \
        libpcre3-dev libfcgi-dev libgoogle-perftools-dev libtbb-dev \
        libpam-dev libgtest-dev ninja-build && \
    apt-get autoremove -y && \
    rm -rf /usr/src

ENV DOCROOT "/var/www"
ENV PORT 80

ADD docker-x0d.conf /etc/x0d/x0d.conf
VOLUME /etc/x0d /var/www /var/lib/x0d /var/log/x0d

ENTRYPOINT ["/usr/bin/x0d"]
CMD ["--log-target=console", "--log-level=info"]
