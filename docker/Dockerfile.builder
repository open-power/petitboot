# Image for compiling petitboot.

ARG DOCKER_FROM

FROM ${DOCKER_FROM}

ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8

RUN apt-get update && apt-get install -y \
	apt-utils \
	autoconf \
	autopoint \
	bison \
	flex \
	gettext \
	gcc \
	git \
	libtool \
	libuv-dev \
	libdevmapper-dev \
	libncurses-dev \
	libgpgme11-dev \
	libssl-dev \
	pkg-config \
	&& rm -rf /var/lib/apt/lists/*

CMD /bin/bash
