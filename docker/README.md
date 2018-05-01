# Petitboot Build Container Support

## Command Summary

 - `build-builder` Builds a docker image that contains tools for building petitboot.  Default docker image tag is `pb-builder:${VERSION}${ARCH_TAG}`.
 - `build-pb` Builds the petitboot programs using a pb-builder container.

## Examples

### Build the petitboot programs

    ./build-builder -v
    ./build-pb -vc

### Run petitboot programs in a pb-builder container

    docker run --rm -v $(pwd):/opt/pb -w /opt/pb $(./docker/build-pb -t) ./ui/ncurses/petitboot-nc --help

## Debugging Build Problems

### Run an interactive pb-builder container

As current user:

    docker run --rm -it --user $(id -u):$(id -g) -v /etc/group:/etc/group:ro -v /etc/passwd:/etc/passwd:ro -v $(pwd):/opt/pb -w /opt/pb $(./docker/build-pb -t) bash

As root:

    docker run --rm -it -v /etc/group:/etc/group:ro -v /etc/passwd:/etc/passwd:ro -v $(pwd):/opt/pb -w /opt/pb $(./docker/build-pb -t) bash
