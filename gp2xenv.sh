## For official SDK
#export GP2XDEVDIR=/gp2xsdk/Tools
#export CC=$GP2XDEVDIR/bin/arm-gp2x-linux-gcc
#export LD=$GP2XDEVDIR/bin/arm-gp2x-linux-ld
#export OBJCOPY=$GP2XDEVDIR/bin/arm-gp2x-linux-objcopy
#export OBJDUMP=$GP2XDEVDIR/bin/arm-gp2x-linux-objdump
## For devkitGP2X
#export GP2XDEVDIR=/usr/local/devkitPro/devkitGP2X
#export CC=$GP2XDEVDIR/bin/arm-linux-gcc
#export LD=$GP2XDEVDIR/bin/arm-linux-ld
#export OBJCOPY=$GP2XDEVDIR/bin/arm-linux-objcopy
#export OBJDUMP=$GP2XDEVDIR/bin/arm-linux-objdump
## For gp2xsdk
export GP2XDEVDIR=/usr/local/gp2xdev
export CC=$GP2XDEVDIR/bin/gp2x-gcc
export LD=$GP2XDEVDIR/bin/gp2x-ld
export OBJCOPY=$GP2XDEVDIR/bin/gp2x-objcopy
export OBJDUMP=$GP2XDEVDIR/bin/gp2x-objdump
export CPPFLAGS="-I$GP2XDEVDIR/include"
export CFLAGS="-O3 -DNDEBUG -DGP2X -mcpu=arm940t -msoft-float"
export LDFLAGS="-L$GP2XDEVDIR/lib" 
