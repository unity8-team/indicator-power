/*
 * Copyright 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 */


#include "dbus-fixture.h"

#include "dbus-shared.h"
#include "device.h"
#include "notifier.h"

#include <gtest/gtest.h>

#include <libnotify/notify.h>

#include <glib.h>
#include <gio/gio.h>

/***
****
***/

class NotifyFixture: public DBusFixture
{
    typedef DBusFixture super;

protected:

    GTestDBus* m_test_bus {};

    static constexpr char const * APP_NAME {"indicator-power-service"};

    void BeforeBusSetUp() override
    {
        // use a fake bus
        m_test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(m_test_bus);

        // start the notifications service
        const gchar* child_argv[] = { "python3", TEST_SCRIPTS_DIR "/start-mock-notifications.py", "--unity8", nullptr };
        GError* error = nullptr;
        g_spawn_async(nullptr, (gchar**)child_argv, nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &error);
        g_assert_no_error(error);
    }

    void SetUp() override
    {
        super::SetUp();

        wait_for_bus_name(G_BUS_TYPE_SESSION, "org.freedesktop.Notifications");
        notify_init(APP_NAME);
    }

    void TearDown() override
    {
        notify_uninit();

        DBusFixture::TearDown();

        g_clear_object(&m_test_bus);
    }
};

/***
****
***/

// simple test to confirm the NotifyFixture plumbing all works
TEST_F(NotifyFixture, HelloWorld)
{
}

/***
****
***/

namespace
{
  static constexpr double percent_critical {2.0};
  static constexpr double percent_very_low {5.0};
  static constexpr double percent_low {10.0};

  void set_battery_percentage (IndicatorPowerDevice * battery, gdouble p)
  {
    g_object_set (battery, INDICATOR_POWER_DEVICE_PERCENTAGE, p, nullptr);
  }
}

TEST_F(NotifyFixture, PercentageToLevel)
{
  auto battery = indicator_power_device_new ("/object/path",
                                             UP_DEVICE_KIND_BATTERY,
                                             50.0,
                                             UP_DEVICE_STATE_DISCHARGING,
                                             30);

  // confirm that the power levels trigger at the right percentages
  for (int i=100; i>=0; --i)
    {
      set_battery_percentage (battery, i);
      const auto level = indicator_power_notifier_get_power_level(battery);

       if (i <= percent_critical)
         EXPECT_STREQ (POWER_LEVEL_STR_CRITICAL, level);
       else if (i <= percent_very_low)
         EXPECT_STREQ (POWER_LEVEL_STR_VERY_LOW, level);
       else if (i <= percent_low)
         EXPECT_STREQ (POWER_LEVEL_STR_LOW, level);
       else
         EXPECT_STREQ (POWER_LEVEL_STR_OK, level);
     }

  g_object_unref (battery);
}

/***
****
***/

// scaffolding to monitor PropertyChanged signals
namespace
{
  enum
  {
    FIELD_POWER_LEVEL = (1<<0),
    FIELD_IS_WARNING  = (1<<1)
  };

  struct ChangedParams
  {
    std::string power_level = POWER_LEVEL_STR_OK;
    bool is_warning = false;
    uint32_t fields = 0;
  };

  void on_battery_property_changed (GDBusConnection *connection G_GNUC_UNUSED,
                                    const gchar *sender_name G_GNUC_UNUSED,
                                    const gchar *object_path G_GNUC_UNUSED,
                                    const gchar *interface_name G_GNUC_UNUSED,
                                    const gchar *signal_name G_GNUC_UNUSED,
                                    GVariant *parameters,
                                    gpointer gchanged_params)
  {
    g_return_if_fail (g_variant_n_children (parameters) == 3);
    auto dict = g_variant_get_child_value (parameters, 1);
    g_return_if_fail (g_variant_is_of_type (dict, G_VARIANT_TYPE_DICTIONARY));
    auto changed_params = static_cast<ChangedParams*>(gchanged_params);

    const char * power_level;
    if (g_variant_lookup (dict, "PowerLevel", "&s", &power_level, nullptr))
    {
      changed_params->power_level = power_level;
      changed_params->fields |= FIELD_POWER_LEVEL;
    }

    gboolean is_warning;
    if (g_variant_lookup (dict, "IsWarning", "b", &is_warning, nullptr))
    {
      changed_params->is_warning = is_warning;
      changed_params->fields |= FIELD_IS_WARNING;
    }

    g_variant_unref (dict);
  }
}

TEST_F(NotifyFixture, LevelsDuringBatteryDrain)
{
  auto battery = indicator_power_device_new ("/object/path",
                                             UP_DEVICE_KIND_BATTERY,
                                             50.0,
                                             UP_DEVICE_STATE_DISCHARGING,
                                             30);

  // set up a notifier and give it the battery so changing the battery's
  // charge should show up on the bus.
  auto notifier = indicator_power_notifier_new ();
  indicator_power_notifier_set_battery (notifier, battery);
  indicator_power_notifier_set_bus (notifier, m_bus);
  wait_msec();

  ChangedParams changed_params;
  auto sub_tag = g_dbus_connection_signal_subscribe (m_bus,
                                                     nullptr,
                                                     "org.freedesktop.DBus.Properties",
                                                     "PropertiesChanged",
                                                     BUS_PATH"/Battery",
                                                     nullptr,
                                                     G_DBUS_SIGNAL_FLAGS_NONE,
                                                     on_battery_property_changed,
                                                     &changed_params,
                                                     nullptr);

  // confirm that draining the battery puts
  // the power_level change through its paces                                                              
  for (int i=100; i>=0; --i)
    {
      changed_params = ChangedParams();
      EXPECT_TRUE (changed_params.fields == 0);

      const auto old_level = indicator_power_notifier_get_power_level(battery);
      set_battery_percentage (battery, i);
      const auto new_level = indicator_power_notifier_get_power_level(battery);
      wait_msec();

      if (old_level == new_level)
        {
          EXPECT_EQ (0, (changed_params.fields & FIELD_POWER_LEVEL));
        }
      else
        {
          EXPECT_EQ (FIELD_POWER_LEVEL, (changed_params.fields & FIELD_POWER_LEVEL));
          EXPECT_EQ (new_level, changed_params.power_level);
        }
    }

  // cleanup
  g_dbus_connection_signal_unsubscribe (m_bus, sub_tag);
  g_object_unref (notifier);
  g_object_unref (battery);
}

/***
****
***/

TEST_F(NotifyFixture, EventsThatChangeNotifications)
{
  auto battery = indicator_power_device_new ("/object/path",
                                             UP_DEVICE_KIND_BATTERY,
                                             percent_low + 1.0,
                                             UP_DEVICE_STATE_DISCHARGING,
                                             30);

  // set up a notifier and give it the battery so changing the battery's
  // charge should show up on the bus.
  auto notifier = indicator_power_notifier_new ();
  indicator_power_notifier_set_battery (notifier, battery);
  indicator_power_notifier_set_bus (notifier, m_bus);
  ChangedParams changed_params;
  auto sub_tag = g_dbus_connection_signal_subscribe (m_bus,
                                                     nullptr,
                                                     "org.freedesktop.DBus.Properties",
                                                     "PropertiesChanged",
                                                     BUS_PATH"/Battery",
                                                     nullptr,
                                                     G_DBUS_SIGNAL_FLAGS_NONE,
                                                     on_battery_property_changed,
                                                     &changed_params,
                                                     nullptr);

  // test setup case
  wait_msec();
  EXPECT_STREQ (POWER_LEVEL_STR_OK, changed_params.power_level.c_str());

  // change the percent past the 'low' threshold and confirm that
  // a) the power level changes
  // b) we get a notification
  changed_params = ChangedParams();
  set_battery_percentage (battery, percent_low);
  wait_msec();
  EXPECT_EQ (FIELD_POWER_LEVEL|FIELD_IS_WARNING, changed_params.fields);
  EXPECT_EQ (indicator_power_notifier_get_power_level(battery), changed_params.power_level);
  EXPECT_TRUE (changed_params.is_warning);

  // now test that the warning changes if the level goes down even lower...
  changed_params = ChangedParams();
  set_battery_percentage (battery, percent_very_low);
  wait_msec();
  EXPECT_EQ (FIELD_POWER_LEVEL, changed_params.fields);
  EXPECT_STREQ (POWER_LEVEL_STR_VERY_LOW, changed_params.power_level.c_str());

  // ...and that the warning is taken down if the battery is plugged back in...
  changed_params = ChangedParams();
  g_object_set (battery, INDICATOR_POWER_DEVICE_STATE, UP_DEVICE_STATE_CHARGING, nullptr);
  wait_msec();
  EXPECT_EQ (FIELD_IS_WARNING, changed_params.fields);
  EXPECT_FALSE (changed_params.is_warning);

  // ...and that it comes back if we unplug again...
  changed_params = ChangedParams();
  g_object_set (battery, INDICATOR_POWER_DEVICE_STATE, UP_DEVICE_STATE_DISCHARGING, nullptr);
  wait_msec();
  EXPECT_EQ (FIELD_IS_WARNING, changed_params.fields);
  EXPECT_TRUE (changed_params.is_warning);

  // ...and that it's taken down if the power level is OK
  changed_params = ChangedParams();
  set_battery_percentage (battery, percent_low+1);
  wait_msec();
  EXPECT_EQ (FIELD_POWER_LEVEL|FIELD_IS_WARNING, changed_params.fields);
  EXPECT_STREQ (POWER_LEVEL_STR_OK, changed_params.power_level.c_str());
  EXPECT_FALSE (changed_params.is_warning);

  // cleanup
  g_dbus_connection_signal_unsubscribe (m_bus, sub_tag);
  g_object_unref (notifier);
  g_object_unref (battery);
}
