#!/bin/bash
function check_is_installed(){
dpkg -l | grep "^ii" | grep -w "$1" 
}
source /opt/bashimport/transhell.sh
load_transhell_debug
HERE=$(dirname $(realpath $0))

if check_is_installed gxde-testing-source ;then
	if garma --question --text="${TRANSHELL_CONTENT_QUIT_TEST_DIALOG}";then
		gxde-app-uninstaller package-name gxde-testing-source 
	else
		exit
	fi
else
    garma --question --text="${TRANSHELL_CONTENT_JOIN_TEST_DIALOG}"
    if [[ "$?" = "0" ]];then
        gxde-app-installer gxde-testing-source 
        if ! check_is_installed gxde-testing-source ;then
            garma --error --text="${TRANSHELL_CONTENT_ERROR_IN_PROCESS}"
            exit -1
        fi
        garma --info --text="${TRANSHELL_CONTENT_JOIN_USER_GROUP}"
        xdg-open "https://www.gxde.top/install/#%E6%B2%9F%E9%80%9A%E6%B8%A0%E9%81%93" &
        garma --info --width=300 --text="Discord：https://discord.gg/t5Uf2xYpvA" &
    else
        exit
    fi
fi
