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
        sh 'cmake .. -DCMAKE_BUILD_TYPE=Release'
        sh 'make'
    }
}

void Test() {
    dir("release") {
        sh './raplayer'
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
                        stage('Test on Linux') {
                            steps {
                                Test()
                                dir("release") {
                                    sh 'mv raplayer raplayer-linux-x86_64'
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
                        stage('Test on macOS') {
                            steps {
                                Test()
                                dir("release") {
                                    sh 'mv raplayer raplayer-mac-x86_64'
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
                                    bat 'cmake .. -DCMAKE_BUILD_TYPE=Release'
                                    bat 'make'
                                }
                            }
                        }
                        stage('Test on Windows') {
                            steps {
                                dir("release") {
                                    bat 'raplayer.exe'
                                    bat 'mv raplayer.exe raplayer-win-x86_64.exe'
                                    archiveArtifacts artifacts: 'raplayer-win-x86_64.exe', fingerprint: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    post {
        always {
            discordSend description: "```\nresult: " + currentBuild.currentResult + "\n```", link: env.BUILD_URL, result: currentBuild.currentResult, title: JOB_NAME + " #" + env.BUILD_NUMBER, webhookURL: "https://discord.com/api/webhooks/876066631284035605/ocEMWjZmT9eFOFN_7zenbiqIRzFNrk921APCkfCw-yIMUaJLTP4wVt6qMtXNhFPfOroi"
        }
    }
}
