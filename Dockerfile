# Builder
FROM ubuntu:22.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build clang git sqlite3 libsqlite3-dev doxygen graphviz && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j && \
    ctest --test-dir build --output-on-failure
RUN cmake --install build --prefix /out

# Runtime (distroless C++)
FROM gcr.io/distroless/cc-debian12
USER 65532:65532  # nonroot
COPY --from=build /out /usr/local
ENTRYPOINT ["/usr/local/bin/neonstore"]
CMD ["--help"]
