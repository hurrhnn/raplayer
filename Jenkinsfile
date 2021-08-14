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
    post {
        always {
            discordSend description: "```\nresult: " + currentBuild.currentResult + "\n```", link: env.BUILD_URL, result: currentBuild.currentResult, title: JOB_NAME + " #" + env.BUILD_NUMBER, webhookURL: "https://discord.com/api/webhooks/876066631284035605/ocEMWjZmT9eFOFN_7zenbiqIRzFNrk921APCkfCw-yIMUaJLTP4wVt6qMtXNhFPfOroi"
            junit '*-tests-results.xml'
        }
    }
}
