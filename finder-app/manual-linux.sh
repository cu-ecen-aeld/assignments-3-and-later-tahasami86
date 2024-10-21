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
CC=/usr/bin/aarch64-linux-gnu-gcc 

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
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC=${CC} mrproper
    #Building defconfigs
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC=${CC} defconfig
    #Build VMLinux Target
    make -j12 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC=${CC} all
    #Build modules and device nodes
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC=${CC} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC=${CC} dtbs

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
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper  # Ensures a clean build state
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig  # Generates default config
    make distclean
    make defconfig
else
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper  # Ensures a clean build state
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig  # Generates default config
    make distclean
    make CC=${CC} defconfig 
fi

echo "Busybox defconfig and mrproper completed"

 echo "Error is before make"
# TODO: Make and install busybox
 make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CC=${CC}
 echo "Error is before install"
 make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs CC=${CC} install

#commentformultiline

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
<< 'multiline'

for file in $(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | sed -n "s#.*program interpreter: \(.*\)]#${SYSROOT_DIR}\1#p"); do
    # Copy each file to the destination directory
    relative_path="${file#$SYSROOT_DIR}"
    echo "Copying ${file} to ${OUTDIR}/rootfs/${relative_path}"
    cp -L "$file" "${OUTDIR}/rootfs/${relative_path}"
done

for file in $(${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | sed -n "s#.*Shared library: \[\(.*\)\]#\1#p"); do
    if [ -f "${SYSROOT_DIR}/lib64/${file}" ]; then
        file_path="${SYSROOT_DIR}/lib64/${file}"
    elif [ -f "${SYSROOT_DIR}/lib/${file}" ]; then
        file_path="${SYSROOT_DIR}/lib/${file}"
    else
        echo "Warning: Shared library ${file} not found in lib or lib64"
        continue
    fi

    relative_path="${file_path#$SYSROOT_DIR}"
    echo "Copying ${file_path} to ${OUTDIR}/rootfs/${relative_path}"
    cp -L "$file_path" "${OUTDIR}/rootfs/${relative_path}"
done
multiline



# TODO: Make device nodes
#sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null  c 1 3
#sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1
#sudo mknod -m 666 ${OUTDIR}/rootfs/dev/ttyAMA0 c 204 64
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
