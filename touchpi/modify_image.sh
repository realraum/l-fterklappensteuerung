#!/bin/zsh
## (c) Bernhard Tittelbach 2017

local TARGETIMAGE=${1}
local BBHOSTNAME=${2:-ventilation.realraum.at}
local MOUNTPTH=$(mktemp -d)
local APTSCRIPT=./aptscript.sh
local CONFIGTXTAPPEND=./config.txt.append
local LOCALROOT=./rootfs
local MYSSHPUBKEY=~/.ssh/id_ed25519.pub

[[ -z $TARGETIMAGE ]] && {echo "Give .img filename"; exit 1}

echo "Modify and and configure $TARGETIMAGE ?"
read -q || exit 1

local LOOPDEV=/dev/mapper/loop0

extendimage() {
  truncate -s $((1024*1024*6907)) $TARGETIMAGE
  #dd if=/dev/zero bs=1M count=3600 >> $TARGETIMAGE
  echo -e "n\np\n3\n$((0x400000))\n$((0x600000))\nn\np\n$((0x601000))\n\np\nw\n" | fdisk $TARGETIMAGE
}

umountimage() {
  [ -e ${LOOPDEV}p4 ] && sudo umount ${LOOPDEV}p4
  [ -e ${LOOPDEV}p3 ] && sudo umount ${LOOPDEV}p3
  [ -e ${LOOPDEV}p1 ] && sudo umount ${LOOPDEV}p1
  sudo umount ${LOOPDEV}p2
  sleep 1
  sudo kpartx  -d -v $TARGETIMAGE
}

mountimagemvvar() {
    LOOPDEV=$(sudo kpartx  -l $TARGETIMAGE  | head -n1 | cut -d' ' -f 5 | sed 's:^/dev/:/dev/mapper/:')
    sudo kpartx  -a -v $TARGETIMAGE
    sleep 1.0
    sudo mount ${LOOPDEV}p2 $MOUNTPTH -o acl
    sudo mount ${LOOPDEV}p1 $MOUNTPTH/boot
    if [[ -e ${LOOPDEV}p3 ]] ; then
        if ! sudo mount ${LOOPDEV}p3 ${MOUNTPTH}/home -o acl; then
            sudo mkfs -L home -t ext4 ${LOOPDEV}p3 || exit 2
            mvhome
            sudo mount ${LOOPDEV}p3 ${MOUNTPTH}/home -o acl
        fi
    fi
    if [[ -e ${LOOPDEV}p4 ]] ; then
        if ! sudo mount ${LOOPDEV}p4 ${MOUNTPTH}/var; then
            sudo mkfs -L var -t ext4 ${LOOPDEV}p4 || exit 2
            mvvar
            sudo mount ${LOOPDEV}p4 ${MOUNTPTH}/var
        fi
    fi
}

mvhome() {
  local TDIR=$(mktemp -d)
  sudo mount ${LOOPDEV}p3 $TDIR
  sudo mv ${MOUNTPTH}/home/*(D) ${TDIR}/
  sudo umount ${LOOPDEV}p3
  sync
}

mvvar() {
  local TDIR=$(mktemp -d)
  sudo mount ${LOOPDEV}p4 $TDIR
  sudo mv ${MOUNTPTH}/var/*(D) ${TDIR}/
  sudo umount ${LOOPDEV}p4
  sync
}

runchroot() {
  sudo systemd-nspawn --bind /usr/bin/qemu-arm --bind /lib/x86_64-linux-gnu --bind /usr/lib/x86_64-linux-gnu/ --bind /lib64 -D "$MOUNTPTH" -- $*
}

modifyBootConfigForTouch() {
  local uenv="${1:-$MOUNTPTH/boot/config.txt}"
  cat $CONFIGTXTAPPEND | sudo tee -a "$uenv"
}

disabledhcpfor() {
  local interfaces="${1:-usb0}"
  local dhcpcdconf="$MOUNTPTH/etc/dhcpcd.conf"
  echo -e "\ndenyinterfaces $interfaces" | sudo tee -a "$dhcpcdconf"
}

gobuildandcp() {
( cd "$1"
  export GOARCH=arm
  export GOOS=linux
  export CGO_ENABLED=0
  go build
)
  sudo mkdir -p "${2}"
  sudo cp -v "${1}/${1:t}" "${2}"
}


extendimage
mountimagemvvar
trap umountimage EXIT

## install/remove packages
runchroot /bin/bash < $APTSCRIPT

## settings and stuff
sudo rsync -var ${LOCALROOT}/  ${MOUNTPTH}/

## newest zsh config
cp ~/.zshrc ~/.zshrc.local ${MOUNTPTH}/home/pi/
chown 1000:1000 ${MOUNTPTH}/home/pi/.zsh*
sudo cp ~/.zshrc ~/.zshrc.local ${MOUNTPTH}/root/

## Hostname
echo "$BBHOSTNAME" | sudo tee ${MOUNTPTH}/etc/hostname

## ssh keys
sudo mkdir -p ${MOUNTPTH}/root/.ssh ${MOUNTPTH}/home/pi/.ssh
cat $MYSSHPUBKEY | sudo tee -a ${MOUNTPTH}/root/.ssh/authorized_keys
cat $MYSSHPUBKEY | sudo tee -a ${MOUNTPTH}/home/pi/.ssh/authorized_keys

modifyBootConfigForTouch "$MOUNTPTH/boot/config.txt"
disabledhcpfor "eth0"
sudo cp -rf ${MOUNTPTH}/usr/share/X11/xorg.conf.d/10-evdev.conf ${MOUNTPTH}/usr/share/X11/xorg.conf.d/45-evdev.conf                                 

gobuildandcp ../ventilationinterface ${MOUNTPTH}/home/pi/bin/
rsync -vr ../ventilationinterface/public ${MOUNTPTH}/home/pi/bin/
sudo setcap cap_net_bind_service=ep ${MOUNTPTH}/home/pi/bin/ventilationinterface
## instead rsync the symlink
#runchroot /bin/systemctl enable ventilationinterface.service
runchroot /bin/loginctl enable-linger pi


runchroot /usr/bin/passwd pi
### ssh-keygen for hostkeys...
runchroot /usr/sbin/dpkg-reconfigure openssh-server
runchroot /usr/bin/chsh -s /bin/zsh root
runchroot /usr/bin/chsh -s /bin/zsh pi
runchroot /bin/zsh


