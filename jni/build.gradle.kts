plugins {
    java
    `maven-publish`
}

val currentOSName = System.getProperty("os.name").toLowerCase()
val os = when {
    currentOSName.contains("windows") -> "windows"
    currentOSName.contains("linux") -> "linux"
    else -> throw UnsupportedOperationException("Unsupported os: $currentOSName")
}

val currentOSArch = System.getProperty("os.arch").toLowerCase()
val archWith64 = when {
    currentOSArch.contains("amd64") -> "amd64" to true
    currentOSArch.contains("x86_64") -> "amd64" to true
    currentOSArch.contains("x64") -> "amd64" to true
    currentOSArch.contains("x86") -> "i386" to false
    currentOSArch.contains("386") -> "i386" to false
    currentOSArch.contains("686") -> "i386" to false
    else -> throw UnsupportedOperationException("Unsupported arch: $currentOSArch")
}
val (arch, is64) = archWith64

val cmakeDir = buildDir.resolve("cmake")

val prepare = task("prepareCompileJniLibs", type = Exec::class) {
    inputs.dir(file("src/main/cpp"))
    outputs.dir(cmakeDir)

    workingDir(cmakeDir)
    commandLine(
        "cmake",
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DJAVA_HOME=${System.getProperty("java.home").replace(File.separatorChar, '/')}",
        file("src/main/cpp")
    )
    if (!is64) {
        environment(
            "CFLAGS" to "${System.getenv("CFLAGS")} -m32",
            "LDFLAGS" to "${System.getenv("LDFLAGS")} -m32",
        )
    }
}

val compile = task("compileJniLibs", type = Exec::class) {
    dependsOn(prepare)

    inputs.dir(file("src/main/cpp"))
    outputs.dir(cmakeDir)

    workingDir(cmakeDir)
    commandLine("cmake", "--build", ".")
}

val copy = task("copyJniLibs", type = Sync::class) {
    from(compile.outputs)

    include("*.dll", "*.so")

    rename {
        val baseName = it.substring(0, it.indexOf('.'))
        val extension = it.substring(it.indexOf('.') + 1)

        "$baseName-$arch.$extension"
    }

    into(buildDir.resolve("jniLibs").resolve("com/github/kr328/clash/compat"))
}

sourceSets {
    named("main") {
        resources.srcDir(buildDir.resolve("jniLibs"))
    }
}

publishing {
    publications {
        create("jniLibs", MavenPublication::class) {
            artifactId = "compat-$os-$arch"

            from(components["java"])
        }
    }
}

afterEvaluate {
    tasks["processResources"].dependsOn(copy)
}