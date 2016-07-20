node ('mesos') {
  try {
    stage 'checkout code'
    checkout scm

    stage 'build and push docker image'
    sh 'docker build -t x0 .'

    stage 'build debian package'
    sh 'dpkg-buildpackage -uc -us'
  } catch (e) {
    error 'Error'
  }
}
