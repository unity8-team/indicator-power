# coding: UTF-8
'''Wrapper around DBusMock's notify-notifications template'''

# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option) any
# later version.  See http://www.gnu.org/copyleft/lgpl.html for the full text
# of the license.

__author__ = 'Charles Kerr'
__email__ = 'charles.kerr@canonical.com'
__copyright__ = '(c) 2015 Canonical Ltd.'
__license__ = 'LGPL 3+'

import argparse

import dbus.service

import dbusmock.mockobject
import dbusmock.testcase

def parse_args():
    parser = argparse.ArgumentParser(description='Notifications wrapper')
    parser.add_argument('-8', '--unity8', action='store_true',
                        help='emulate Unity 8 notifications')
    parser.add_argument('-7', '--unity7', action='store_true',
                        help='emulate Unity 7 notifications')

    args = parser.parse_args()

    if args.unity7 and args.unity8:
        parser.error('specifying --unity7 and --unity8 are mutually exclusive')

    return args


if __name__ == '__main__':
    import dbus.mainloop.glib
    from gi.repository import GLib

    args = parse_args()
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

    params = {}
    if args.unity7:
        params['capabilities'] = ' '.join([
            'append',
            'body',
            'body-markup',
            'icon-static',
            'image/svg+xml',
            'private-icon-only',
            'private-synchronous',
            'truncation',
            'x-canonical-append',
            'x-canonical-private-icon-only',
            'x-canonical-private-synchronous',
            'x-canonical-truncation'])
    elif args.unity8:
        params['capabilities'] = ' '.join([
            'body',
            'body-markup',
            'icon-static',
            'image/svg+xml',
            'sound-file',
            'suppress-sound',
            'urgency',
            'value',
            'x-canonical-non-shaped-icon',      
            'x-canonical-private-affirmative-tint',      
            'x-canonical-private-icon-only',      
            'x-canonical-private-menu-model',      
            'x-canonical-private-rejection-tint',      
            'x-canonical-private-synchronous',      
            'x-canonical-secondary-icon',      
            'x-canonical-snap-decisions',      
            'x-canonical-snap-decisions-swipe',
            'x-canonical-snap-decisions-timeout',     
            'x-canonical-switch-to-application',     
            'x-canonical-truncation',
            'x-canonical-value-bar-tint'])

    args.template = 'notification_daemon'
    module = dbusmock.mockobject.load_module(args.template)
    args.name = module.BUS_NAME
    args.path = module.MAIN_OBJ
    args.system = module.SYSTEM_BUS
    args.interface = module.MAIN_IFACE

    main_loop = GLib.MainLoop()
    bus = dbusmock.testcase.DBusTestCase.get_dbus(args.system)

    # quit mock when the bus is going down
    bus.add_signal_receiver(main_loop.quit,
                            signal_name='Disconnected',
                            path='/org/freedesktop/DBus/Local',
                            dbus_interface='org.freedesktop.DBus.Local')

    bus_name = dbus.service.BusName(args.name,
                                    bus,
                                    allow_replacement=True,
                                    replace_existing=True,
                                    do_not_queue=True)

    main_object = dbusmock.mockobject.DBusMockObject(bus_name, args.path,
                                                     args.interface, {},
                                                     None,
                                                     False)

    main_object.AddTemplate(args.template, params)

    dbusmock.mockobject.objects[args.path] = main_object
    main_loop.run()
