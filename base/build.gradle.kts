plugins {
    java
    `maven-publish`
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