#!/bin/zsh
local SSHHOST=${1:-pi@ventilation.realraum.at}
local EXEC=${0:A:h:t}
local DSTDIR=/home/pi/bin/
EXTRASTUFF=( ./public )
export GOARCH=arm
export GOOS=linux
export CGO_ENABLED=0
go build && \
#ssh $SSHHOST mount / -o remount,rw && trap "ssh $SSHHOST mount / -o remount,ro" EXIT && \
rsync -rlv --exclude .git/ --delete "$EXEC" "${EXTRASTUFF[@]}" --delay-updates $SSHHOST:$DSTDIR && \
ssh $SSHHOST sudo setcap cap_net_bind_service=ep ${DSTDIR}${EXEC} && \
{ echo -n "Restart $EXEC [yn]? "; read -q && ssh $SSHHOST systemctl --user restart ${EXEC}.service ; return 0}
