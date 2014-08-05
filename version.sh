#! /bin/bash
#
# version.sh: create a version string for use by configure.ac

version=
datefmt='%Y%m%d'

export GIT_DIR=$(basename $0)/.git/

if head=$(git rev-parse --short=8 --verify HEAD 2>/dev/null); then

	suffix=''
	# Add a '-dirty' suffix for uncommitted changes.
	if git diff-index HEAD | read dummy; then
		suffix=-dirty
	fi

	if tag=$(git describe --tags --exact-match 2>/dev/null); then
		# use a tag; remove any 'v' prefix from v<VERSION> tags
		tag=${tag#v}
		version=$(printf "%s%s" ${tag} ${suffix})
	else
		# Use the git commit revision for the package version, and add
		# a date prefix for easy comparisons.
		date=$(git log --pretty=format:"%ct" -1 HEAD)
		version=$(printf "%($datefmt)T.g%s%s" ${date} ${head} ${suffix})
	fi
else
	# Default to current date and time.
	version="$(date +dev.$datefmt)"
fi

echo $version
