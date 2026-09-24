package xendroid.compose.driver

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

data class DriverInfo(
    val name: String,
    val version: String,
    val url: String,
    val sha256: String = ""
)

object DriverRepository {

    private const val RELEASES_API =
        "https://api.github.com/repos/K11MCH1/AdrenoToolsDrivers/releases"

    suspend fun loadDrivers(): List<DriverInfo> = withContext(Dispatchers.IO) {
        val connection = URL(RELEASES_API).openConnection() as HttpURLConnection

        connection.connectTimeout = 10000
        connection.readTimeout = 15000
        connection.requestMethod = "GET"
        connection.setRequestProperty("Accept", "application/vnd.github+json")
        connection.setRequestProperty("User-Agent", "XenDroid")

        try {
            if (connection.responseCode != HttpURLConnection.HTTP_OK) {
                throw Exception("GitHub API HTTP ${connection.responseCode}")
            }

            val json = connection.inputStream.bufferedReader().use { it.readText() }
            val releases = JSONArray(json)

            buildList {
                for (i in 0 until releases.length()) {
                    val release = releases.getJSONObject(i)

                    if (release.optBoolean("draft", false) ||
                        release.optBoolean("prerelease", false)
                    ) {
                        continue
                    }

                    val releaseName = release.optString("name").ifBlank {
                        release.optString("tag_name")
                    }

                    val tagName = release.optString("tag_name")
                    val assets = release.optJSONArray("assets") ?: continue

                    for (j in 0 until assets.length()) {
                        val asset = assets.getJSONObject(j)

                        val assetName = asset.optString("name")
                        val downloadUrl = asset.optString("browser_download_url")

                        if (!assetName.endsWith(".zip", ignoreCase = true)) {
                            continue
                        }

                        if (downloadUrl.isBlank()) {
                            continue
                        }

                        add(
                            DriverInfo(
                                name = assetName,
                                version = "$releaseName ($tagName)",
                                url = downloadUrl
                            )
                        )
                    }
                }
            }
        } finally {
            connection.disconnect()
        }
    }

    suspend fun downloadDriver(
        context: Context,
        driver: DriverInfo,
        onProgress: (Int) -> Unit
    ): File = withContext(Dispatchers.IO) {

        val directory = File(context.cacheDir, "driver_downloads")

        if (!directory.exists() && !directory.mkdirs()) {
            throw Exception("Unable to create download directory")
        }

        val safeName = driver.name.replace(
            Regex("[^A-Za-z0-9._-]"),
            "_"
        )

        val file = File(directory, safeName)

        val connection = URL(driver.url).openConnection() as HttpURLConnection

        connection.connectTimeout = 15000
        connection.readTimeout = 30000
        connection.requestMethod = "GET"
        connection.setRequestProperty("User-Agent", "XenDroid")

        try {
            if (connection.responseCode != HttpURLConnection.HTTP_OK) {
                throw Exception("Download HTTP ${connection.responseCode}")
            }

            val total = connection.contentLengthLong

            connection.inputStream.use { input ->
                file.outputStream().use { output ->
                    val buffer = ByteArray(16384)
                    var downloaded = 0L

                    while (true) {
                        val count = input.read(buffer)

                        if (count == -1) {
                            break
                        }

                        output.write(buffer, 0, count)
                        downloaded += count

                        if (total > 0) {
                            onProgress(
                                ((downloaded * 100L) / total).toInt()
                            )
                        }
                    }
                }
            }

            if (driver.sha256.isNotBlank()) {
                val actualHash = sha256(file)

                if (!actualHash.equals(driver.sha256, ignoreCase = true)) {
                    file.delete()
                    throw Exception("SHA-256 verification failed")
                }
            }

            file
        } finally {
            connection.disconnect()
        }
    }

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")

        file.inputStream().use { input ->
            val buffer = ByteArray(16384)

            while (true) {
                val count = input.read(buffer)

                if (count == -1) {
                    break
                }

                digest.update(buffer, 0, count)
            }
        }

        return digest.digest().joinToString("") {
            "%02x".format(it)
        }
    }
}