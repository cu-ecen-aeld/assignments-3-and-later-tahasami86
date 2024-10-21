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
TOOLCHAIN_PATH=/home/taha/Desktop/arm_GNU_toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin
export PATH=${TOOLCHAIN_PATH}:$PATH
CC=${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-gcc
LD=${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-ld
#<< 'commentformultiline'
if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here

    #dist clean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}  CC=${CC} mrproper   LD=${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-ld
    #Building defconfigs
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}  CC=${CC} defconfig  LD=${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-ld
    #Build VMLinux Target
    make -j12 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}  CC=${CC} all   LD=${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-ld
    #Build modules and device nodes
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}  CC=${CC} modules    LD=${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-ld  
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}  CC=${CC} dtbs       LD=${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-ld  

fi

echo "Adding the Image in outdir"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "$OUTDIR/"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
#mkdir -p ${OUTDIR}/rootfs
mkdir -p ${OUTDIR}/rootfs/{bin,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,usr,var}
mkdir -p ${OUTDIR}/rootfs/usr/{bin,lib,sbin}
mkdir -p ${OUTDIR}/rootfs/var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make CC=${CC} defconfig 
fi

echo "Busybox defconfig and mrproper completed"

# TODO: Make and install busybox
 make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC=${CC}
 make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs CC=${CC} install

echo "Library dependencies"
echo "the error is after this line"
${CROSS_COMPILE}readelf -a  ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a  ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"
#
# TODO: Add library dependencies to rootfs
cp /home/taha/Desktop/arm_GNU_toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1  ${OUTDIR}/rootfs/lib

cp /home/taha/Desktop/arm_GNU_toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64

cp /home/taha/Desktop/arm_GNU_toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libm.so.6  ${OUTDIR}/rootfs/lib64

cp /home/taha/Desktop/arm_GNU_toolchain/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64


# Add library dependencies to rootfs
SYSROOT_DIR=$(${CROSS_COMPILE}gcc -print-sysroot)
SYSROOT_DIR=$(realpath $SYSROOT_DIR)


cd ${OUTDIR}/rootfs
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1


# TODO: Clean and build the writer utility
cd /home/taha/Desktop/LInux_specialization/course_1/course_1/assignment-1-tahasami86/finder-app
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

file writer

#cp writer.o ${OUTDIR}/rootfs/home/
cp writer ${OUTDIR}/rootfs/home/
cd ..
cp -r finder-app/ ${OUTDIR}/rootfs/home/
cd finder-app/

sleep 10

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp finder.sh ${OUTDIR}/rootfs/home/
cp finder-test.sh ${OUTDIR}/rootfs/home/
cp autorun-qemu.sh ${OUTDIR}/rootfs/home/
cd ../conf
cp assignment.txt ${OUTDIR}/rootfs/home/
cp username.txt ${OUTDIR}/rootfs/home/
cd ..
cp -r conf/  ${OUTDIR}/rootfs/home/


echo "copy portion completed"

# TODO: Chown the root directory

cd ${OUTDIR}/rootfs
sudo chown -R root:root "${OUTDIR}/rootfs"

echo "chown the root directory completed"



# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
