export RIOTBASE=$(shell git rev-parse --show-toplevel)
# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS      ?= -j auto
SPHINXBUILD     ?= sphinx-build
SOURCEDIR       = source
BUILDDIR        = build
SPHINX_SOURCE   = $(RIOTBASE)/doc/sphinx/source
SPHINX_EXAMPLES = $(SPHINX_SOURCE)/examples
SPHINX_BOARDS   = $(SPHINX_SOURCE)/boards

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

copy-boards:
	mkdir -p $(SPHINX_BOARDS)
	rm -f $(SPHINX_BOARDS)/*.rst
	find $(RIOTBASE)/boards/*/*.rst | awk -v SPHINX_BOARDS=$(SPHINX_BOARDS) -F/ \
		'{print $$0 " " SPHINX_BOARDS "/" $$(NF-1) ".rst"}' | xargs -r -n2 cp

copy-examples:
	mkdir -p $(SPHINX_EXAMPLES)
	rm -f $(SPHINX_EXAMPLES)/*.rst
	find $(RIOTBASE)/examples/*/*.rst | awk -v SPHINX_EXAMPLES=$(SPHINX_EXAMPLES) -F/ \
		'{print $$0 " " SPHINX_EXAMPLES "/" $$(NF-1) ".rst"}' | xargs -r -n2 cp


# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: copy-boards copy-examples Makefile 
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
