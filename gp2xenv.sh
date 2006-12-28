export GP2XDEVDIR=/usr/local/devkitPro/devkitGP2X
export CFLAGS="-O3 -DNDEBUG -DGP2X -I$GP2XDEVDIR/include -I/usr/local/include"
export LDFLAGS="-L$GP2XDEVDIR/lib" 
export CC=$GP2XDEVDIR/bin/arm-linux-gcc
export LD=$GP2XDEVDIR/bin/arm-linux-ld
