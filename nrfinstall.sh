#!/bin/bash
args=("$@")

if [[ ${#args[@]} -gt 0 ]]
then
    cd ${args[0]}
else
    cd ~
fi
ROOT_PATH="$(pwd)/rf24libs"
REPOS=("RF24" "RF24Network" "RF24Mesh" "RF24Gateway")
DO_INSTALL=("0" "0" "0" "0")
CHOOSE_VERSION="N"
EXAMPLE_PATH=("examples_linux" "examples_RPi" "examples_RPi" "examples")
SUGGESTED_EXAMPLE=("gettingstarted" "helloworld_tx" "RF24Mesh_Example_Master" "RF24Gateway_ncurses")

# TODO Remove this when ready for master branches (or improve it via CLI args)
BRANCHES=("master" "master" "master" "master")

echo $'\n'"RF24 libraries installer by TMRh20 and 2bndy5"
echo "report issues at https://github.com/nRF24/RF24/issues"
echo $'\n'"******************** NOTICE **********************"
echo "This installer will create a 'rf24libs' folder for installation of selected libraries"
echo "To prevent mistaken deletion, users must manually delete existing library folders within 'rf24libs' if upgrading"
echo "Run 'sudo rm -r $ROOT_PATH' to clear the entire directory"
if [[ ! -d $ROOT_PATH ]]
then
    echo $'\n'"Creating $ROOT_PATH folder."
    mkdir $ROOT_PATH
fi

if ! command -v git &> /dev/null
then
    echo "Installing git from apt-get"
    sudo apt-get install git
fi
if ! command -v cmake &> /dev/null
then
    echo "Installing cmake from apt-get"
    sudo apt-get install cmake
fi

#read -p "Choose versions to install (Default: Install latest code from master) [y/N]? " CHOOSE_VERSION
CHOOSE_VERSION="y"
for index in "${!REPOS[@]}"
do
    #read -p "Do you want to install the ${REPOS[index]} library, [y/N]? " answer
    answer="y"
    case ${answer^^} in
        Y ) DO_INSTALL[index]=1
            if [ "${CHOOSE_VERSION^^}" = "Y" ]
            then
                #read -p "Which version/branch of ${REPOS[index]} to install (default is ${BRANCHES[index]})? " version
                version=""
                if [[ ${#version} -gt 0 ]]; then
                    BRANCHES[index]=$version
                fi
            fi
            ;;
        * ) ;;
    esac
done

if [[ ${DO_INSTALL[0]} > 0 ]]
then
    echo "WARNING: It is advised to remove the previously installed RF24 library first."
    echo $'\t'"This is done to avoid Runtime conflicts."
    if [[ -f "/usr/local/lib/librf24.so" ]]
    then
        echo "Uninstalling previously installed RF24 lib (/usr/local/lib/librf24.so)"
        sudo rm /usr/local/lib/librf24.*
        # check for presence of a very old install
        if [[ -f "/usr/local/lib/librf24-bcm.so" ]]
        then
            sudo rm /usr/local/lib/librf24-bcm.so
        fi
        sudo rm -r /usr/local/include/RF24
    fi
fi

if [[ ${DO_INSTALL[3]} > 0  && ! -f "/usr/lib/$(ls /usr/lib/gcc | tail -1)/libcurses.so" ]]
then
    #read -p "    Install ncurses library, recommended for RF24Gateway [y/N]? " answer
    answer="y"
    case ${answer^^} in
        Y ) sudo apt-get install libncurses5-dev;;
        * ) SUGGESTED_EXAMPLE[3]=RF24GatewayNode;;
    esac
    echo ""
fi

echo "*** Which hardware driver library do you wish to use? ***"
echo "1. SPIDEV (most compatible, Default)"
echo "2. WiringPi"
echo "3. MRAA (Intel Devices)"
echo "4. PiGPIO"
echo "5. BCM2835 Driver (for RPi only)"
echo "6. LittleWire"
#read answer
answer="1"
case ${answer^^} in
    1) RF24DRIVER+="SPIDEV";;
    2) RF24DRIVER+="wiringPi";;
    3) RF24DRIVER+="MRAA";;
    4) RF24DRIVER+="pigpio";;
    5) RF24DRIVER+="RPi";;
    6) RF24DRIVER+="LittleWire";;
    *) RF24DRIVER+="SPIDEV";;
esac

# set an env var for easier reuse (specific to RF24 repos).
# Any applicable CMakeLists.txt is configured to use this when it is set.
export RF24_DRIVER=$RF24DRIVER

# ensure we have a fresh build directory
create_build_env() {
    if [[ -f "Makefile.inc" ]]; then
        rm Makefile.inc
    fi
    if [[ -d "./build" ]]
    then
        echo "Purging build environment."$'\n'
        sudo rm -r build/
    fi
    mkdir build
    cd build
}

# array index is a required arg
install_repo() {
    echo $'\n'"Installing ${REPOS[$1]} Repo..."
    if [[ ! -d "$ROOT_PATH/${REPOS[$1]}" ]]
    then
        git clone https://github.com/nRF24/${REPOS[$1]} $ROOT_PATH/${REPOS[$1]}
    else
        echo "Using already cloned repo $ROOT_PATH/${REPOS[$1]}"
    fi
    echo ""
    cd $ROOT_PATH/${REPOS[$1]}
    git fetch --all
    git checkout ${BRANCHES[$1]}
    if [[ -f "CMakeLists.txt" ]]; then
        create_build_env
        cmake ..
    elif [[ -f "configure" ]]; then
        ./configure --driver=$RF24_DRIVER
    fi
    if ! make
    then
        echo "Building lib ${REPOS[$1]} failed. Quiting now."
        exit 1
    fi
    if ! sudo make install
    then
        echo "Installing lib ${REPOS[$1]} failed. Quiting now."
        exit 1
    fi
    CWD=$(pwd)
    if [[ "$CWD" != "*/build" ]]; then
        sudo ldconfig
    fi
    #read -p $'\n'"Do you want to build the ${REPOS[$1]} examples [Y/n]? " answer
    answer="y"
    case ${answer^^} in
        N ) ;;
        * )
            cd $ROOT_PATH/${REPOS[$1]}/${EXAMPLE_PATH[$1]}
            if [[ -f "CMakeLists.txt" ]]; then
                create_build_env
                cmake ..
            fi
            if ! make
            then
                echo "Building examples for lib ${REPOS[$1]} failed. Quiting now."
                exit 1
            fi
            echo ""
            echo "Complete! To run the example:"
            CWD=$(pwd)
            echo "cd $CWD"
            echo "sudo ./${SUGGESTED_EXAMPLE[$1]}"
    esac
}

for index in "${!REPOS[@]}"
do
    if [[ ${DO_INSTALL[index]} > 0 ]]
    then
        install_repo $index
    fi
done

INSTALL_PYRF24="N"
echo $'\n'
#read -p "Would you like to install the unified python wrapper package (pyrf24) [y/N]?" INSTALL_PYRF24
INSTALL_PYRF24="y"
case ${INSTALL_PYRF24^^} in
    Y )
        if [[ ! -d "$ROOT_PATH/pyRF24" ]]
        then
            git clone https://github.com/nRF24/pyRF24 $ROOT_PATH/pyRF24
        else
            echo "Using already cloned repo $ROOT_PATH/pyRF24"
        fi
        cd $ROOT_PATH/pyRF24
        echo $'\nInitializing frozen submodules\n'
        git submodule update --init
        echo $'\nInstalling build prequisites.\n'
        python3 -m pip install -r requirements.txt
        echo $'\nInstalling pyrf24 package (from source).\n'
        # building from src respects the selected $RF24_DRIVER ('pip install pyrf24' strictly uses SPIDEV)
        python3 -m pip install .
        ;;
esac
echo $'\n\n'"*** Installer Complete ***"
echo "See http://tmrh20.github.io for documentation"
echo "See http://tmrh20.blogspot.com for info "
echo $'\n'"Listing repositories in $ROOT_PATH"
ls ${ROOT_PATH}

# clean up env var
unset RF24_DRIVER
