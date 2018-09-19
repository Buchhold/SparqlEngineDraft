FROM ubuntu:18.04 as base
MAINTAINER Niklas Schnelle <schnelle@informatik.uni-freiburg.de>
ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8
ENV LC_CTYPE C.UTF-8

FROM base as builder
RUN apt-get update && apt-get install -y build-essential cmake libsparsehash-dev libbz2-dev
COPY . /app/

# Check formatting with the .clang-format project style
WORKDIR /app/
RUN misc/format-check.sh

WORKDIR /app/build/
RUN cmake -DCMAKE_BUILD_TYPE=Release .. && make -j $(nproc)

FROM base as runtime
WORKDIR /app
RUN apt-get update && apt-get install -y wget python3-yaml unzip curl libgomp1
ARG UID=1000
RUN groupadd -r qlever && useradd --no-log-init -r -u $UID -g qlever qlever && chown qlever:qlever /app
RUN apt-get update && apt-get install -y bzip2

COPY --from=builder /app/build/*Main /app/src/web/* /app/
COPY --from=builder /app/e2e/* /app/e2e/
ENV PATH=/app/:$PATH

USER qlever
EXPOSE 7001
VOLUME ["/input", "/index"]

ENV INDEX_PREFIX wikidata-full
# Need the shell to get the INDEX_PREFIX envirionment variable
ENTRYPOINT ["/bin/sh", "-c", "exec ServerMain -i \"/index/${INDEX_PREFIX}\" -p 7001 \"$@\"", "--"]
CMD ["-a", "-j 8"]

# docker build -t qlever-<name> .
# # When running with user namespaces you may need to make the index folder accessible
# # to e.g. the "nobody" user
# chmod -R o+rw ./index
# # For an existing index copy it into the ./index folder and make sure to either name it
# # index.* or
# # set the envirionment variable "INDEX_PREFIX" during `docker run` using `-e INDEX_PREFIX=<prefix>`
# # To build an index run a bash inside the container as follows
# docker run -it --rm --entrypoint bash -v "<path_to_input>:/input" -v "$(pwd)/index:/index" qlever-<name>
# # Then inside that shell IndexBuilder is in the path and can be used like
# # described in the README.md with the files in /input
# # To run a server use
# docker run -d -p 7001:7001 -e "INDEX_PREFIX=<prefix>" -v "$(pwd)/index:/index" --name qlever-<name> qlever-<name>

