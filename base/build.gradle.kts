plugins {
    java
    `maven-publish`
}

dependencies {
    compileOnly("org.jetbrains:annotations:23.0.0")
}

java {
    withSourcesJar()
}

publishing {
    publications {
        create("base", MavenPublication::class) {
            artifactId = "compat"

            from(components["java"])
        }
    }
}