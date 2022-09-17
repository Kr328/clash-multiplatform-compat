val releaseTag = Runtime.getRuntime()
    .exec("git describe --exact-match --tags HEAD")
    .inputStream.readBytes().toString(Charsets.UTF_8).trim()
    .takeIf { it.isNotEmpty() } ?: "master"

subprojects {
    group = "com.github.kr328.clash.compat"
    version = releaseTag

    plugins.withId("java") {
        configure<JavaPluginExtension> {
            sourceCompatibility = JavaVersion.VERSION_1_8
            targetCompatibility = JavaVersion.VERSION_1_8
        }
    }

    plugins.withId("maven-publish") {
        extensions.configure<PublishingExtension> {
            publications {
                withType(MavenPublication::class) {
                    version = project.version.toString()
                    group = project.group.toString()

                    pom {
                        name.set("Clash Multiplatform Compat")
                        description.set("Clash multiplatform compat helpers")
                        url.set("https://github.com/Kr328/clash-multiplatform-compat")
                        licenses {
                            license {
                                name.set("MIT License")
                                url.set("https://github.com/Kr328/clash-multiplatform-compat/blob/main/LICENSE")
                            }
                        }
                        developers {
                            developer {
                                name.set("Kr328")
                            }
                        }
                        scm {
                            scm {
                                connection.set("scm:git:https://github.com/Kr328/clash-multiplatform-compat.git")
                                url.set("https://github.com/Kr328/clash-multiplatform-compat")
                            }
                        }
                    }
                }
                repositories {
                    mavenLocal()
                    maven {
                        name = "kr328app"
                        url = uri("https://maven.kr328.app/releases")
                        credentials(PasswordCredentials::class)
                    }
                }
            }
        }
    }
}

task("clean", type = Delete::class) {
    group = "build"

    delete(buildDir)
}