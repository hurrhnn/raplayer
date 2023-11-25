void Clean() {
    dir("submodules") {
        deleteDir()
        writeFile file:'dummy', text:'' // Creates the directory
    }
    dir("release") {
        deleteDir()
        writeFile file:'dummy', text:'' // Creates the directory
    }
}

void Build() {
    checkout scm
    dir("release") {
        sh 'cmake .. -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release'
        sh 'make'
    }
}

void Test() {
    dir("release") {
        sh './application/main'
    }
}

pipeline {
    agent none
    stages {
        stage('Initialization') {
            parallel {
                stage('Linux') {
                    agent {
                        label 'Linux'
                    }
                    stages {
                        stage('Clean on Linux') {
                            steps {
                                Clean()
                            }
                        }
                        stage('Build on Linux') {
                            steps {
                                Build()
                            }
                        }
                        stage('Test & Release on Linux') {
                            steps {
                                Test()
                                dir("release") {
                                    sh 'mv application/main raplayer-linux-x86_64'
                                    archiveArtifacts artifacts: 'raplayer-linux-x86_64', fingerprint: true
                                }
                            }
                        }
                    }
                }

                stage('macOS') {
                    agent {
                        label 'macOS'
                    }
                    stages {
                        stage('Clean on macOS') {
                            steps {
                                Clean()
                            }
                        }
                        stage('Build on macOS') {
                            steps {
                                Build()
                            }
                        }
                        stage('Test & Release on macOS') {
                            steps {
                                Test()
                                dir("release") {
                                    sh 'mv application/main raplayer-mac-x86_64'
                                    archiveArtifacts artifacts: 'raplayer-mac-x86_64', fingerprint: true
                                }
                            }
                        }
                    }
                }

                stage('Windows') {
                    agent {
                        label 'Windows'
                    }
                    stages {
                        stage('Clean on Windows') {
                            steps {
                                Clean()
                            }
                        }
                        stage('Build on Windows') {
                            steps {
                                dir("release") {
                                    bat 'cmake .. -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release'
                                    bat 'make'
                                }
                            }
                        }
                        stage('Test & Release on Windows') {
                            steps {
                                dir("release") {
                                    bat 'application\\main.exe'
                                    bat 'move application\\main.exe raplayer-win-x86_64.exe'
                                    bat 'copy C:\\cygwin64\\bin\\cygwin1.dll cygwin1.dll'
                                    script { zip zipFile: 'raplayer-win-x86_64.zip', archive: false, glob: 'cygwin1.dll, raplayer-win-x86_64.exe' }
                                    archiveArtifacts artifacts: 'raplayer-win-x86_64.zip', fingerprint: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
