package xendroid.compose.updater

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.google.gson.annotations.SerializedName
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import retrofit2.http.GET
import xendroid.compose.BuildConfig
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor


data class GithubRelease(
    @SerializedName("name")
    val name: String? = null,

    @SerializedName("tag_name")
    val tagName: String,

    @SerializedName("body")
    val changelog: String? = null,

    @SerializedName("html_url")
    val releaseUrl: String
)

sealed class UpdateResult {
    data class Available(
        val release: GithubRelease
    ) : UpdateResult()

    data class Latest(
        val commitHash: String
    ) : UpdateResult()

     data class Cooldown(
        val remainingMillis: Long
    ) : UpdateResult()
}

interface GithubApi {

    @GET("repos/rfandango/XenDroid/releases/latest")
    suspend fun latestRelease(): GithubRelease
}

private const val PREFS_NAME = "updater"
private const val KEY_LAST_CHECK = "last_update_check"
private const val UPDATE_INTERVAL = 5 * 60 * 1000L

fun getRemainingCooldown(context: Context): Long {
    val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    val lastCheck = prefs.getLong(KEY_LAST_CHECK, 0L)
    val now = System.currentTimeMillis()

    return (UPDATE_INTERVAL - (now - lastCheck)).coerceAtLeast(0L)
}

fun shouldCheckForUpdates(context: Context): Boolean {
    val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    val lastCheck = prefs.getLong(KEY_LAST_CHECK, 0L)
    val now = System.currentTimeMillis()

    return now - lastCheck >= UPDATE_INTERVAL
}

fun saveLastCheck(context: Context) {
    context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        .edit()
        .putLong(KEY_LAST_CHECK, System.currentTimeMillis())
        .apply()
}

private val retrofit = Retrofit.Builder()
    .baseUrl("https://api.github.com/")
    .client(
        OkHttpClient.Builder()
            .addInterceptor(
                HttpLoggingInterceptor().apply {
                    level = HttpLoggingInterceptor.Level.BODY
                }
            )
            .build()
    )
    .addConverterFactory(GsonConverterFactory.create())
    .build()


private val api = retrofit.create(GithubApi::class.java)


suspend fun checkForUpdates(): UpdateResult {

    if (BuildConfig.APPLICATION_ID.endsWith(".fork")) {
        return UpdateResult.Latest(BuildConfig.VERSION_NAME)
    }

    Log.d("Updater", "Starting update check")
 
    val release = api.latestRelease()

    Log.d("Updater", "Release tag: ${release.tagName}")

    val currentHash = BuildConfig.VERSION_NAME

    Log.d("Updater", "Current: $currentHash")

    val latestHash = release.tagName
        .removePrefix("XenDroid-")
        .trim()

    Log.d("Updater", "Latest: $latestHash")

    return if (currentHash != latestHash) {
        UpdateResult.Available(release)
    } else {
        UpdateResult.Latest(currentHash)
    }
}

fun cleanChangelog(text: String): String {
    return text
        .replace(Regex("(?s)\\*\\*Full Changelog\\*\\*:.*"), "")
        .replace(Regex("https?://\\S+"), "")
        .replace("## What's Changed", "")
        .replace(Regex("\\* "), "• ")
        .replace(Regex("\n{3,}"), "\n\n")
        .trim()
}

@Composable
fun UpdateDialog(
    release: GithubRelease,
    onDismiss: () -> Unit
) {
    val context = LocalContext.current

    AlertDialog(
        onDismissRequest = onDismiss,

        title = {
            Text("Update available")
        },

        text = {
           Column(
            modifier = Modifier
            .heightIn(max = 400.dp)
            .verticalScroll(rememberScrollState())
            ) {
                Text(
                    "A new update for XenDroid is available."
                )

                Spacer(modifier = Modifier.height(16.dp))

                Text(
                    "Version: ${release.tagName}"
                )

                Spacer(modifier = Modifier.height(16.dp))

                Text(
                    "What changed:"
                )

                Spacer(modifier = Modifier.height(8.dp))

               Text(
                cleanChangelog(
                    release.changelog ?: "No changelog available."
                )
            )

                Spacer(modifier = Modifier.height(16.dp))

                Text(
                    "Do you want to download now?"
                )

                Spacer(modifier = Modifier.height(16.dp))

                Text(
                    "If not the updater will ask again in 5 minutes (avoids API rate limit issues)."
                )
            }
        },

        confirmButton = {
            TextButton(
                onClick = {
                    val intent = Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse(release.releaseUrl)
                    )

                    context.startActivity(intent)
                    onDismiss()
                }
            ) {
                Text("Yes")
            }
        },

        dismissButton = {
            TextButton(
                onClick = onDismiss
            ) {
                Text("No")
            }
        }
    )
}

@Composable
fun LatestVersionDialog(
    commitHash: String,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,

        title = {
            Text("No updates available")
        },

        text = {
            Text(
                "You're on latest version: $commitHash"
            )
        },

        confirmButton = {
            TextButton(
                onClick = onDismiss
            ) {
                Text("OK")
            }
        }
    )
}

@Composable
fun CooldownDialog(
    remainingMillis: Long,
    onDismiss: () -> Unit
) {
    val minutes = remainingMillis / 1000 / 60
    val seconds = (remainingMillis / 1000) % 60

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Text("Updater cooldown")
        },
        text = {
            Text(
                "Updater is in cooldown.\n\n" +
                "You can check again in ${minutes}m ${seconds}s."
            )
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("OK")
            }
        }
    )
}