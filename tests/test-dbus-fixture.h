/*
 * Copyright 2013-2016 Canonical Ltd.
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

#pragma once

#include "glib-fixture.h"

/***
****
***/

class TestDBusFixture: public GlibFixture
{
  public:

    TestDBusFixture() =default;
    virtual ~TestDBusFixture() =default;

    explicit TestDBusFixture(const std::vector<std::string>& service_dirs_in):
        service_dirs(service_dirs_in)
    {
    }

  private:

    typedef GlibFixture super;

    static void
    on_bus_opened (GObject* /*object*/, GAsyncResult * res, gpointer gself)
    {
      auto self = static_cast<TestDBusFixture*>(gself);

      GError* err {};
      self->m_bus = g_bus_get_finish (res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->m_loop);
    }

    static void
    on_bus_closed (GObject* /*object*/, GAsyncResult * res, gpointer gself)
    {
      auto self = static_cast<TestDBusFixture*>(gself);

      GError* err {};
      g_dbus_connection_close_finish (self->m_bus, res, &err);
      g_assert_no_error (err);

      g_main_loop_quit (self->m_loop);
    }

  protected:

    GTestDBus * m_test_dbus {};
    GDBusConnection * m_bus {};
    const std::vector<std::string> service_dirs;

    virtual void SetUp() override
    {
      super::SetUp ();

      // pull up a test dbus
      m_test_dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
      for (const auto& dir : service_dirs)
        g_test_dbus_add_service_dir (m_test_dbus, dir.c_str());
      g_test_dbus_up (m_test_dbus);
      const char * address = g_test_dbus_get_bus_address (m_test_dbus);
      g_setenv ("DBUS_SYSTEM_BUS_ADDRESS", address, true);
      g_setenv ("DBUS_SESSION_BUS_ADDRESS", address, true);
      g_debug ("test_dbus's address is %s", address);

      // wait for the GDBusConnection before returning
      g_bus_get (G_BUS_TYPE_SYSTEM, nullptr, on_bus_opened, this);
      g_main_loop_run (m_loop);
    }

    virtual void TearDown() override
    {
      wait_msec();

      // close the bus
      g_dbus_connection_close(m_bus, nullptr, on_bus_closed, this);
      g_main_loop_run(m_loop);
      g_clear_object(&m_bus);

      // tear down the test dbus
      g_test_dbus_down(m_test_dbus);
      g_clear_object(&m_test_dbus);

      super::TearDown();
    }
};
