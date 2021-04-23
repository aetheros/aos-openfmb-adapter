
sdk_not_found=

if [ -n "$AOS_SDK_ROOT" ]; then
	if [ ! -d $AOS_SDK_ROOT ]; then
		echo "AOS_SDK_ROOT directory ($AOS_SDK_ROOT) does not exist"
	else
		. ${AOS_SDK_ROOT}/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi
	fi
elif [ -f /opt/tools/aos-esr1k-x86_64/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi ]; then
	. /opt/tools/aos-esr1k-x86_64/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi
elif  [ -f /usr/local/aos-esr1k-x86_64/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi ]; then
	. /usr/local/aos-esr1k-x86_64/environment-setup-armv7a-vfp-neon-oe-linux-gnueabi
else
	echo "Can't find the SDK directory" >&2
	echo "Please install the AOS SDK from ..." >&2
	echo " or export AOS_SDK_ROOT pointing to the directory where it is installed" >&2
	sdk_not_found=1
fi

