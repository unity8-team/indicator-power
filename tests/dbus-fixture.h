/*
 * Copyright © 2015 Canonical Ltd.
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
 *   Ted Gould <ted.gould@canonical.com>
 */

#ifndef DBUS_FIXTURE_H
#define DBUS_FIXTURE_H

#include <gtest/gtest.h>

#include <gio/gio.h>

#include "glib-fixture.h"

struct DBusFixture : public GlibFixture
{
    typedef GlibFixture super;

protected:

    virtual void SetUp() override
    {
        BeforeBusSetUp();

        super::SetUp();

        m_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        g_dbus_connection_set_exit_on_close(m_bus, FALSE);
        g_object_add_weak_pointer(G_OBJECT(m_bus), (gpointer*)&m_bus);
    }

    virtual void TearDown() override
    {
        super::TearDown();

        BeforeBusTearDown();

        g_object_unref(m_bus);

        unsigned int cleartry = 0;
        while (m_bus != nullptr && cleartry < 100)
        {
            g_usleep(G_USEC_PER_SEC/10);
            while (g_main_pending())
            {
                g_main_iteration(TRUE);
            }
            cleartry++;
        }

        ASSERT_LT(cleartry, 100);
    }

    virtual void BeforeBusSetUp() {}
    virtual void BeforeBusTearDown() {}

    void wait_for_bus_name(GBusType bus_type,
                           const char* bus_name)
    {
        auto on_name_appeared = [](GDBusConnection*, const char*, const char*, gpointer gloop) {
            g_main_loop_quit(static_cast<GMainLoop*>(gloop));
        };
        auto watch_name_tag = g_bus_watch_name(bus_type,
                                               bus_name,
                                               G_BUS_NAME_WATCHER_FLAGS_NONE,
                                               on_name_appeared,
                                               nullptr,
                                               m_loop,
                                               nullptr);
        g_main_loop_run(m_loop);
        g_bus_unwatch_name(watch_name_tag);
    }

    GDBusConnection* m_bus = nullptr;
};

#endif // DBUS_FIXTURE_H
