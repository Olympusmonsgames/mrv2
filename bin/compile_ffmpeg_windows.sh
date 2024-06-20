#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# mrv2
# Copyright Contributors to the mrv2 Project. All rights reserved.

#
# This script compiles a GPL or LGPL version of ffmpeg. The GPL version has
# libx264 encoding support.  The LGPL version does not have libx264.
#
if [[ ! -e etc/build_dir.sh ]]; then
    echo "You must run this script from the root of mrv2 directory like:"
    echo
    script=`basename $0`
    echo "> bin/$script"
    exit 1
fi

if [[ ! $RUNME ]]; then
    . etc/build_dir.sh
else
    . etc/functions.sh
fi

if [[ $KERNEL != *Msys* ]]; then
    echo
    echo "This script is for Windows MSys2-64 only."
    echo
    exit 1
fi

#
# Latest TAGS of all libraries
#
LIBVPX_TAG=v1.12.0
DAV1D_TAG=1.3.0
SVTAV1_TAG=v1.8.0
X264_TAG=stable
FFMPEG_TAG=6.1.1

#
# Repositories
#
SVTAV1_REPO=https://gitlab.com/AOMediaCodec/SVT-AV1.git
LIBVPX_REPO=https://chromium.googlesource.com/webm/libvpx.git
LIBDAV1D_REPO=https://code.videolan.org/videolan/dav1d.git 
LIBX264_REPO=https://code.videolan.org/videolan/x264.git

#
# FFmpeg has two repositories.  The first one was failing.
#
FFMPEG_REPO=https://git.ffmpeg.org/ffmpeg.git
FFMPEG_REPO2=https://github.com/FFmpeg/FFmpeg

#
# Some auxiliary variables
#
MRV2_ROOT=$PWD
ROOT_DIR=$PWD/$BUILD_DIR/tlRender/etc/SuperBuild/FFmpeg
INSTALL_DIR=$PWD/$BUILD_DIR/install

if [[ $FFMPEG_GPL == GPL || $FFMPEG_GPL == LGPL ]]; then
    true
else
    echo
    echo "No --gpl or --lgpl flag provided to compile FFmpeg.  Choosing --lgpl."
    echo
    export FFMPEG_GPL=LGPL
fi

if [ -z "$TLRENDER_FFMPEG" ]; then
    export TLRENDER_FFMPEG=ON
fi

BUILD_FFMPEG=$TLRENDER_FFMPEG

lgpl_ffmpeg=""
if [[ -e $INSTALL_DIR/lib/avformat.lib ]]; then
    avformat_dll=`ls $INSTALL_DIR/bin/avformat*.dll`
    lgpl_ffmpeg=`strings $avformat_dll | findstr "LGPL" | grep -o "LGPL"`
fi

if [[ "$lgpl_ffmpeg" != "" ]]; then
    if [[ $lgpl_ffmpeg != $FFMPEG_GPL ]]; then
	echo "Incompatible FFmpeg already installed.  Installed is $lgpl_ffmpeg, want $FMMPEG_GPL."
	run_cmd rm -rf $INSTALL_DIR/lib/avformat.lib
    else
	echo "Compatible FFmpeg already installed."
	export BUILD_FFMPEG=OFF
    fi
fi

    
if [[ $BUILD_FFMPEG == ON || $BUILD_FFMPEG == 1 ]]; then
    pacman -Sy make wget diffutils nasm pkg-config --noconfirm
fi
    

has_pip3=0
if type -P pip3 &> /dev/null; then
    has_pip3=1
fi

has_meson=0
if type -P meson &> /dev/null; then
    has_meson=1
fi

has_cmake=0
if type -P cmake &> /dev/null; then
    has_cmake=1
fi

if [ -z "$TLRENDER_AV1" ]; then
    export TLRENDER_AV1=ON
fi

if [ -z "$TLRENDER_NET" ]; then
    export TLRENDER_NET=ON
fi

if [ -z "$TLRENDER_VPX" ]; then
    export TLRENDER_VPX=ON
fi

#
# Build with h264 encoding.
#
TLRENDER_X264=ON
if [[ $FFMPEG_GPL == LGPL ]]; then
    TLRENDER_X264=OFF
fi

if [[ $BUILD_FFMPEG == OFF || $BUILD_FFMPEG == 0 ]]; then
    export TLRENDER_VPX=OFF
    export TLRENDER_AV1=OFF
    export TLRENDER_NET=OFF
    export TLRENDER_X264=OFF
else
    echo
    echo "Installing packages needed to build:"
    echo
    if [[ $TLRENDER_VPX == ON || $TLRENDER_VPX == 1 ]]; then
	echo "libvpx"
    fi
    if [[ $TLRENDER_AV1 == ON || $TLRENDER_AV1 == 1 ]]; then
	if [[ $has_meson || $has_pip3 ]]; then
	    echo "libdav1d"
	fi
	if [[ $has_cmake == 1 ]]; then
	    echo "libSvtAV1"
	fi
    fi
    if [[ $TLRENDER_X264 == ON || $TLRENDER_X264 == 1 ]]; then
	echo "libx264"
    fi
    echo "FFmpeg"
    echo
fi

download_yasm() {
    cd $ROOT_DIR/sources

    if [ ! -e yasm.exe ]; then
	# We need to download a win64 specific yasm, not msys64 one
	wget -c http://www.tortall.net/projects/yasm/releases/yasm-1.3.0-win64.exe
	mv yasm-1.3.0-win64.exe yasm.exe
    fi
}

export OLD_PATH="$PATH"

#
# Add current path to avoid long PATH on Windows.
#
export PATH=".:$OLD_PATH"

#
# Build with libdav1d decoder and SVT-AV1 encoder.
#
if [[ $TLRENDER_AV1 == ON || $TLRENDER_AV1 == 1 ]]; then
    BUILD_LIBDAV1D=1
    if [[ $has_meson == 0 ]]; then
	if [[ $has_pip3 == 1 ]]; then
	    pip3 install meson
	    has_meson=1
	    BUILD_LIBDAV1D=1
	else
	    echo "Please install meson from https://github.com/mesonbuild/meson/releases"
	    BUILD_LIBDAV1D=0
	fi
    fi

    BUILD_LIBSVTAV1=1
    if [[ $has_cmake == 0 ]]; then
	BUILD_LIBSVTAV1=0
    fi
fi

mkdir -p $ROOT_DIR

cd    $ROOT_DIR

mkdir -p sources
mkdir -p build


#
# Download Special YASM for Windows (libvpx and svt-av1 need it)
#
if [[ $TLRENDER_VPX == ON || $TLRENDER_VPX == 1 || \
	  $BUILD_LIBSVTAV1 == 1 ]]; then
    download_yasm
fi



#############
## BUILDING #
#############


## Build libvpx
ENABLE_LIBVPX=""
if [[ $TLRENDER_VPX == ON || $TLRENDER_VPX == 1 ]]; then
    cd $ROOT_DIR/sources
    if [[ ! -d libvpx ]]; then
	git clone --depth 1 --branch ${LIBVPX_TAG} ${LIBVPX_REPO}
    fi
    
    if [[ ! -e $INSTALL_DIR/lib/vpx.lib ]]; then

	cd $ROOT_DIR/sources/libvpx
    
	echo
	echo "Compiling libvpx......"
	echo
	
	cp $ROOT_DIR/sources/yasm.exe .
	
	./configure --prefix=$INSTALL_DIR \
		    --target=x86_64-win64-vs17 \
		    --enable-vp9-highbitdepth \
		    --disable-unit-tests \
		    --disable-examples \
		    --disable-docs
	devenv vpx.sln -Upgrade
	make -j ${CPU_CORES}
	make install
	run_cmd mv $INSTALL_DIR/lib/x64/vpxmd.lib $INSTALL_DIR/lib/vpx.lib
	run_cmd rm -rf $INSTALL_DIR/lib/x64/
    fi
    
    ENABLE_LIBVPX='--enable-libvpx 
            --enable-decoder=libvpx_vp8
            --enable-decoder=libvpx_vp9
            --enable-encoder=libvpx_vp8
            --enable-encoder=libvpx_vp9
            --extra-libs=vpx.lib 
            --extra-libs=kernel32.lib 
            --extra-libs=user32.lib
            --extra-libs=gdi32.lib 
            --extra-libs=winspool.lib
            --extra-libs=shell32.lib
            --extra-libs=ole32.lib
            --extra-libs=oleaut32.lib
            --extra-libs=uuid.lib
            --extra-libs=comdlg32.lib
            --extra-libs=advapi32.lib
            --extra-libs=msvcrt.lib'
fi


#
# Build libdav1d decoder
#
ENABLE_LIBDAV1D=""
if [[ $BUILD_LIBDAV1D == 1 ]]; then
    
    cd $ROOT_DIR/sources

    if [[ ! -d dav1d ]]; then
	git clone --depth 1 ${LIBDAV1D_REPO} --branch ${DAV1D_TAG}
    fi

    if [[ ! -e $INSTALL_DIR/lib/dav1d.lib ]]; then
	echo
	echo "Compiling libdav1d......"
	echo
	cd dav1d
	export CC=cl.exe
	meson setup -Denable_tools=false -Denable_tests=false --default-library=static -Dlibdir=$INSTALL_DIR/lib --prefix=$INSTALL_DIR build
	cd build
	ninja -j ${CPU_CORES}
	ninja install
	run_cmd mv $INSTALL_DIR/lib/libdav1d.a $INSTALL_DIR/lib/dav1d.lib 
    fi
    
    ENABLE_LIBDAV1D="--enable-libdav1d --enable-decoder=libdav1d"
fi

#
# Build libSvt-AV1 encoder
#
ENABLE_LIBSVTAV1=""
if [[ $BUILD_LIBSVTAV1 == 1 ]]; then
    
    cd $ROOT_DIR/sources

    if [[ ! -d SVT-AV1 ]]; then
	echo "Cloning ${SVTAV1_REPO}"
	git clone --depth 1 ${SVTAV1_REPO} --branch ${SVTAV1_TAG}
    fi

    if [[ ! -e $INSTALL_DIR/lib/SvtAV1Enc.lib ]]; then
	echo "Building SvtAV1Enc.lib"
	cd SVT-AV1
	
	cp -f $ROOT_DIR/sources/yasm.exe .
	
	cd Build/windows
	cmd //c build.bat 2019 release static no-dec no-apps

	cd -
	
	cp Bin/Release/SvtAv1Enc.lib $INSTALL_DIR/lib

	mkdir -p $INSTALL_DIR/include/svt-av1
	cp Source/API/*.h $INSTALL_DIR/include/svt-av1
	
	cd $ROOT_DIR/sources
    fi

    if [[ ! -e $INSTALL_DIR/lib/pkgconfig/SvtAv1Enc.pc ]]; then

	mkdir -p $INSTALL_DIR/lib/pkgconfig
	cd $INSTALL_DIR/lib/pkgconfig
	
	cat <<EOF > SvtAv1Enc.pc
prefix=$INSTALL_DIR
includedir=\${prefix}/include
libdir=\${prefix}/lib

Name: SvtAv1Enc
Description: SVT (Scalable Video Technology) for AV1 encoder library
Version: 1.8.0
Libs: -L\${libdir} -lSvtAv1Enc
Cflags: -I\${includedir}/svt-av1
Cflags.private: -UEB_DLL
EOF

	sed -i -e 's#([A-Z])\:#/\\1/#' SvtAv1Enc.pc

	cd $ROOT_DIR/sources
    fi
    
    ENABLE_LIBSVTAV1="--enable-libsvtav1 --enable-encoder=libsvtav1"
fi


#
# Build x264
#
ENABLE_LIBX264=""
if [[ $TLRENDER_X264 == ON || $TLRENDER_X264 == 1 ]]; then
    
    cd $ROOT_DIR/sources

    if [[ ! -d x264 ]]; then
	git clone --depth 1 ${LIBX264_REPO} --branch ${X264_TAG}
    fi

    if [[ ! -e $INSTALL_DIR/lib/libx264.lib ]]; then
	echo
	echo "Compiling libx264 as GPL......"
	echo
	cd $ROOT_DIR/build
	mkdir -p x264
	cd x264
	CC=cl ./../../sources/x264/configure --prefix=$INSTALL_DIR --enable-shared
	make -j ${CPU_CORES}
	make install
	run_cmd mv $INSTALL_DIR/lib/libx264.dll.lib $INSTALL_DIR/lib/libx264.lib
    fi
    
    ENABLE_LIBX264="--enable-libx264 
                    --enable-decoder=libx264
                    --enable-encoder=libx264
                    --enable-gpl"
else
    # Remove unused libx264
    if [[ -e $INSTALL_DIR/lib/libx264.lib ]]; then
	run_cmd rm -f $INSTALL_DIR/bin/libx264*.dll
	run_cmd rm -f $INSTALL_DIR/lib/libx264.lib
    fi
fi


#
# Install openssl and libcrypto
#
ENABLE_OPENSSL=""
if [[ $TLRENDER_NET == ON || $TLRENDER_NET == 1 ]]; then
    if [[ ! -e $BUILD_DIR/install/lib/ssl.lib ]]; then
	
	if [[ ! -e /mingw64/lib/libssl.dll.a ]]; then
	    pacman -Sy mingw-w64-x86_64-openssl --noconfirm
	fi

	run_cmd cp /mingw64/bin/libssl*.dll $INSTALL_DIR/bin/
	run_cmd cp /mingw64/lib/libssl.dll.a $INSTALL_DIR/lib/ssl.lib
	run_cmd cp /mingw64/bin/libcrypto*.dll $INSTALL_DIR/bin/
	run_cmd cp /mingw64/lib/libcrypto.dll.a $INSTALL_DIR/lib/crypto.lib
	run_cmd mkdir -p $INSTALL_DIR/lib/pkgconfig
	run_cmd cp /mingw64/lib/pkgconfig/libssl.pc $INSTALL_DIR/lib/pkgconfig/openssl.pc

	run_cmd cp -r /mingw64/include/openssl $INSTALL_DIR/include/
	run_cmd sed -i -e 's/SSL_library_init../SSL_library_init/' $INSTALL_DIR/include/openssl/ssl.h
	run_cmd sed -i -e "s#=/mingw64#=$INSTALL_DIR#" $INSTALL_DIR/lib/pkgconfig/openssl.pc
	run_cmd sed -i -e 's%Requires.private:.libcrypto%%' $INSTALL_DIR/lib/pkgconfig/openssl.pc
	
	run_cmd cp /mingw64/lib/pkgconfig/libcrypto.pc $INSTALL_DIR/lib/pkgconfig/
	run_cmd sed -i -e "s#=/mingw64#=$INSTALL_DIR#" $INSTALL_DIR/lib/pkgconfig/libcrypto.pc
    fi
    ENABLE_OPENSSL="--enable-openssl --extra-libs=crypto.lib --enable-version3"
fi

#
# Build FFmpeg
#

if [[ $BUILD_FFMPEG == ON || $BUILD_FFMPEG == 1 ]]; then

    echo "Preparing to build FFmpeg...."
    
    cd $ROOT_DIR/sources

    if [[ ! -d ffmpeg ]]; then
	echo "Cloning ffmpeg repository..."
	set +e
	git clone --depth 1 --branch n${FFMPEG_TAG} ${FFMPEG_REPO} 2> /dev/null
	clone_status=$?
	if [ $clone_status -eq 0 ]; then
	    echo "git clone successful"
	else
	    echo "git clone failed with exit code: $clone_status.  Trying mirror."
	    git clone --depth 1 --branch n${FFMPEG_TAG} ${FFMPEG_REPO2} 2> /dev/null
	    # Handle the failure (e.g., exit script, display specific message)
	    clone_status=$?
	    if [ $clone_status -eq 0 ]; then
		echo "git clone successful"
	    else
		echo "Mirror also failed with exit code: $clone_status.  Trying with curl..."
		curl https://ffmpeg.org/releases/ffmpeg-${FFMPEG_TAG}.tar.bz2 \
		     --output ffmpeg-${FFMPEG_TAG}.tar.bz2
		clone_status=$?
		if [ $clone_status -eq 0 ]; then
		    echo "curl was successful.  Untarring to ffmpeg."
		    mkdir -p ffmpeg
		    tar -xf ffmpeg-${FFMPEG_TAG}.tar.bz2 -C ffmpeg --strip-components 1
		    if [[ ! -d ffmpeg ]]; then
			echo "tar failed"
			exit 1
		    fi
		else
		    echo "curl failed."
		    exit $clone_status
		fi
	    fi
	fi
	set -o pipefail -e
    fi
    
    if [[ ! -e $INSTALL_DIR/lib/avformat.lib ]]; then
	echo
	echo "Compiling FFmpeg as ${FFMPEG_GPL}......"
	echo
	run_cmd rm -rf $ROOT_DIR/build/ffmpeg
	cd $ROOT_DIR/build
	mkdir -p ffmpeg
	cd ffmpeg
	
	if [[ -e $INSTALL_DIR/include/zconf.h ]]; then
	    mv $INSTALL_DIR/include/zconf.h $INSTALL_DIR/include/zconf.h.orig
	fi

	export CC=cl
	export PKG_CONFIG_PATH=$INSTALL_DIR/lib/pkgconfig

	ENABLE_MINIMAL=""
	if [[ $TLRENDER_FFMPEG_MINIMAL == ON || $TLRENDER_FFMPEG_MINIMAL == 1 ]]
	then
	    ENABLE_MINIMAL="
	    --disable-audiotoolbox
	    --disable-videotoolbox

            --disable-decoders
            --enable-decoder=aac
            --enable-decoder=ac3
            --enable-decoder=av1
            --enable-decoder=ayuv
            --enable-decoder=cfhd
            --enable-decoder=dnxhd
            --enable-decoder=eac3
            --enable-decoder=flac
            --enable-decoder=gif
            --enable-decoder=h264
            --enable-decoder=hevc
            --enable-decoder=mjpeg
            --enable-decoder=mp3
            --enable-decoder=mpeg2video
            --enable-decoder=mpeg4
            --enable-decoder=opus
            --enable-decoder=pcm_alaw
            --enable-decoder=pcm_bluray
            --enable-decoder=pcm_dvd
            --enable-decoder=pcm_f16le
            --enable-decoder=pcm_f24le
            --enable-decoder=pcm_f32be
            --enable-decoder=pcm_f32le
            --enable-decoder=pcm_f64be
            --enable-decoder=pcm_f64le
            --enable-decoder=pcm_lxf
            --enable-decoder=pcm_mulaw
            --enable-decoder=pcm_s16be
            --enable-decoder=pcm_s16be_planar
            --enable-decoder=pcm_s16le
            --enable-decoder=pcm_s16le_planar
            --enable-decoder=pcm_s24be
            --enable-decoder=pcm_s24daud
            --enable-decoder=pcm_s24le
            --enable-decoder=pcm_s24le_planar
            --enable-decoder=pcm_s32be
            --enable-decoder=pcm_s32le
            --enable-decoder=pcm_s32le_planar
            --enable-decoder=pcm_s64be
            --enable-decoder=pcm_s64le
            --enable-decoder=pcm_s8
            --enable-decoder=pcm_s8_planar
            --enable-decoder=pcm_sga
            --enable-decoder=pcm_u16be
            --enable-decoder=pcm_u16le
            --enable-decoder=pcm_u24be
            --enable-decoder=pcm_u24le
            --enable-decoder=pcm_u32be
            --enable-decoder=pcm_u32le
            --enable-decoder=pcm_u8
            --enable-decoder=pcm_vidc
            --enable-decoder=prores
            --enable-decoder=rawvideo
            --enable-decoder=truehd
            --enable-decoder=v210
            --enable-decoder=v210x
            --enable-decoder=v308
            --enable-decoder=v408
            --enable-decoder=v410
            --enable-decoder=vorbis
            --enable-decoder=vp9
            --enable-decoder=yuv4
            --enable-decoder=wmalossless
            --enable-decoder=wmapro
            --enable-decoder=wmav1
            --enable-decoder=wmav2
            --enable-decoder=wmavoice
            --enable-decoder=wmv1
            --enable-decoder=wmv2
            --enable-decoder=wmv3
            --enable-decoder=wmv3image

            --disable-encoders
            --enable-encoder=aac
            --enable-encoder=ac3
            --enable-encoder=ayuv
            --enable-encoder=cfhd
            --enable-encoder=dnxhd
            --enable-encoder=eac3
            --enable-encoder=gif
            --enable-encoder=mjpeg
            --enable-encoder=mpeg2video
            --enable-encoder=mpeg4
            --enable-encoder=opus
            --enable-encoder=pcm_alaw
            --enable-encoder=pcm_bluray
            --enable-encoder=pcm_dvd
            --enable-encoder=pcm_f32be
            --enable-encoder=pcm_f32le
            --enable-encoder=pcm_f64be
            --enable-encoder=pcm_f64le
            --enable-encoder=pcm_mulaw
            --enable-encoder=pcm_s16be
            --enable-encoder=pcm_s16be_planar
            --enable-encoder=pcm_s16le
            --enable-encoder=pcm_s16le_planar
            --enable-encoder=pcm_s24be
            --enable-encoder=pcm_s24daud
            --enable-encoder=pcm_s24le
            --enable-encoder=pcm_s24le_planar
            --enable-encoder=pcm_s32be
            --enable-encoder=pcm_s32le
            --enable-encoder=pcm_s32le_planar
            --enable-encoder=pcm_s64be
            --enable-encoder=pcm_s64le
            --enable-encoder=pcm_s8
            --enable-encoder=pcm_s8_planar
            --enable-encoder=pcm_u16be
            --enable-encoder=pcm_u16le
            --enable-encoder=pcm_u24be
            --enable-encoder=pcm_u24le
            --enable-encoder=pcm_u32be
            --enable-encoder=pcm_u32le
            --enable-encoder=pcm_u8
            --enable-encoder=pcm_vidc
            --enable-encoder=prores
            --enable-encoder=prores_ks
            --enable-encoder=rawvideo
            --enable-encoder=truehd
            --enable-encoder=v210
            --enable-encoder=v308
            --enable-encoder=v408
            --enable-encoder=v410
            --enable-encoder=yuv4
            --enable-encoder=vorbis
            --enable-encoder=wmav1
            --enable-encoder=wmav2
            --enable-encoder=wmv1
            --enable-encoder=wmv2

            --disable-demuxers
            --enable-demuxer=aac
            --enable-demuxer=ac3
            --enable-demuxer=aiff
            --enable-demuxer=asf
            --enable-demuxer=av1
            --enable-demuxer=dnxhd
            --enable-demuxer=dts
            --enable-demuxer=dtshd
            --enable-demuxer=eac3
            --enable-demuxer=flac
            --enable-demuxer=gif
            --enable-demuxer=h264
            --enable-demuxer=hevc
            --enable-demuxer=m4v
            --enable-demuxer=matroska
            --enable-demuxer=mjpeg
            --enable-demuxer=mov
            --enable-demuxer=mp3
            --enable-demuxer=mxf
            --enable-demuxer=ogg
            --enable-demuxer=pcm_alaw
            --enable-demuxer=pcm_f32be
            --enable-demuxer=pcm_f32le
            --enable-demuxer=pcm_f64be
            --enable-demuxer=pcm_f64le
            --enable-demuxer=pcm_mulaw
            --enable-demuxer=pcm_s16be
            --enable-demuxer=pcm_s16le
            --enable-demuxer=pcm_s24be
            --enable-demuxer=pcm_s24le
            --enable-demuxer=pcm_s32be
            --enable-demuxer=pcm_s32le
            --enable-demuxer=pcm_s8
            --enable-demuxer=pcm_u16be
            --enable-demuxer=pcm_u16le
            --enable-demuxer=pcm_u24be
            --enable-demuxer=pcm_u24le
            --enable-demuxer=pcm_u32be
            --enable-demuxer=pcm_u32le
            --enable-demuxer=pcm_u8
            --enable-demuxer=pcm_vidc
            --enable-demuxer=rawvideo
            --enable-demuxer=truehd
            --enable-demuxer=v210
            --enable-demuxer=v210x
            --enable-demuxer=wav
            --enable-demuxer=yuv4mpegpipe

            --disable-muxers
            --enable-muxer=ac3
            --enable-muxer=aiff
            --enable-muxer=asf
            --enable-muxer=dnxhd
            --enable-muxer=dts
            --enable-muxer=eac3
            --enable-muxer=flac
            --enable-muxer=gif
            --enable-muxer=h264
            --enable-muxer=hevc
            --enable-muxer=m4v
            --enable-muxer=matroska
            --enable-muxer=mjpeg
            --enable-muxer=mov
            --enable-muxer=mp4
            --enable-muxer=mpeg2video
            --enable-muxer=mxf
            --enable-muxer=ogg
            --enable-muxer=opus
            --enable-muxer=pcm_alaw
            --enable-muxer=pcm_f32be
            --enable-muxer=pcm_f32le
            --enable-muxer=pcm_f64be
            --enable-muxer=pcm_f64le
            --enable-muxer=pcm_mulaw
            --enable-muxer=pcm_s16be
            --enable-muxer=pcm_s16le
            --enable-muxer=pcm_s24be
            --enable-muxer=pcm_s24le
            --enable-muxer=pcm_s32be
            --enable-muxer=pcm_s32le
            --enable-muxer=pcm_s8
            --enable-muxer=pcm_u16be
            --enable-muxer=pcm_u16le
            --enable-muxer=pcm_u24be
            --enable-muxer=pcm_u24le
            --enable-muxer=pcm_u32be
            --enable-muxer=pcm_u32le
            --enable-muxer=pcm_u8
            --enable-muxer=pcm_vidc
            --enable-muxer=rawvideo
            --enable-muxer=truehd
            --enable-muxer=wav
            --enable-muxer=yuv4mpegpipe

            --disable-parsers
            --enable-parser=aac
            --enable-parser=ac3
            --enable-parser=av1
            --enable-parser=dnxhd
            --enable-parser=dolby_e
            --enable-parser=flac
            --enable-parser=gif
            --enable-parser=h264
            --enable-parser=hevc
            --enable-parser=mjpeg
            --enable-parser=mpeg4video
            --enable-parser=mpegaudio
            --enable-parser=mpegvideo
            --enable-parser=opus
            --enable-parser=vorbis
            --enable-parser=vp9

            --disable-protocols
            --enable-protocol=crypto
            --enable-protocol=file
            --enable-protocol=ftp
            --enable-protocol=http
            --enable-protocol=httpproxy
            --enable-protocol=https
            --enable-protocol=md5"
	fi
	      
	# -wd4828 disables non UTF-8 characters found on non-English MSVC
	# -wd4101 disables local variable without reference
	# -wd4267 disables conversion from size_t to int, possible loss of data
	# -wd4334 '<<': result of 32-bit shift implicitly converted to 64 bits
	# -wd4090 'function': different 'const' qualifiers
	./../../sources/ffmpeg/configure \
            --prefix=$INSTALL_DIR \
	    --pkg-config-flags=--static \
            --disable-programs \
	    --disable-avfilter \
            --disable-doc \
            --disable-postproc \
            --disable-hwaccels \
            --disable-devices \
            --disable-filters \
            --disable-alsa \
            --disable-appkit \
            --disable-avfoundation \
            --disable-bzlib \
            --disable-coreimage \
            --disable-iconv \
            --disable-libxcb \
            --disable-libxcb-shm \
            --disable-libxcb-xfixes \
            --disable-libxcb-shape \
            --disable-lzma \
            --disable-metal \
            --disable-sndio \
            --disable-schannel \
            --disable-sdl2 \
            --disable-securetransport \
            --disable-vulkan \
            --disable-xlib \
            --disable-zlib \
            --disable-amf \
            --disable-audiotoolbox \
            --disable-cuda-llvm \
            --disable-cuvid \
            --disable-d3d11va \
            --disable-dxva2 \
            --disable-ffnvcodec \
            --disable-nvdec \
            --disable-nvenc \
            --disable-v4l2-m2m \
            --disable-vaapi \
            --disable-vdpau \
	    --disable-large-tests \
            --disable-videotoolbox \
            --enable-pic \
            --toolchain=msvc \
            --target-os=win64 \
            --arch=x86_64 \
            --enable-x86asm \
            --enable-asm \
            --enable-shared \
            --disable-static \
            --enable-swresample \
	    $ENABLE_MINIMAL \
            $ENABLE_OPENSSL \
            $ENABLE_LIBX264 \
            $ENABLE_LIBDAV1D \
            $ENABLE_LIBSVTAV1 \
            $ENABLE_LIBVPX \
            --extra-ldflags="-LIBPATH:$INSTALL_DIR/lib/" \
            --extra-cflags="-I$INSTALL_DIR/include/ -MD -wd4828 -wd4101 -wd4267 -wd4334 -wd4090"

        make -j ${CPU_CORES}
        make install

        # @bug: FFmpeg places .lib in bin/
        run_cmd mv $INSTALL_DIR/bin/*.lib $INSTALL_DIR/lib/

        if [[ -e $INSTALL_DIR/include/zconf.h.orig ]]; then
            run_cmd mv $INSTALL_DIR/include/zconf.h.orig $INSTALL_DIR/include/zconf.h
        fi
    fi
fi


if [[ $BUILD_FFMPEG == ON || $BUILD_FFMPEG == 1 ]]; then
    echo
    echo "Removing packages used to build:"
    echo
    if [[ $TLRENDER_VPX == ON || $TLRENDER_VPX == 1 ]]; then
	echo "libvpx"
    fi
    if [[ $TLRENDER_AV1 == ON || $TLRENDER_AV1 == 1 ]]; then
	if [[ $has_meson == 1 || $has_pip3 ]]; then
	    echo "libdav1d"
	fi
	if [[ $has_cmake == 1 ]]; then
	    echo "libSvtAV1"
	fi
    fi
    if [[ $TLRENDER_X264 == ON || $TLRENDER_X264 == 1 ]]; then
	echo "libx264"
    fi
    echo "FFmpeg"
    echo
    pacman -R nasm --noconfirm
fi

#
# Restore PATH
#
export PATH="$OLD_PATH"


cd $MRV2_ROOT
