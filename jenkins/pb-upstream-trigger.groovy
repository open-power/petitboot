#!groovy
// Check for upstream updates and run builds.

properties([
    buildDiscarder(logRotator(daysToKeepStr: '30', numToKeepStr: '5')),
    pipelineTriggers([pollSCM('H/30 * * * *')]),
    parameters([
    string(name: 'GIT_URL',
        defaultValue: 'git://ozlabs.org/petitboot',
        description: 'URL of petitboot git repository.'),
    ])
])

stage('Build') {
    node {
        git(poll: true, changelog: false, url: params.GIT_URL)
        build(
            job: 'pb-build-matrix',
            parameters: [
                string(name: 'GIT_URL', value: params.GIT_URL),
            ],
        )
    }
}
