# SPDX-License-Identifier: BSD-3-Clause
# mrv2
# Copyright Contributors to the mrv2 Project. All rights reserved.

#
# For this example, we will just use the timeline module.
#
import mrv2
from mrv2 import timeline

#
# Hello Plugin class.  Must derive from mrv2.plugin.Plugin
#
class HelloPlugin(mrv2.plugin.Plugin):
    def __init__(self):
        """
        Constructor.  Init your variables here.
        """
        super().__init__()
        pass
    
    def active(self):
        """
        Status of the plug-in.  Must be True of False.
        Change this to False to not activate the plug-in.
        If active is missing from the class, it is assumed to be True.
        """
        return True

    def on_open_file(self, filename):
        """
        Callback called when a file is opened.
        """
        print(f'Opened "{filename}".')

        
    def run(self):
        """
        The actual plugin execution for Python/Hello.
        """
        print("Hello from python plugin!")
        
    def menus(self):
        """
        Return the menus to add to mrv2's main toolbar.
        You can add your commands to mrv2 menus or create a new
        menu entry. Each Submenu is separated by '/'.
        Currently, you cannot remove a menu entry.

        Returns:
        dict: a dictionary of key for menu entries and values as methods.
              The value can be a tuple to add options like a divider for a menu
              entry.
        """
        menus = {
            "Python/Hello" : self.run,
        }
        return menus
        

#
# Playback Plugin class. Must derive from mrv2.plugin.Plugin
#
class PlaybackPlugin(mrv2.plugin.Plugin):
    """
    Constructor.  Init your variables here.
    """
    def __init__(self):
        super().__init__()
        pass

    """
    Status of the plug-in.  Must be True of False.
    Change this to False to not activate the plug-in.
    If active is missing from the class, it is assumed to be True.
    """
    def active(self):
        return True

    """
    The actual plugin execution for Python/Playback/Forwards.
    """
    def play(self):
        timeline.playForwards()
        
    """
    Return the menus to add to mrv2's main toolbar.
    You can add your commands to mrv2 menus or create a new
    menu entry. Each Submenu is separated by '/'.
    Currently, you cannot remove a menu entry.

    Returns:
    dict: a dictionary of key for menu entries and values as methods.
          The value can be a tuple to add options like a divider for a menu
          entry.
    """
    def menus(self):
        menus = {
            # Call a method and place a divider line after the menu
            "Python/Play/Forwards" : (self.play, '__divider__'), 
            "Python/Play/Backwards" : timeline.playBackwards # call a function
        }
        return menus
        
