#! /bin/bash
#
# version.sh: create a version string for use by configure.ac

version=
datefmt='%Y%m%d'

export GIT_DIR=$(dirname $0)/.git/

if head=$(git rev-parse --short=8 --verify HEAD 2>/dev/null); then

	suffix=''
	# Add a '-dirty' suffix for uncommitted changes.
	if git diff-index HEAD | read dummy; then
		suffix=-dirty
	fi

	tag=$(git describe --tags 2>/dev/null)
	version=$(printf "%s%s" ${tag} ${suffix})
else
	# Check if a specific version is set, eg: by buildroot
	if [ ! -z "$PETITBOOT_VERSION" ];
	then
		# Full git hash
		len=$(echo -n "${PETITBOOT_VERSION}" | wc -c)
		if [[ ${len} == 40 ]]; then
			version=`echo -n ${PETITBOOT_VERSION} | \
				sed "s/^\([0-9a-f]\{7\}\).*/\1/;"`
		else
			version="$PETITBOOT_VERSION"
		fi
	else
		# Default to current date and time.
		version="$(date +dev.$datefmt)"
	fi
fi

echo $version
