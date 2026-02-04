#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$(realpath "$1")
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION} "${OUTDIR}/linux-stable"
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd  ${OUTDIR}/linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    
    echo "Clean build"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    echo "Cleaning defconfig"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    echo "Building kernel"
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    echo "Building modules"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    echo "Building device tree blob"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    cd -
fi

echo "Adding the Image in outdir"
echo "Copy Image to ${OUTDIR}"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR} 

echo "Creating the staging directory for the root filesystem"

if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir -pv ${OUTDIR}/rootfs/{bin,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,usr/{bin,lib,sbin},var/log}

if [ ! -d "${OUTDIR}/busybox" ]
then
    cd ${OUTDIR}
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
 else
    cd  ${OUTDIR}/busybox
fi

echo "Installing busybox into rootfs"
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd ${OUTDIR}
${CROSS_COMPILE}readelf -a rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a rootfs/bin/busybox | grep "Shared library"

echo "Copying shared libraries"
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp ${SYSROOT}/lib/ld-linux-aarch64.so.* ${OUTDIR}/rootfs/lib/ 
cp ${SYSROOT}/lib64/libm.so.* ${OUTDIR}/rootfs/lib64/
cp ${SYSROOT}/lib64/libresolv.so.* ${OUTDIR}/rootfs/lib64/
cp ${SYSROOT}/lib64/libc.so.* ${OUTDIR}/rootfs/lib64/

echo "Creating device nodes"
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

echo "Building writer utility now"
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}


echo "Copying artifacts from finder to roots home directory"
cd ..
echo "Current directory: $PWD"
cp -v ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home
cp -v ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
cp -v ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
mkdir -pv ${OUTDIR}/rootfs/home/conf
cp -v conf/username.txt ${OUTDIR}/rootfs/home/conf
cp -v conf/assignment.txt ${OUTDIR}/rootfs/home/conf
sed -i 's|../conf/assignment.txt|conf/assignment.txt|g' ${OUTDIR}/rootfs/home/finder-test.sh
sed -i '1s|^#!/bin/bash|#!/bin/sh|' ${OUTDIR}/rootfs/home/*.sh
cp -v ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home

sudo chown -R root:root ${OUTDIR}/rootfs

cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip -f initramfs.cpio
