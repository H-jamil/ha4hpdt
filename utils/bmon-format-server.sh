sudo bmon -p team400if,enp202s0f0,enp202s0f1,enp0s20f0u7u2c2,ens2f0,ens2f1 -o format:fmt='$(element:name) $(attr:rxrate:bytes) $(attr:txrate:bytes) $(attr:rxrate:packets) $(attr:txrate:packets) \n' | while read line; do echo "`date +%s` $line"; done &>> /local/tbicer/perf-log/transfer_00.log

