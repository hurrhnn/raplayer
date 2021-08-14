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
        sh '`which cmake` .. -DCMAKE_BUILD_TYPE=Release'
        sh '`which make`'
    }
}

void Test() {
    dir("release") {
        sh './raplayer --gtest_output=xml:project-tests-results.xml'
    }
    post {
        always {
            junit '*-tests-results.xml'
        }
    }
}

pipeline {
    agent none
    options {
        skipDefaultCheckout(true)   // to avoid force checkouts on every node in a first stage
        disableConcurrentBuilds()   // to avoid concurrent builds on same nodes
    }
    stages {
        stage ('Clean') {
            steps {
                node('Linux') {
                    Clean()
                }
                node('MacOS') {
                    Clean()
                }
            }
        }

        stage ('Build') {
            steps {
                node('Linux') {
                    Build()
                }
                node('MacOS') {
                    Build()
                }
            }
        }

        stage ('Test') {
            steps {
                node('Linux') {
                    Test()
                }
                node('MacOS') {
                    Test()
                }
            }
        }
    }
}
