all:
	$(MAKE) -C example
	$(MAKE) -C sev

clean:
	$(MAKE) -C example clean
	$(MAKE) -C sev clean
