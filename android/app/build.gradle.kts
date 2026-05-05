import java.time.LocalDate
import java.time.format.DateTimeFormatter

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.kotlin.serialization)
}

fun gitRevisionCountOrDefault(): Int {
    return try {
        val process = ProcessBuilder("git", "rev-list", "--count", "HEAD")
            .directory(rootDir)
            .redirectErrorStream(true)
            .start()
        val output = process.inputStream.bufferedReader().use { it.readText().trim() }
        if (process.waitFor() == 0) output.toIntOrNull() ?: 1 else 1
    } catch (_: Exception) {
        1
    }
}

val computedVersionCode = gitRevisionCountOrDefault()
val computedVersionName = "V1.0." + computedVersionCode + "." + LocalDate.now().format(DateTimeFormatter.ofPattern("yyMMdd"))

android {
    namespace = "cn.easydarwin.easyrtc"
    compileSdk = 36

    // ✅ 添加签名配置 打包命令
    // release 版本   yarn build:release
    // debug 版本     yarn build:debug
    signingConfigs {
        create("release") {
            storeFile = file("../jks/rtc_yb.jks")
            storePassword = "123456"  // 替换为实际的密钥库密码
            keyAlias = "rtc_yb"  // 替换为实际的密钥别名
            keyPassword = "123456"  // 替换为实际的密钥密码
        }
    }

    defaultConfig {
        applicationId = "cn.easydarwin.easyrtc"
        minSdk = 28
        targetSdk = 36
        versionCode = computedVersionCode
        versionName = computedVersionName

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    applicationVariants.all {
        val variant = this
        variant.outputs.map { it as com.android.build.gradle.internal.api.BaseVariantOutputImpl }.forEach { output ->
            val outputFileName = "EasyRTC_${versionName}.apk"
            output.outputFileName = outputFileName
        }
    }

    // ✅ 添加自定义输出目录配置
    applicationVariants.all {
        val variant = this
        variant.assembleProvider?.configure {
            doLast {
                variant.outputs.forEach { output ->
                    val outputFile = output.outputFile
                    if (outputFile != null && outputFile.exists()) {
                        val targetDir = File(project.rootDir, "release")
                        if (!targetDir.exists()) {
                            targetDir.mkdirs()
                        }
                        val targetFile = File(targetDir, outputFile.name)
                        outputFile.copyTo(targetFile, overwrite = true)
                        println("APK 已复制到: ${targetFile.absolutePath}")
                    }
                }
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // ✅ 应用签名配置到 release 构建类型
            signingConfig = signingConfigs.getByName("release")
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro"
            )
        }

        // ✅ 可选：也为 debug 构建类型配置签名（方便测试）
        debug {
            signingConfig = signingConfigs.getByName("release")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        buildConfig = true
        compose = true
    }
}

dependencies {
    implementation(project(":easyrtc"))
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    implementation(libs.androidx.camera.core)
    implementation(libs.core)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.media3.exoplayer)
    implementation(libs.androidx.graphics.core)
    implementation(libs.androidx.core.ktx.v1120)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.exoplayer)
    implementation(libs.androidx.recyclerview)
    implementation(libs.androidx.preference.ktx)
    implementation(libs.kotlinx.serialization.json)
    implementation(libs.material)
    implementation(libs.material.icons);
    implementation(libs.androidx.swiperefreshlayout)
    implementation(libs.tencent.bugly)

    // Retrofit
    implementation(libs.retrofit2.retrofit)
    implementation(libs.retrofit2.converter.gson)
    implementation(libs.okhttp3.logging.interceptor)

    // Coroutines
    implementation(libs.kotlinx.coroutines.android)

    testImplementation(libs.junit)
    testImplementation("androidx.arch.core:core-testing:2.2.0")
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation("androidx.test:runner:1.5.2")
    androidTestImplementation("androidx.test:rules:1.5.0")
    androidTestImplementation("androidx.test.ext:junit:1.1.5")
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.ui.test.junit4)
    debugImplementation(libs.androidx.ui.tooling)
    debugImplementation(libs.androidx.ui.test.manifest)


}
