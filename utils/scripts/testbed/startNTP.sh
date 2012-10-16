#!/bin/bash

#sudo /etc/init.d/ntp stop
#sudo ntpdate us.pool.ntp.org
#sudo /etc/init.d/ntp start
runAll.sh sudo /etc/init.d/ntp stop
runAll.sh sudo ntpdate sw1
runAll.sh sudo /etc/init.d/ntp start

echo "NTP Started on nodes"
