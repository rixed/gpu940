SUBDIRS = include lib console perftime bin sample GL

cscope:
	cscope -Rb $(CPPFLAGS) -I$(top_srcdir)/include -Ilib -Ibin -IGL

dumpcodegen:
	@for i in /tmp/codegen_*; do \
		$$GP2XDEVDIR/bin/*-objdump -D -b binary -m arm $$i ; \
	done | less
