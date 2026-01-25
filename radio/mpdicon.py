#!/usr/bin/env python
# MPD control notification icon + show song name in tooltip

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import GLib
from gi.repository import Gtk as gtk
import subprocess
#import gobject
from gi.repository import GObject as gobject
#import time

def bash_command(cmd):
    subprocess.Popen(['/bin/bash', '-c', cmd])

#track = subprocess.check_output("mpc 2>&1 | grep -", shell=True)
#track = track.rstrip('\n')

class StatusIcon:
    def __init__(self, trk):
        self.statusicon = gtk.StatusIcon()
        self.statusicon.set_from_file("/usr/share/icons/gnome/22x22/apps/play.png")
        self.statusicon.connect("activate", self.left_click_event) #activate defines left-click action
        self.statusicon.connect("popup-menu", self.right_click_event) #popup-menu defines right-click action
        self.statusicon.connect("scroll_event", self.icon_scroll)
        self.statusicon.connect('button-press-event', self.middleclick)
    def middleclick(self, statusicon, event):
        if event.button == 2:
            bash_command('mpc next')
    def icon_scroll(self, param, data=None): #control next/previous with scroll wheel
        if data.direction == gtk.gdk.SCROLL_UP:
            bash_command('mpc volume +5')
        elif data.direction == gtk.gdk.SCROLL_DOWN:
            bash_command('mpc volume -5')
    def right_click_event(self, icon, button, time):
        menu = gtk.Menu()
        nexts = gtk.MenuItem("Next")
        prevs = gtk.MenuItem("Prev")
        nexts.connect("activate", self.nexts)
        prevs.connect("activate", self.prevs)
        menu.append(nexts)
        menu.append(prevs)
        menu.show_all()
        menu.popup(None, None, gtk.status_icon_position_menu, button, time, self.statusicon)
    def left_click_event(self, event):
        bash_command('if [[ `mpc | grep playing` ]]; then mpc pause; else mpc play; fi')
    def nexts(self, event):
        bash_command('mpc next')
    def prevs(self, event):
        bash_command('mpc prev')
    def update_track(self): #Updates tooltip with new track
#        track = subprocess.check_output("echo $(mpc 2>&1 | grep - ; echo \|;mpc volume | cut -d \" \" -f 2; echo Vol)", shell=True)
        track ="hallo\n"
#        track = track.rstrip('\n')
        self.statusicon.set_has_tooltip(track)

        return True
if __name__ == '__main__': #Main loop
        track ="hallo\n"
#	track = subprocess.check_output("mpc 2>&1 | grep -", shell=True)
#	track = track.rstrip('\n')
        trayicon = StatusIcon(track)
        GLib.timeout_add(1500, trayicon.update_track)
        trayicon.update_track()
        gtk.main()
