
.PHONY: bin/Cemu_release
bin/Cemu_release:
	rm -rf $(@D)
	mkdir -p $(@D)
	id=$$(docker create cemu_build) ; \
		docker cp $$id:/cemu/bin . ; \
		docker rm -v $$id
	cp ./run $(@D)/run
	chmod +x $(@D)/run

.PHONY: image
image:
	docker build -t cemu_build -f ./Dockerfile ..