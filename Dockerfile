FROM debian:bookworm AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential git ca-certificates curl pkg-config binutils unzip zip python3 \
      libsdl2-dev libsdl2-ttf-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

RUN git clone --depth 1 --branch 1.24.9 --recurse-submodules --shallow-submodules \
      https://github.com/ArtifexSoftware/mupdf.git
RUN cd mupdf && \
    export XCFLAGS="-DFZ_ENABLE_JS=0 -DFZ_ENABLE_XPS=0 -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_OFFICE=0" && \
    ( make -j"$(nproc)" build=release HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no HAVE_LIBCRYPTO=no USE_SYSTEM_LIBS=no libs > /tmp/mupdf.log 2>&1 \
      || { echo "=== MUPDF DERLEME HATASI: hata satirlari ==="; \
           grep -n -E "error|Error|\*\*\*|No rule|not found" /tmp/mupdf.log | head -n 30; \
           echo "=== SON 40 SATIR ==="; tail -n 40 /tmp/mupdf.log; exit 1; } ) && \
    ls -l build/release/libmupdf.a build/release/libmupdf-third.a

# FFmpeg: sadece MP4/MKV/AVI cozmek icin kucuk, statik derleme (H.264, MPEG-4, AAC, MP3, AC3, Vorbis, Opus)
RUN git clone --depth 1 --branch n6.1.1 https://github.com/FFmpeg/FFmpeg.git ffmpeg
RUN cd ffmpeg && ./configure --prefix=/build/ffout \
      --disable-everything --disable-programs --disable-doc --disable-debug --disable-autodetect \
      --disable-network --disable-avdevice --disable-avfilter --disable-zlib --disable-iconv \
      --disable-shared --enable-static --enable-pic --enable-pthreads \
      --enable-protocol=file \
      --enable-demuxer=mov,matroska,avi \
      --enable-parser=h264,mpeg4video,aac,aac_latm,mpegaudio,ac3,vorbis,opus \
      --enable-decoder=h264,mpeg4,aac,mp3,mp3float,ac3,eac3,vorbis,opus \
    && make -j"$(nproc)" && make install

RUN curl -fsSL -o dr_mp3.h https://raw.githubusercontent.com/mackron/dr_libs/master/dr_mp3.h

COPY src/mediabox.c .
RUN gcc -O2 -std=gnu11 -Wall -ffunction-sections -fdata-sections -o mediabox mediabox.c \
      -Imupdf/include $(pkg-config --cflags sdl2 SDL2_ttf) \
      -I/build/ffout/include \
      mupdf/build/release/libmupdf.a mupdf/build/release/libmupdf-third.a \
      /build/ffout/lib/libavformat.a /build/ffout/lib/libavcodec.a /build/ffout/lib/libswresample.a \
      /build/ffout/lib/libswscale.a /build/ffout/lib/libavutil.a \
      $(pkg-config --libs sdl2 SDL2_ttf) -lm -lpthread -Wl,--gc-sections \
    && strip mediabox \
    && echo "--- bagimliliklar ---" && readelf -d mediabox | grep NEEDED \
    && echo "--- en yuksek GLIBC ---" && (objdump -T mediabox | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1) \
    && ls -l mediabox

FROM scratch AS out
COPY --from=build /build/mediabox /mediabox
