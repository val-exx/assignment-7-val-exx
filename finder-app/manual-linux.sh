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
#CROSS_COMPILE=aarch64-none-linux-gnu-
TOOLCHAIN_BIN=$(dirname $(find ~ -name "aarch64-none-linux-gnu-gcc" 2>/dev/null | head -n 1))
export CROSS_COMPILE=${TOOLCHAIN_BIN}/aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

#create outdir if it doesnt exist, fail if the directory could not be created
if ! mkdir -p ${OUTDIR}; then
	echo"ERROR: could not create directory ${OUTDIR}" >&2
	exit 1
fi
#added by me

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
     make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} defconfig
     make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE} all
    #added by me
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}
#added by me

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "Creating necessary base directories"
mkdir -p rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/bin usr/sbin
mkdir -p var/log
#added by me

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
#git clone git://busybox.net/busybox.git
git clone https://git.busybox.net/busybox
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make defconfig
    #make menuconfig #not mandatory, use only if we want to personalize the configuration
    #added by me
else
    cd busybox
fi

# TODO: Make and install busybox
echo "Making and installing busybox"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd ${OUTDIR}/rootfs
#added by me 

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "Adding library dependencies to rootfs"
SYSROOT="$(${CROSS_COMPILE}gcc -print-sysroot)"
cp -L "${SYSROOT}/lib/ld-linux-aarch64.so.1" "${OUTDIR}/rootfs/lib"
cp -L "${SYSROOT}/lib64/libm.so.6" "${OUTDIR}/rootfs/lib64"
cp -L "${SYSROOT}/lib64/libresolv.so.2" "${OUTDIR}/rootfs/lib64"
cp -L "${SYSROOT}/lib64/libc.so.6" "${OUTDIR}/rootfs/lib64"
#added by me

# TODO: Make device nodes
echo "Making device nodes"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1
#added by me

# TODO: Clean and build the writer utility
echo "Cleaning and bulding the writer utility"
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE} all
#added by me

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
#find ${FINDER_APP_DIR} -type f -name "finder.sh*" | xargs cp -t ${OUTDIR}/rootfs/home
#find ${FINDER_APP_DIR} -type f -name "finder-test.sh*" | xargs cp -t ${OUTDIR}/rootfs/home
echo "Copying the finder related scripts and executables to the /home dir"
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home
#added by me

#copy conf/username.txt into outdir/rootfs home directory
#copy conf/assignment.txt into outdir/rootfs home directory
#copy autorun-qemu.sh into outdir/rootfs
echo "Copying conf/assignment, conf/username and autorun qemu bash"
mkdir ${OUTDIR}/rootfs/home/conf
cp ${FINDER_APP_DIR}/conf/"username.txt" ${OUTDIR}/rootfs/home/conf
cp ${FINDER_APP_DIR}/conf/"assignment.txt" ${OUTDIR}/rootfs/home/conf
cp ${FINDER_APP_DIR}/"autorun-qemu.sh" ${OUTDIR}/rootfs/home/
#added by me

# TODO: Chown the root directory
echo "Chowning the root directory"
cd ${OUTDIR}/rootfs
sudo chown -R root:root *
#added by me

# TODO: Create initramfs.cpio.gz
echo "Creating initramfs.cpio.gz"
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
#added by me


