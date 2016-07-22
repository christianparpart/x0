node ('mesos') {
  stage 'checkout code'
  checkout scm

  stage 'build and push docker image'
  sh 'docker build -t x0 .'

  // stage 'build debian package'
  // sh 'apt-get install -y debhelper automake autoconf libfcgi-dev'
  // sh 'dpkg-buildpackage -uc -us'
}
