FROM alpine:3

# Musl does not contain fts so musl-fts-dev must be installed.
# Clang uses libgcc compiler runtime library by default, but it
# can also use compiler-rt.
RUN apk add --no-cache musl-dev musl-fts-dev clang15 lld-dev compiler-rt make

# Symlink lld otherwise clang won't find it.
RUN ln -s /usr/bin/ld.lld /usr/bin/ld

WORKDIR /usr/src/gonc-app
COPY gonc.c .
COPY test ./test
COPY runtest.sh .

# We have to specify the runtime library.
# Also -lfts as it is not included in musl.
RUN clang -rtlib=compiler-rt -o gonc gonc.c -lfts

CMD ["./runtest.sh"]