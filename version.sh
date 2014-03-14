#! /bin/bash
#
# version.sh: create a version string for use by configure.ac

version=
datefmt='%Y%m%d'

if head=$(git rev-parse --short=8 --verify HEAD 2>/dev/null); then

	# If available, use the git commit revision for the package version,
	# and add a date prefix for easy comparisons.

	date=$(git log --pretty=format:"%ct" -1 HEAD)

	suffix=''
	# Add a '-dirty' postfix for uncommitted changes.
	if git diff-index HEAD | read dummy; then
		suffix=-dirty
	fi

	version=$(printf "%($datefmt)T-g%s%s" ${date} ${head} ${suffix})
else
	# Default to current date and time.
	version="$(date +dev-$datefmt)"
fi

echo $version
