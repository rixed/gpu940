# We still need to know where the cross compilator is for make dumpcodegen
. gp2xenv.sh
# Then override for x86 target
export CPPFLAGS=""
export CFLAGS="-O2 -g2 -DSOFT_DIVS=1"
export LDFLAGS=""
export CC=colorgcc
unset LD
