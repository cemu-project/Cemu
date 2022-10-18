ARCH=$(uname -m)


bin/Cemu-$(ARCH).appimage: rsc/Cemu.desktop rsc/info.cemu.Cemu.png image
	rm -rf $(@D)
	mkdir -p $(@D)
	id=$$(docker create cemu_build) ; \
		docker cp $$id:/cemu/Cemu-$(ARCH).AppImage $@ ; \
		docker rm -v $$id

rsc/Cemu.desktop: ../dist/linux/info.cemu.Cemu.desktop
	cp ../dist/linux/info.cemu.Cemu.desktop $@
	echo "" >> $@

rsc/info.cemu.Cemu.png: ../src/resource/logo_icon.png 
	cp ../src/resource/logo_icon.png $@

.PHONY: image
image:
	docker build -t cemu_build -f ./Dockerfile ..