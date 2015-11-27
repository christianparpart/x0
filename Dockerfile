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
RUN /usr/src/x0/bin/build.sh

VOLUME ["/etc/x0d"]
VOLUME ["/var/lib/x0d"]
VOLUME ["/var/log/x0d"]

ENTRYPOINT ["/usr/bin/x0d"]
CMD ["--log-target=console", "--log-level=info"]
