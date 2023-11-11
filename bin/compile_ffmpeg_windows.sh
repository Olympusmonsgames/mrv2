#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# mrv2
# Copyright Contributors to the mrv2 Project. All rights reserved.

#
# This script compiles a GPL or BSD version of ffmpeg. The GPL version has
# libx264 encoding and libvpx support.  The LGPL version does not have libx264.
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

LIBVPX_TAG=v1.12.0
X264_TAG=master
FFMPEG_TAG=n6.0

MRV2_ROOT=$PWD
ROOT_DIR=$PWD/$BUILD_DIR/tlRender/etc/SuperBuild/FFmpeg
INSTALL_DIR=$PWD/$BUILD_DIR/install

if [[ $FFMPEG_GPL == GPL || $FFMPEG_GPL == LGPL ]]; then
    true
else
    echo
    echo "No --gpl or --lgpl flag provided to compile FFmpeg.  Choosing --lgpl."
    export FFMPEG_GPL=LGPL
fi

#
# This configures the environment for compilation.  It also cleans at the
# end to leave it ready for mrv2 build.
#
MSYS_LIBS=1

#
# Build with libvpx (webm) movies.
#
BUILD_LIBVPX=1

#
# Build wiht h264 encoding.
#
BUILD_LIBX264=1
if [[ $FFMPEG_GPL == LGPL ]]; then
    BUILD_LIBX264=0
fi

#
# Build with openssl and libcrypto support
# 
BUILD_SSL=1


#
# Build FFMPEG with the GPL libraries specified above.
#
BUILD_FFMPEG=1



if [[ $MSYS_LIBS == 1 ]]; then
    pacman -Sy --noconfirm

    #
    # This is for libx264 and ffmpeg
    #
    if [[ $FFMPEG_GPL == GPL ]]; then
	echo
	echo "Installing packages needed to build libvpx, libx264 and FFmpeg..."
    else
	echo
	echo "Installing packages needed to build libvpx and FFmpeg..."
    fi
    echo
    
    pacman -Sy make diffutils yasm nasm pkg-config --noconfirm

fi

mkdir -p $ROOT_DIR

cd    $ROOT_DIR

mkdir -p sources
mkdir -p build



#############
## BUILDING #
#############
#
# Build x264
#
ENABLE_LIBX264=""
if [[ $BUILD_LIBX264 == 1 ]]; then
    
    cd $ROOT_DIR/sources

    if [[ ! -d x264 ]]; then
	git clone --depth 1 https://code.videolan.org/videolan/x264.git --branch ${X264_TAG} 2> /dev/null
    fi

    if [[ ! -e $INSTALL_DIR/lib/libx264.lib ]]; then
	echo
	echo "Compiling libx264 as $FFMPEG_GPL......"
	echo
	cd $ROOT_DIR/build
	mkdir -p x264
	cd x264
	CC=cl ./../../sources/x264/configure --prefix=$INSTALL_DIR --enable-shared
	make -j ${CPU_CORES}
	make install
	run_cmd mv $INSTALL_DIR/lib/libx264.dll.lib $INSTALL_DIR/lib/libx264.lib
    fi
    
    ENABLE_LIBX264="--enable-libx264 --enable-gpl"
    if [[ $BUILD_SSL == 1 ]]; then
	ENABLE_LIBX264="${ENABLE_LIBX264} --enable-version3"
    fi
else
    # Remove unused libx264
    if [[ -e $INSTALL_DIR/lib/libx264.lib ]]; then
	run_cmd rm -f $INSTALL_DIR/bin/libx264*.dll
	run_cmd rm -f $INSTALL_DIR/lib/libx264.lib
    fi
fi

## Build libvpx
ENABLE_LIBVPX=""
if [[ $BUILD_LIBVPX == 1 ]]; then
    cd $ROOT_DIR/sources
    if [[ ! -d libvpx ]]; then
	git clone --depth 1 --branch ${LIBVPX_TAG} https://chromium.googlesource.com/webm/libvpx 2> /dev/null
    fi
    
    if [[ ! -e $INSTALL_DIR/lib/vpx.lib ]]; then
	cd $ROOT_DIR/build
	mkdir -p libvpx
	cd libvpx
    
	echo
	echo "Compiling libvpx......"
	echo

	
	# ./../../sources/libvpx/configure --prefix=$INSTALL_DIR \
	# 				 --target=x86_64-win64-vs16 \
	# 				 --disable-examples \
	# 				 --disable-docs \
	# 				 --disable-unit-tests \
	# 				 --disable-debug \
	# 				 --log=no \
	# 				 --disable-debug-libs \
	# 				 --disable-dependency-tracking
	
	./../../sources/libvpx/configure --prefix=$INSTALL_DIR \
					 --target=x86_64-win64-vs16 \
					 --enable-vp9-highbitdepth \
					 --disable-unit-tests \
					 --disable-examples \
					 --disable-docs
	make -j ${CPU_CORES}
	make install
	run_cmd mv $INSTALL_DIR/lib/x64/vpxmd.lib $INSTALL_DIR/lib/vpx.lib
	run_cmd rm -rf $INSTALL_DIR/lib/x64/
    fi
    
    ENABLE_LIBVPX='--enable-libvpx --extra-libs=vpx.lib --extra-libs=kernel32.lib --extra-libs=user32.lib --extra-libs=gdi32.lib --extra-libs=winspool.lib --extra-libs=shell32.lib --extra-libs=ole32.lib --extra-libs=oleaut32.lib --extra-libs=uuid.lib --extra-libs=comdlg32.lib --extra-libs=advapi32.lib --extra-libs=msvcrt.lib'
fi


#
# Install openssl and libcrypto
#
ENABLE_OPENSSL=""
if [[ $BUILD_SSL == 1 ]]; then
    if [[ ! -e $BUILD_DIR/install/lib/ssl.lib ]]; then
	
	if [[ ! -e /mingw64/lib/libssl.dll.a ]]; then
	    pacman -Sy mingw-w64-x86_64-openssl --noconfirm
	fi

	run_cmd cp /mingw64/bin/libssl*.dll $INSTALL_DIR/bin/
	run_cmd cp /mingw64/lib/libssl.dll.a $INSTALL_DIR/lib/ssl.lib
	run_cmd cp /mingw64/bin/libcrypto*.dll $INSTALL_DIR/bin/
	run_cmd cp /mingw64/lib/libcrypto.dll.a $INSTALL_DIR/lib/crypto.lib

	run_cmd cp -r /mingw64/include/openssl $INSTALL_DIR/include/
	run_cmd sed -i -e 's/SSL_library_init../SSL_library_init/' $INSTALL_DIR/include/openssl/ssl.h
    fi
    ENABLE_OPENSSL="--enable-openssl"
fi


#
# Build FFmpeg
#

if [[ $BUILD_FFMPEG == 1 ]]; then
    cd $ROOT_DIR/sources

    if [[ ! -d ffmpeg ]]; then
	git clone --depth 1 --branch ${FFMPEG_TAG} git://source.ffmpeg.org/ffmpeg.git 2> /dev/null
    fi

    lgpl_ffmpeg=""
    if [[ -e $INSTALL_DIR/lib/avformat.lib ]]; then
	avformat_dll=`ls $INSTALL_DIR/bin/avformat*.dll`
	lgpl_ffmpeg=`strings $avformat_dll | findstr "LGPL" | grep -o "LGPL"`
    fi

    if [[ $lgpl_ffmpeg != "" ]]; then
	if [[ $lgpl_ffmpeg != $FFMPEG_GPL ]]; then
	    echo "Incompatible FFmpeg already installed.  Installed is $lgpl_ffmpeg, want $FMMPEG_GPL."
	    run_cmd rm -rf $INSTALL_DIR/lib/avformat.lib
	else
	    echo "Compatible FFmpeg already installed."
	fi
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

	# -wd4828 disables non UTF-8 characters found on non-English MSVC
	# -wd4101 disables local variable without reference
	./../../sources/ffmpeg/configure \
            --prefix=$INSTALL_DIR \
            --disable-programs \
            --disable-doc \
            --disable-postproc \
            --disable-avfilter \
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
            $ENABLE_OPENSSL \
            $ENABLE_LIBX264 \
            $ENABLE_LIBVPX \
            --extra-ldflags="-LIBPATH:$INSTALL_DIR/lib/" \
            --extra-cflags="-I$INSTALL_DIR/include/ -MD -wd4828 -wd4101"

        make -j ${CPU_CORES}
        make install

        # @bug: FFmpeg places .lib in bin/
        run_cmd mv $INSTALL_DIR/bin/*.lib $INSTALL_DIR/lib/

        if [[ -e $INSTALL_DIR/include/zconf.h.orig ]]; then
            run_cmd mv $INSTALL_DIR/include/zconf.h.orig $INSTALL_DIR/include/zconf.h
        fi
    fi
fi


if [[ $MSYS_LIBS == 1 ]]; then
    echo "Removing packages used to build libx264 and FFmpeg..."
    pacman -R yasm nasm --noconfirm
fi

cd $MRV2_ROOT
