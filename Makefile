denyfs install uninstall clean all:
	$(MAKE) -C src $@

help:
	@echo "make help	This"
	@echo "make denyfs	Compile denyfs"
	@echo "make install	Install denyfs"
	@echo "make uninstall	Uninstall denyfs"
	@echo "make clean	Clean compilation"
