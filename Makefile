# Installation prefix and build directory
BUILD_DIR ?= build
PREFIX ?= /opt/picofuse
CMAKE_BUILD_TYPE ?= Release

# Tools
CMAKE ?= $(shell which cmake 2>/dev/null)
DOCKER ?= $(shell which docker 2>/dev/null)
GIT ?= $(shell which git 2>/dev/null)


###############################################################################
# CONFIGURE AND BUILD

.PHONY: configure
configure: dep-cmake
	@${CMAKE} -B ${BUILD_DIR} \
		-D CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
		$(if ${PICO_BOARD},-D PICO_BOARD=${PICO_BOARD})

.PHONY: build
build: configure
	@${CMAKE} --build ${BUILD_DIR} --target all -j 4

.PHONY: test
test: build
	@${CMAKE} --build ${BUILD_DIR} --target test

###############################################################################
# DOCUMENTATION

.PHONY: doc
doc: dep-docker 
	@echo
	@echo make doc
	@${DOCKER} run -v .:/data greenbone/doxygen doxygen /data/doxygen/Doxyfile

###############################################################################
# DEPENDENCIES

.PHONY: dep-cmake
dep-cmake:
	@test -f "${CMAKE}" && test -x "${CMAKE}" || (echo "Missing CMAKE: ${CMAKE}" && exit 1)

.PHONY: dep-docker
dep-docker:
	@test -f "${DOCKER}" && test -x "${DOCKER}" || (echo "Missing DOCKER: ${DOCKER}" && exit 1)

.PHONY: dep-git
dep-git:
	@test -f "${GIT}" && test -x "${GIT}" || (echo "Missing GIT: ${GIT}" && exit 1)
