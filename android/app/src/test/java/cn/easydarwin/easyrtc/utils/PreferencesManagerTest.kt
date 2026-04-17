package cn.easydarwin.easyrtc.utils

import android.content.SharedPreferences
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class PreferencesManagerTest {

    @Test
    fun clearLoginInfo_clears_all_login_related_fields() {
        val sharedPreferences = FakeSharedPreferences()
        val manager = PreferencesManagerTestAccess.create(sharedPreferences)

        manager.saveLoginStatus(
            isLoggedIn = true,
            username = "alice",
            token = "token-1",
            serverIp = "192.168.1.10",
            serverPort = "8443"
        )

        manager.clearLoginInfo()
        val info = manager.getLoginInfo()

        assertFalse(info.isLoggedIn)
        assertEquals("", info.username)
        assertEquals("", info.token)
        assertEquals("", info.serverIp)
        assertEquals("", info.serverPort)
        assertEquals(0L, info.loginTime)
    }

    @Test
    fun saveLogiinStatus_delegates_to_saveLoginStatus() {
        val sharedPreferences = FakeSharedPreferences()
        val manager = PreferencesManagerTestAccess.create(sharedPreferences)

        manager.saveLogiinStatus(
            isLoggedIn = true,
            username = "bob",
            token = "token-2",
            serverIp = "10.0.0.5",
            serverPort = "443"
        )

        val info = manager.getLoginInfo()
        assertEquals(true, info.isLoggedIn)
        assertEquals("bob", info.username)
        assertEquals("token-2", info.token)
        assertEquals("10.0.0.5", info.serverIp)
        assertEquals("443", info.serverPort)
    }
}

private object PreferencesManagerTestAccess {
    fun create(sharedPreferences: SharedPreferences): PreferencesManager {
        val constructor = PreferencesManager::class.java.getDeclaredConstructor(SharedPreferences::class.java)
        constructor.isAccessible = true
        return constructor.newInstance(sharedPreferences)
    }
}

private class FakeSharedPreferences : SharedPreferences {
    private val values = mutableMapOf<String, Any?>()

    override fun contains(key: String?): Boolean = key != null && values.containsKey(key)

    override fun getBoolean(key: String?, defValue: Boolean): Boolean {
        val value = values[key]
        return if (value is Boolean) value else defValue
    }

    override fun getFloat(key: String?, defValue: Float): Float {
        val value = values[key]
        return if (value is Float) value else defValue
    }

    override fun getInt(key: String?, defValue: Int): Int {
        val value = values[key]
        return if (value is Int) value else defValue
    }

    override fun getLong(key: String?, defValue: Long): Long {
        val value = values[key]
        return if (value is Long) value else defValue
    }

    override fun getString(key: String?, defValue: String?): String? {
        val value = values[key]
        return if (value is String?) value else defValue
    }

    @Suppress("UNCHECKED_CAST")
    override fun getStringSet(key: String?, defValues: MutableSet<String>?): MutableSet<String>? {
        val value = values[key]
        return if (value is MutableSet<*>) value as MutableSet<String> else defValues
    }

    override fun getAll(): MutableMap<String, *> = values.toMutableMap()

    override fun edit(): SharedPreferences.Editor = Editor(values)

    override fun registerOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {}

    override fun unregisterOnSharedPreferenceChangeListener(listener: SharedPreferences.OnSharedPreferenceChangeListener?) {}

    private class Editor(
        private val target: MutableMap<String, Any?>
    ) : SharedPreferences.Editor {
        private val pending = mutableMapOf<String, Any?>()
        private val removals = mutableSetOf<String>()
        private var clearAll = false

        override fun putBoolean(key: String?, value: Boolean): SharedPreferences.Editor {
            if (key != null) pending[key] = value
            return this
        }

        override fun putFloat(key: String?, value: Float): SharedPreferences.Editor {
            if (key != null) pending[key] = value
            return this
        }

        override fun putInt(key: String?, value: Int): SharedPreferences.Editor {
            if (key != null) pending[key] = value
            return this
        }

        override fun putLong(key: String?, value: Long): SharedPreferences.Editor {
            if (key != null) pending[key] = value
            return this
        }

        override fun putString(key: String?, value: String?): SharedPreferences.Editor {
            if (key != null) pending[key] = value
            return this
        }

        override fun putStringSet(key: String?, values: MutableSet<String>?): SharedPreferences.Editor {
            if (key != null) pending[key] = values
            return this
        }

        override fun remove(key: String?): SharedPreferences.Editor {
            if (key != null) removals.add(key)
            return this
        }

        override fun clear(): SharedPreferences.Editor {
            clearAll = true
            return this
        }

        override fun commit(): Boolean {
            apply()
            return true
        }

        override fun apply() {
            if (clearAll) {
                target.clear()
            }
            removals.forEach(target::remove)
            target.putAll(pending)
        }
    }
}
