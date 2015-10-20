FROM ubuntu:14.04
MAINTAINER Christian Parpart <trapni@gmail.com>

WORKDIR /tmp

ENV DEBIAN_FRONTEND noninteractive
ENV CXX /usr/bin/g++-4.8
ENV HOME /root

ENV CXXFLAGS -march=native -mtune=native -O3 -pipe
ENV CFLAGS -march=native -mtune=native -O3 -pipe

RUN locale-gen en_US.UTF-8 && \
    update-locale en_US.UTF-8

# base system preps
RUN dpkg-divert --local --rename --add /sbin/initctl
RUN rm /sbin/initctl
RUN ln -s /bin/true /sbin/initctl
RUN echo "force-unsafe-io" > /etc/dpkg/dpkg.cfg.d/02apt-speedup
RUN echo "Acquire::http {No-Cache=True;};" > /etc/apt/apt.conf.d/no-cache
RUN echo "deb http://archive.ubuntu.com/ubuntu precise main universe" > /etc/apt/sources.list

# base system upgrade
RUN apt-get -qqy install python-software-properties
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt-get update
RUN apt-get -qqy upgrade

# get dependencies
RUN apt-get -qqy install \
        g++-4.8 git make cmake libssl-dev libmysqlclient-dev \
        zlib1g-dev libbz2-dev pkg-config libpcre3-dev libfcgi-dev \
        libmagickwand-dev librrd-dev libtbb-dev

# x0d: clone, configure, compile, install
RUN git clone --depth=0 git://github.com/xzero/x0.git /usr/src/x0
RUN cmake -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX=/usr /usr/src/x0
RUN make all
RUN make install

# cleanup
RUN rm -rf /tmp/*
RUN apt-get remove --purge \
        g++-4.8 git make cmake libssl-dev libmysqlclient-dev \
        zlib1g-dev libbz2-dev pkg-config libpcre3-dev libfcgi-dev \
        libmagickwand-dev librrd-dev libtbb-dev

RUN mkdir /etc/xzero

VOLUME ["/etc/xzero", "/var/www"]

CMD ["/usr/bin/x0d", "--no-fork", "--config", "/etc/xzero/x0d.conf"]
