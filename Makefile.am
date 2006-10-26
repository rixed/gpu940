SUBDIRS = include lib console perftime bin sample GL

cscope:
	cscope -Rb -I$(topsrc)/include -Ilib -Ibin

dumpcodegen:
	@for i in /tmp/codegen_*; do \
		/usr/local/devkitPro/devkitGP2X/bin/arm-linux-objdump -D -b binary -m arm $$i ; \
	done
