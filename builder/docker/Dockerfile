FROM ubuntu:18.04
ENV TERM linux
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update -y 
# ENV CROSS_COMPILE=arm-linux-gnueabi-
# ENV CROSS_COMPILE=aarch64-linux-gnu-
RUN apt-get update -y && apt-get -yqq --no-install-recommends install \
        git u-boot-tools device-tree-compiler mtools repo \
        parted libudev-dev \
        libusb-1.0-0-dev lib32gcc-7-dev \
        python-linaro-image-tools linaro-image-tools \
        gcc-arm-linux-gnueabihf gcc-aarch64-linux-gnu gcc-arm-linux-gnueabi \
        libstdc++-7-dev autoconf autotools-dev \
        libsigsegv2 m4 intltool libdrm-dev \
        curl sed make binutils build-essential \
        gcc g++ bash patch gzip bzip2 perl tar \
        cpio python unzip rsync file bc wget \
        libncurses5 cvs mercurial \
        rsync openssh-client subversion asciidoc \
        w3m dblatex python-matplotlib \
        libssl-dev pv e2fsprogs fakeroot devscripts \
        libi2c-dev libncurses5-dev texinfo liblz4-tool genext2fs \
        flex swig cmake python-dev bison kmod automake gettext-base \
        gettext liblocale-gettext-perl libtool time \
        g++-multilib gcc-multilib liblz4-tool \
        libmpc3 pkg-config tree vim screen gawk expect
        # libqt4-dev libglib2.0-dev libgtk2.0-dev libglade2-dev graphviz
WORKDIR /data
CMD ["/bin/bash"]