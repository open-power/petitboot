#!groovy
// Builds pb-builder image and runs build-pb script.
//
// The `jenkins` user must be in the `docker` user group.
// Requires nodes with labels: `amd64`, `arm64`, `docker`.
// Required plugins: build-timeout, copyartifact, git, pipeline, ssh-agent,
// workflow-aggregator.

properties([
    buildDiscarder(logRotator(daysToKeepStr: '30', numToKeepStr: '5')),
    parameters([
    string(name: 'BUILD_ARCH_LIST',
        defaultValue: 'amd64 arm64',
        description: 'List of Jenkins node architectures to build on.'),
    booleanParam(name: 'DOCKER_PURGE',
        defaultValue: false,
        description: 'Remove existing pb-builder docker image and rebuild.'),
    booleanParam(name: 'DRY_RUN',
        defaultValue: false,
        description: 'Dry run, do not build.'),
    string(name: 'GIT_URL',
        defaultValue: 'git://ozlabs.org/petitboot',
        description: 'URL of petitboot git repository.'),
    ])
])

def build_pb = { String _build_arch, Boolean _dry_run, String _git_url,
    Boolean _purge
    ->
    String build_arch = _build_arch
    Boolean dry_run = _dry_run
    String git_url = _git_url
    Boolean purge = _purge
    String builder_args = ""
    String pb_args = ""

    if (dry_run) {
        builder_args += " --dry-run"
        pb_args += " --dry-run"
    }
    if (purge) {
        builder_args += " --purge"
    }

    // timeout if no build_arch node is available.
    timeout(time: 15, unit: 'MINUTES') {
        node("${build_arch} && docker") {
            git(poll: false, changelog: false, url: git_url)

            stage("[${build_arch}--build-builder]") {
                sh("""./docker/build-builder --verbose ${builder_args}""")
            }
            stage("[${build_arch}--build-pb]") {
                sh("""./docker/build-pb --verbose --check ${pb_args}""")
            }
            stage('Post-build') {
                String result_file = "${BUILD_TAG}-${build_arch}-test-results.tar.xz"
                String test_info = """build_arch=${build_arch}
    BUILD_URL=${BUILD_URL}
    BUILD_TAG=${BUILD_TAG}
    GIT_URL=${GIT_URL}
    """

                writeFile(file: 'test-info.txt', text: test_info)
                sh("tar -cJf ${result_file} test-info.txt test-suite.log \
                    \$(find test -name '*.log')")
                archiveArtifacts  "${result_file}"
            }
        }
    }
}

def build_map = [:]
build_map.failFast = false

for (build_arch in params.BUILD_ARCH_LIST.split()) {
    build_map[build_arch] = build_pb.curry(build_arch, params.DRY_RUN,
        params.GIT_URL, params.DOCKER_PURGE)
}

parallel build_map
