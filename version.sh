#! /bin/bash
#
# version.sh: create a version string for use by configure.ac

version=

if head=$(git rev-parse --short --verify HEAD 2>/dev/null); then

	# If available, use the git commit revision for the package version.

	# Add a date prefix for easy reading.
	# date='2010-11-30 16:36:09 -0800'

	date=$(git log --pretty=format:"%ci" -1 HEAD)
	date=${date##20}
	date=${date%%:[0-9][0-9] *}
	date=${date//-/.}
	date=${date// /.}
	date=${date//:/.}

	version=$(printf '%s-%s%s' ${date} g ${head})

	# Add a '-dirty' postfix for uncommitted changes.

	if git diff-index HEAD | read dummy; then
		version=`printf '%s%s' ${version} -dirty`
	fi
else
	# Default to current date and time.

	version="dev-$(date +%y.%m.%d-%H.%M.%S)"
fi

echo $version
